#include "ScrollView.hpp"

#include "ui/widget/foundation/animation/SpringMotion.hpp"
#include "ui/widget/foundation/layout/LayoutTypes.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kIndicatorChannel = 0x5343524Cu;

float clampedOffset(const ScrollState &state) noexcept {
  return std::clamp(state.offset, 0.0f, std::max(state.maximum, 0.0f));
}

void applyDrag(ScrollState &state, float pointerDelta,
               float resistance) noexcept {
  const float movement = -pointerDelta;
  const float maximum = std::max(state.maximum, 0.0f);
  if (movement == 0.0f)
    return;

  if (state.offset < 0.0f) {
    if (movement < 0.0f) {
      state.offset += movement * resistance;
      return;
    }
    const float returning = std::min(movement, -state.offset);
    state.offset += returning;
    const float remainder = movement - returning;
    if (remainder > 0.0f) {
      if (maximum <= 0.0f)
        state.offset = remainder * resistance;
      else if (remainder <= maximum)
        state.offset = remainder;
      else
        state.offset = maximum + (remainder - maximum) * resistance;
    }
    return;
  }

  if (state.offset > maximum) {
    if (movement > 0.0f) {
      state.offset += movement * resistance;
      return;
    }
    const float returning = std::max(movement, maximum - state.offset);
    state.offset += returning;
    const float remainder = movement - returning;
    if (remainder < 0.0f) {
      const float proposed = maximum + remainder;
      state.offset = proposed >= 0.0f ? proposed : proposed * resistance;
    }
    return;
  }

  const float proposed = state.offset + movement;
  if (proposed < 0.0f)
    state.offset = proposed * resistance;
  else if (proposed > maximum)
    state.offset = maximum + (proposed - maximum) * resistance;
  else
    state.offset = proposed;
}

void updateGesture(Context &context, ScrollState &state, ImGuiID id,
                   const ImVec2 &minimum, const ImVec2 &maximum,
                   const ScrollViewOptions &options, float density) noexcept {
  if (state.releaseSuppressionNextFrame) {
    state.suppressTap = false;
    state.releaseSuppressionNextFrame = false;
  }
  const float resistance =
      std::clamp(options.overscrollResistance, 0.05f, 1.0f);
  const VerticalDragState drag = context.gestures().verticalDrag(
      id, minimum, maximum,
      VerticalDragParameters{options.touchSlop * density,
                             options.directionRatio});

  if (drag.tracking && state.gesture == ScrollGestureState::Idle) {
    state.gesture = ScrollGestureState::Possible;
    state.gestureDelta = 0.0f;
    state.suppressTap =
        std::abs(state.velocity) >= options.minimumVelocity * density ||
        std::abs(state.offset - clampedOffset(state)) > 0.5f;
    state.velocity = 0.0f;
  }
  if (drag.active) {
    const float pointerDelta = drag.deltaY - state.gestureDelta;
    state.gestureDelta = drag.deltaY;
    state.gesture = ScrollGestureState::Dragging;
    state.suppressTap = true;
    state.velocity = -drag.velocityY;
    applyDrag(state, pointerDelta, resistance);
  }
  if (drag.released || drag.cancelled) {
    if (drag.cancelled)
      state.velocity = 0.0f;
    else
      state.velocity = -drag.velocityY;
    state.gesture = ScrollGestureState::Idle;
    state.gestureDelta = 0.0f;
    state.releaseSuppressionNextFrame = state.suppressTap;
  }
}

void updateMotion(ScrollState &state, const FrameInfo &frame,
                  const ScrollViewOptions &options,
                  float viewportHeight) noexcept {
  if (state.gesture == ScrollGestureState::Dragging)
    return;
  const float deltaTime = frame.deltaTime;
  const float boundary = clampedOffset(state);
  if (std::abs(state.offset - boundary) > 0.01f) {
    SpringMotion motion{state.offset, state.velocity};
    stepSpring(motion, boundary,
               SpringOptions{0.34f, 0.82f, 0.08f * frame.density},
               deltaTime, frame.reduceMotion);
    state.offset = motion.value;
    state.velocity = motion.velocity;
  } else if (!frame.reduceMotion &&
             std::abs(state.velocity) >=
                 options.minimumVelocity * frame.density) {
    state.offset += state.velocity * deltaTime;
    state.velocity *=
        std::exp(-std::max(options.deceleration, 0.0f) * deltaTime);
  } else {
    state.offset = boundary;
    state.velocity = 0.0f;
  }
  const float overscrollLimit = std::max(viewportHeight * 0.28f, 1.0f);
  state.offset = std::clamp(state.offset, -overscrollLimit,
                            state.maximum + overscrollLimit);
}

void translateOverscrollVertices(const ScrollScope &scope) noexcept {
  if (std::abs(scope.visualOverscroll) <= 0.01f)
    return;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const int begin = std::clamp(scope.vertexStart, 0, drawList->VtxBuffer.Size);
  for (int index = begin; index < drawList->VtxBuffer.Size; ++index)
    drawList->VtxBuffer[index].pos.y -= scope.visualOverscroll;
}

void drawIndicator(Context &context, ImGuiID id, const ScrollScope &scope) {
  const ScrollState &state = *scope.state;
  const FrameInfo &frame = context.frame();
  const float viewportHeight = scope.maximum.y - scope.minimum.y;
  const bool moving = state.gesture == ScrollGestureState::Dragging ||
                      std::abs(state.velocity) >
                          scope.indicatorMinimumVelocity * frame.density;
  const float target = state.maximum > 0.5f && moving ? 1.0f : 0.0f;
  const float opacity = context.animations().animate(
      id, kIndicatorChannel, target, target > 0.0f ? 0.06f : 0.28f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  if (opacity <= 0.001f || state.maximum <= 0.5f)
    return;

  const float density = frame.density;
  const float inset = std::max(scope.indicatorInset * density, 0.0f);
  const float width = std::max(scope.indicatorWidth * density, 1.0f);
  const float trackHeight = std::max(viewportHeight - 2.0f * inset, 1.0f);
  const float contentHeight = viewportHeight + state.maximum;
  const float thumbHeight = std::clamp(
      trackHeight * viewportHeight / std::max(contentHeight, 1.0f),
      scope.indicatorMinimumLength * density, trackHeight);
  const float progress =
      state.maximum > 0.0f ? clampedOffset(state) / state.maximum : 0.0f;
  const float y = scope.minimum.y + inset +
                  (trackHeight - thumbHeight) * progress;
  ImVec4 color = scope.indicatorColor;
  color.w *= opacity;
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(scope.maximum.x - inset - width, y),
      ImVec2(scope.maximum.x - inset, y + thumbHeight),
      ImGui::GetColorU32(color), width * 0.5f);
}

}

ScrollViewResult BeginScrollView(Context &context, const char *id,
                                 const ScrollViewOptions &options) {
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 size(resolveLayoutExtent(options.size.x, available.x, density),
                    resolveLayoutExtent(options.size.y, available.y, density));
  const ImVec2 minimum = ImGui::GetCursorScreenPos();
  const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
  const ImGuiID scrollId = ImGui::GetID(id ? id : "##scroll-view");
  ScrollState &state =
      context.scrollStates().acquire(scrollId, frame.frameNumber);
  updateGesture(context, state, scrollId, minimum, maximum, options, density);
  updateMotion(state, frame, options, size.y);

  const float scroll = clampedOffset(state);
  ImGui::SetNextWindowScroll(ImVec2(-1.0f, scroll));
  const bool visible = ImGui::BeginChild(
      id ? id : "##scroll-view", size, options.childFlags,
      options.windowFlags | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_NoScrollWithMouse);
  context.drawTransforms().attach(ImGui::GetWindowDrawList());
  state.maximum = std::max(ImGui::GetScrollMaxY(), 0.0f);
  const float overscroll = state.offset - clampedOffset(state);
  context.scrollStates().pushScope(
      ScrollScope{&state, minimum, maximum,
                  ImGui::GetWindowDrawList()->VtxBuffer.Size,
                  overscroll * std::clamp(options.overscrollResistance, 0.05f,
                                          1.0f),
                  options.indicatorWidth, options.indicatorMinimumLength,
                  options.indicatorInset, options.minimumVelocity,
                  options.indicatorColor});
  context.gestures().pushTapScope(
      state.gesture == ScrollGestureState::Dragging || state.suppressTap);
  return ScrollViewResult{visible,
                          state.gesture == ScrollGestureState::Dragging,
                          state.offset, state.maximum};
}

void EndScrollView(Context &context) {
  context.gestures().popTapScope();
  ScrollScope *scope = context.scrollStates().currentScope();
  if (scope && scope->state) {
    scope->state->maximum = std::max(ImGui::GetScrollMaxY(), 0.0f);
    translateOverscrollVertices(*scope);
    drawIndicator(context, ImGui::GetID("##scroll-indicator"), *scope);
  }
  context.scrollStates().popScope();
  ImGui::EndChild();
}

}
