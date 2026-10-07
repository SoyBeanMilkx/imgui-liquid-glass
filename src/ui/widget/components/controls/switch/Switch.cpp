#include "Switch.hpp"

#include "ui/widget/foundation/interaction/ItemBehavior.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kValueAnimationChannel = 20;
constexpr uint32_t kPressAnimationChannel = 21;

ImVec4 mix(const ImVec4 &from, const ImVec4 &to, float amount) noexcept {
  const float value = std::clamp(amount, 0.0f, 1.0f);
  return ImVec4(
      from.x + (to.x - from.x) * value, from.y + (to.y - from.y) * value,
      from.z + (to.z - from.z) * value, from.w + (to.w - from.w) * value);
}

ImVec4 withAlpha(ImVec4 color, float multiplier) noexcept {
  color.w *= multiplier;
  return color;
}

void addCapsuleGlow(ImDrawList *drawList, const ImVec2 &minimum,
                    const ImVec2 &maximum, const ImVec4 &color, float spread,
                    float opacity) {
  if (!drawList || spread <= 0.0f || opacity <= 0.0f)
    return;

  constexpr int kArcPointCount = 24;
  constexpr int kPointCount = kArcPointCount * 2;
  constexpr int kRingCount = 4;
  constexpr float kPi = 3.14159265358979323846f;
  // Shared ring boundaries create a continuous Gaussian-like falloff.
  constexpr std::array<float, kRingCount> kRadiusStops = {0.0f, 0.28f, 0.62f,
                                                          1.0f};
  constexpr std::array<float, kRingCount> kAlphaStops = {1.0f, 0.56f, 0.16f,
                                                         0.0f};

  const float width = maximum.x - minimum.x;
  const float height = maximum.y - minimum.y;
  const float radius = std::max(0.0f, std::min(width, height) * 0.5f);
  if (radius <= 0.0f)
    return;

  const float centerY = (minimum.y + maximum.y) * 0.5f;
  const ImVec2 leftCenter(minimum.x + radius, centerY);
  const ImVec2 rightCenter(maximum.x - radius, centerY);
  const ImVec2 whitePixel = ImGui::GetFontTexUvWhitePixel();

  drawList->PrimReserve((kRingCount - 1) * kPointCount * 6,
                        kRingCount * kPointCount);
  const ImDrawIdx baseIndex = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
  for (int ring = 0; ring < kRingCount; ++ring) {
    const float ringRadius = radius + spread * kRadiusStops[ring];
    const ImU32 ringColor = ImGui::GetColorU32(
        withAlpha(color, std::clamp(opacity * kAlphaStops[ring], 0.0f, 1.0f)));
    for (int index = 0; index < kPointCount; ++index) {
      const bool rightArc = index < kArcPointCount;
      const int arcIndex = rightArc ? index : index - kArcPointCount;
      const float amount =
          static_cast<float>(arcIndex) / static_cast<float>(kArcPointCount - 1);
      const float startAngle = rightArc ? -kPi * 0.5f : kPi * 0.5f;
      const float angle = startAngle + kPi * amount;
      const ImVec2 &center = rightArc ? rightCenter : leftCenter;
      drawList->PrimWriteVtx(ImVec2(center.x + std::cos(angle) * ringRadius,
                                    center.y + std::sin(angle) * ringRadius),
                             whitePixel, ringColor);
    }
  }

  for (int ring = 0; ring < kRingCount - 1; ++ring) {
    for (int index = 0; index < kPointCount; ++index) {
      const int next = (index + 1) % kPointCount;
      const ImDrawIdx inner =
          static_cast<ImDrawIdx>(baseIndex + ring * kPointCount + index);
      const ImDrawIdx outer = static_cast<ImDrawIdx>(inner + kPointCount);
      const ImDrawIdx nextInner =
          static_cast<ImDrawIdx>(baseIndex + ring * kPointCount + next);
      const ImDrawIdx nextOuter =
          static_cast<ImDrawIdx>(nextInner + kPointCount);
      drawList->PrimWriteIdx(inner);
      drawList->PrimWriteIdx(outer);
      drawList->PrimWriteIdx(nextOuter);
      drawList->PrimWriteIdx(inner);
      drawList->PrimWriteIdx(nextOuter);
      drawList->PrimWriteIdx(nextInner);
    }
  }
}

}
bool Switch(Context &context, const char *id, bool *value,
            const SwitchOptions &options) {
  if (!id || !value)
    return false;

  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const float visualWidth =
      (options.size.x > 0.0f ? options.size.x : theme.switchWidth) * density;
  const float visualHeight =
      (options.size.y > 0.0f ? options.size.y : theme.switchHeight) * density;
  const float minimumTouch =
      (options.minimumTouchSize >= 0.0f ? options.minimumTouchSize
                                        : theme.minimumTouchSize) *
      density;
  const ImVec2 hitSize(std::max(visualWidth, minimumTouch),
                       std::max(visualHeight, minimumTouch));
  const ItemBehaviorState item =
      buttonBehavior(context, id, hitSize, options.enabled);
  if (item.pressed)
    *value = !*value;

  const float progress = context.animations().animate(
      item.id, kValueAnimationChannel, *value ? 1.0f : 0.0f,
      theme.switchAnimationResponse, frame.deltaTime, frame.frameNumber,
      frame.reduceMotion);
  const float press = context.animations().animate(
      item.id, kPressAnimationChannel, item.active ? 1.0f : 0.0f,
      theme.switchAnimationResponse * 0.6f, frame.deltaTime, frame.frameNumber,
      frame.reduceMotion);

  const ImVec2 visualMinimum(item.minimum.x + (hitSize.x - visualWidth) * 0.5f,
                             item.minimum.y +
                                 (hitSize.y - visualHeight) * 0.5f);
  const ImVec2 visualMaximum(visualMinimum.x + visualWidth,
                             visualMinimum.y + visualHeight);
  ImVec4 offColor = options.trackOffColor.value_or(theme.switchTrackOff);
  ImVec4 onColor = options.trackOnColor.value_or(theme.switchTrackOn);
  ImVec4 thumbColor = options.thumbColor.value_or(theme.switchThumb);
  ImVec4 trackColor = mix(offColor, onColor, progress);
  if (!options.enabled) {
    trackColor.w *= 0.42f;
    thumbColor.w *= 0.55f;
  }

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  if (options.enabled && progress > 0.001f) {
    const ImVec4 glow = options.glowColor.value_or(theme.switchGlow);
    const float glowAmount = progress * (1.0f - 0.25f * press);
    const float glowSpread =
        (options.glowSpread >= 0.0f ? options.glowSpread
                                    : theme.switchGlowSpread) *
        density;
    const float glowStrength = options.glowStrength >= 0.0f
                                   ? options.glowStrength
                                   : theme.switchGlowStrength;
    addCapsuleGlow(drawList, visualMinimum, visualMaximum, glow, glowSpread,
                   glowAmount * glowStrength);
  }

  drawList->AddRectFilled(visualMinimum, visualMaximum,
                          ImGui::GetColorU32(trackColor), visualHeight * 0.5f);

  const float margin = 2.5f * density;
  const float thumbRadius = std::max(1.0f, visualHeight * 0.5f - margin);
  const float startX = visualMinimum.x + margin + thumbRadius;
  const float endX = visualMaximum.x - margin - thumbRadius;
  const ImVec2 center(startX + (endX - startX) * progress,
                      (visualMinimum.y + visualMaximum.y) * 0.5f);
  const float pressedRadius = thumbRadius * (1.0f - 0.07f * press);
  const ImVec2 shadowCenter(center.x, center.y + 1.0f * density);
  drawList->AddCircleFilled(
      shadowCenter, pressedRadius + 0.75f * density,
      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.18f)));
  drawList->AddCircleFilled(center, pressedRadius,
                            ImGui::GetColorU32(thumbColor));
  return item.pressed;
}

}
