#include "Slider.hpp"

#include "ui/widget/foundation/interaction/ItemBehavior.hpp"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kValueChannel = 30;
constexpr uint32_t kMorphChannel = 31;
constexpr uint32_t kJellyChannel = 32;
constexpr uint32_t kTrackScaleChannel = 33;
constexpr int kDragPriority = 1;

float quantize(float value, float minimum, float maximum,
               float step) noexcept {
  const float clamped = std::clamp(value, minimum, maximum);
  if (step <= 0.0f)
    return clamped;
  const float snapped =
      minimum + std::round((clamped - minimum) / step) * step;
  return std::clamp(snapped, minimum, maximum);
}

float progressOf(float value, float minimum, float maximum) noexcept {
  const float extent = maximum - minimum;
  return extent > 0.0f
             ? std::clamp((value - minimum) / extent, 0.0f, 1.0f)
             : 0.0f;
}

float smoothStep(float value) noexcept {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}

float trackFillEnd(float center, float minimum, float maximum,
                   float thumbWidth, float padding) noexcept {
  const float width = std::max(maximum - minimum, 0.0f);
  const float position = center - minimum;
  const float margin = thumbWidth * 0.5f;
  const float transition = thumbWidth * 0.25f;
  const float leading = margin + transition;
  const float trailing = width - margin - transition;
  if (position <= margin)
    return minimum + padding;
  if (position >= width - margin)
    return maximum - padding;
  if (position < leading) {
    const float ratio = (position - margin) / std::max(transition, 1.0f);
    return minimum + padding +
           smoothStep(ratio) * (leading - padding);
  }
  if (position > trailing) {
    const float ratio = (position - trailing) / std::max(transition, 1.0f);
    return minimum + trailing +
           smoothStep(ratio) * (width - padding - trailing);
  }
  return center;
}

bool canUseGlass(const Context &context) noexcept {
  const EffectCapabilities &capabilities = context.effectCapabilities();
  return capabilities.backdropCapture &&
         context.glassRequests().requests().size() <
             capabilities.maxGlassRegions;
}

void submitGlassThumb(Context &context, ImGuiID id, const ImVec2 &minimum,
                      const ImVec2 &maximum, const ImVec4 &clipRect,
                      const GlassStyle &style, ImVec2 deformation,
                      float blurMix, float opacity,
                      ImDrawList *drawList, int drawCommandOffset) {
  GlassRequest request;
  request.id = id;
  request.min = minimum;
  request.max = maximum;
  request.radii = CornerRadii::all((maximum.y - minimum.y) * 0.5f /
                                   context.frame().density);
  request.shape = GlassShapeKind::Capsule;
  request.style = style;
  request.clipRect = clipRect;
  request.deformation = deformation;
  request.drawList = drawList;
  request.drawCommandOffset = drawCommandOffset;
  request.backdropSource = GlassBackdropSource::PreviousContent;
  request.density = context.frame().density;
  request.opacity = std::clamp(opacity, 0.0f, 1.0f);
  request.blurMix = std::clamp(blurMix, 0.0f, 1.0f);
  context.glassRequests().submit(request);
}

void glassAnchor(const ImDrawList *, const ImDrawCmd *) {}

ImVec4 alpha(ImVec4 color, float opacity) noexcept {
  color.w *= std::clamp(opacity, 0.0f, 1.0f);
  return color;
}

struct SliderInteraction {
  bool dragging = false;
  bool active = false;
  float velocity = 0.0f;
  float overshoot = 0.0f;
};

struct SliderVisual {
  float progress;
  float morph;
  float jelly;
  float trackScale;
};

SliderInteraction updateValue(Context &context, const ItemBehaviorState &item,
                              float *value, float minimum, float maximum,
                              float startX, float endX,
                              const SliderOptions &options) {
  const FrameInfo &frame = context.frame();
  HorizontalDragState drag;
  if (options.enabled) {
    drag = context.gestures().horizontalDrag(
        item.id, item.minimum, item.maximum,
        HorizontalDragParameters{options.touchSlop * frame.density,
                                 options.directionRatio, kDragPriority});
  }
  const bool mouseFallback = !context.gestures().hasPointerInput();
  const bool mouseDragging =
      mouseFallback && item.active && ImGui::IsMouseDown(0);
  SliderInteraction interaction;
  interaction.dragging = drag.active || drag.released || mouseDragging;
  interaction.active = drag.tracking || drag.active || mouseDragging;
  const bool tapped = item.pressed && !drag.consumeTap;
  const ImVec2 pointer = context.gestures().hasPointerInput()
                             ? drag.position
                             : ImGui::GetIO().MousePos;
  if ((interaction.dragging || tapped) && pointer.x > -FLT_MAX &&
      pointer.x < FLT_MAX) {
    const float progress =
        endX > startX
            ? std::clamp((pointer.x - startX) / (endX - startX), 0.0f, 1.0f)
            : 0.0f;
    *value = quantize(minimum + (maximum - minimum) * progress,
                     minimum, maximum, options.step);
  }
  interaction.velocity = drag.active || drag.released ? drag.velocityX : 0.0f;
  if (mouseDragging && frame.deltaTime > 0.0f)
    interaction.velocity = ImGui::GetIO().MouseDelta.x / frame.deltaTime;
  if (interaction.active)
    interaction.overshoot =
        std::max({startX - pointer.x, pointer.x - endX, 0.0f});
  return interaction;
}

SliderVisual updateVisual(Context &context, ImGuiID id, float targetProgress,
                          const SliderInteraction &interaction) {
  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  AnimationStore &animations = context.animations();
  const float progress = animations.animate(
      id, kValueChannel, targetProgress,
      interaction.dragging ? 0.0f : theme.sliderAnimationResponse,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const SpringTransition morph = animations.spring(
      id, kMorphChannel, interaction.active ? 1.0f : 0.0f,
      SpringOptions{theme.sliderMorphResponse, 0.58f, 0.002f},
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float jellyTarget =
      interaction.active
          ? std::clamp(interaction.velocity /
                           std::max(theme.sliderVelocityForFullEffect * frame.density,
                                    1.0f),
                       -1.0f, 1.0f)
          : 0.0f;
  const SpringTransition jelly = animations.spring(
      id, kJellyChannel, jellyTarget, SpringOptions{0.22f, 0.52f, 0.001f},
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float trackScaleTarget =
      1.0f - 0.30f * std::clamp(interaction.overshoot / (100.0f * frame.density),
                               0.0f, 1.0f);
  const SpringTransition track = animations.spring(
      id, kTrackScaleChannel, trackScaleTarget,
      SpringOptions{0.24f, 0.78f, 0.001f}, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);
  return SliderVisual{progress, std::clamp(morph.value, 0.0f, 1.12f),
                      std::clamp(jelly.value, -1.15f, 1.15f), track.value};
}

}

bool Slider(Context &context, const char *id, float *value, float minimum,
            float maximum, const SliderOptions &options) {
  if (!id || !value)
    return false;
  if (minimum > maximum)
    std::swap(minimum, maximum);
  if (!std::isfinite(*value))
    *value = minimum;
  const float previous = *value;
  *value = quantize(*value, minimum, maximum, options.step);

  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float width =
      options.size.x > 0.0f
          ? options.size.x * density
          : (options.size.x < 0.0f
                 ? std::max(available.x + options.size.x * density, 1.0f)
                 : std::max(available.x, 1.0f));
  const float height = std::max(
      (options.size.y > 0.0f ? options.size.y : theme.sliderHeight) * density,
      1.0f);
  const float minimumTouch =
      (options.minimumTouchSize >= 0.0f ? options.minimumTouchSize
                                        : theme.minimumTouchSize) *
      density;
  const ImVec2 hitSize(std::max(width, minimumTouch),
                       std::max(height, minimumTouch));
  const ItemBehaviorState item =
      buttonBehavior(context, id, hitSize, options.enabled);

  const ImVec2 thumbSize(
      (options.thumbSize.x > 0.0f ? options.thumbSize.x
                                  : theme.sliderThumbWidth) *
          density,
      (options.thumbSize.y > 0.0f ? options.thumbSize.y
                                  : theme.sliderThumbHeight) *
          density);
  const float visualMinimumX = item.minimum.x + (hitSize.x - width) * 0.5f;
  const float visualMaximumX = visualMinimumX + width;
  const float startX = visualMinimumX + thumbSize.x * 0.5f;
  const float endX = std::max(visualMaximumX - thumbSize.x * 0.5f, startX);
  const SliderInteraction interaction = updateValue(
      context, item, value, minimum, maximum, startX, endX, options);
  const SliderVisual visual = updateVisual(
      context, item.id, progressOf(*value, minimum, maximum), interaction);
  const float opacity = std::clamp(visual.morph, 0.0f, 1.0f);

  ImVec4 trackColor = options.trackColor.value_or(theme.sliderTrack);
  ImVec4 fillColor = options.fillColor.value_or(theme.sliderFill);
  ImVec4 thumbColor = options.thumbColor.value_or(theme.sliderThumb);
  if (!options.enabled) {
    trackColor.w *= 0.45f;
    fillColor.w *= 0.45f;
    thumbColor.w *= 0.55f;
  }

  const float centerY = (item.minimum.y + item.maximum.y) * 0.5f;
  const float baseTrackHeight =
      (options.trackHeight >= 0.0f ? options.trackHeight
                                  : theme.sliderTrackHeight) *
      density;
  const float trackHeight = std::clamp(
      baseTrackHeight * visual.trackScale, 1.0f, height);
  const float trackRadius = trackHeight * 0.5f;
  const float centerX = startX + (endX - startX) * visual.progress;
  const float trackPadding = std::min(2.0f * density, width * 0.5f);
  const ImVec2 trackMinimum(visualMinimumX + trackPadding,
                            centerY - trackRadius);
  const ImVec2 trackMaximum(visualMaximumX - trackPadding,
                            centerY + trackRadius);
  const float fillEnd = trackFillEnd(centerX, visualMinimumX,
                                     visualMaximumX, thumbSize.x,
                                     trackPadding);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(trackMinimum, trackMaximum,
                          ImGui::GetColorU32(trackColor), trackRadius);
  if (fillEnd > trackMinimum.x) {
    const ImDrawFlags fillFlags =
        fillEnd >= trackMaximum.x - 0.5f
            ? ImDrawFlags_RoundCornersAll
            : ImDrawFlags_RoundCornersLeft;
    drawList->AddRectFilled(trackMinimum,
                            ImVec2(fillEnd, trackMaximum.y),
                            ImGui::GetColorU32(fillColor), trackRadius,
                            fillFlags);
  }

  const float activeScale = options.activeScale > 0.0f
                                ? options.activeScale
                                : theme.sliderActiveScale;
  const float baseScale = 1.0f + (activeScale - 1.0f) * visual.morph;
  const float jellyAmount = visual.jelly * opacity;
  const ImVec2 activeThumbSize(
      thumbSize.x * baseScale * (1.0f + 0.25f * jellyAmount),
      thumbSize.y * baseScale * (1.0f - 0.18f * jellyAmount));
  const ImVec2 restMinimum(centerX - thumbSize.x * 0.5f,
                           centerY - thumbSize.y * 0.5f);
  const ImVec2 restMaximum(centerX + thumbSize.x * 0.5f,
                           centerY + thumbSize.y * 0.5f);
  const ImVec2 thumbMinimum(centerX - activeThumbSize.x * 0.5f,
                            centerY - activeThumbSize.y * 0.5f);
  const ImVec2 thumbMaximum(centerX + activeThumbSize.x * 0.5f,
                            centerY + activeThumbSize.y * 0.5f);
  const float radius = activeThumbSize.y * 0.5f;

  if (opacity > 0.001f) {
    const ImVec4 clipRect = drawList->_CmdHeader.ClipRect;
    const bool glass = options.glassOnInteraction && canUseGlass(context);
    if (glass) {
      drawList->AddCallback(glassAnchor, nullptr);
      const int glassCommandOffset = drawList->CmdBuffer.Size - 2;
      submitGlassThumb(context, item.id, thumbMinimum, thumbMaximum, clipRect,
                       options.glassStyle,
                       ImVec2(visual.jelly * 0.20f * opacity, 0.0f),
                       options.glassBlurMix, opacity, drawList,
                       glassCommandOffset);
    } else {
      ImVec4 fallback = options.glassStyle.tint;
      fallback.w = std::max(fallback.w, 0.32f) * opacity;
      drawList->AddRectFilled(thumbMinimum, thumbMaximum,
                              ImGui::GetColorU32(fallback), radius);
    }
  }

  if (opacity < 0.999f) {
    const float restOpacity = 1.0f - opacity;
    drawList->AddRectFilled(
        ImVec2(restMinimum.x, restMinimum.y + density),
        ImVec2(restMaximum.x, restMaximum.y + density),
        ImGui::GetColorU32(
            ImVec4(0.0f, 0.0f, 0.0f, 0.15f * restOpacity)),
        thumbSize.y * 0.5f);
    drawList->AddRectFilled(
        restMinimum, restMaximum,
        ImGui::GetColorU32(alpha(thumbColor, restOpacity)),
        thumbSize.y * 0.5f);
  }

  return std::abs(*value - previous) > 0.000001f;
}

}
