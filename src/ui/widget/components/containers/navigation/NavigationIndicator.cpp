#include "NavigationIndicator.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kDirectionChannel = 1;
constexpr uint32_t kMinimumEdgeChannel = 2;
constexpr uint32_t kMaximumEdgeChannel = 3;

}

void NavigationIndicator(Context &context, const char *id,
                         ImVec2 targetMinimum, ImVec2 targetMaximum,
                         const NavigationIndicatorOptions &options) {
  const FrameInfo &frame = context.frame();
  const ImGuiID itemId = ImGui::GetID(id);
  const bool vertical = options.axis == NavigationAxis::Vertical;
  const ImVec2 windowPosition = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  const float axisOrigin = vertical ? windowPosition.y : windowPosition.x;
  const float axisExtent =
      std::max(vertical ? windowSize.y : windowSize.x, 1.0f);
  // Store motion within the window; moving its parent is not a selection change.
  const float targetStart =
      ((vertical ? targetMinimum.y : targetMinimum.x) - axisOrigin) /
      axisExtent;
  const float targetEnd =
      ((vertical ? targetMaximum.y : targetMaximum.x) - axisOrigin) /
      axisExtent;
  const float targetCenter = (targetStart + targetEnd) * 0.5f;
  const float directionGuide = context.animations().animate(
      itemId, kDirectionChannel, targetCenter, 0.12f, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);
  const bool movingForward = targetCenter >= directionGuide;
  SpringOptions startSpring =
      movingForward ? options.trailingSpring : options.leadingSpring;
  SpringOptions endSpring =
      movingForward ? options.leadingSpring : options.trailingSpring;
  startSpring.precision /= axisExtent;
  endSpring.precision /= axisExtent;
  const SpringTransition start = context.animations().spring(
      itemId, kMinimumEdgeChannel, targetStart, startSpring, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);
  const SpringTransition end = context.animations().spring(
      itemId, kMaximumEdgeChannel, targetEnd, endSpring, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);

  const float axisMinimum =
      axisOrigin + std::min(start.value, end.value) * axisExtent;
  const float axisMaximum =
      axisOrigin + std::max(start.value, end.value) * axisExtent;
  const float baseLength =
      std::max((targetEnd - targetStart) * axisExtent, 1.0f);
  const float stretchedLength = axisMaximum - axisMinimum;
  const float stretch =
      std::clamp((stretchedLength - baseLength) / baseLength, 0.0f, 1.0f);
  const float crossMinimum =
      vertical ? targetMinimum.x : targetMinimum.y;
  const float crossMaximum =
      vertical ? targetMaximum.x : targetMaximum.y;
  const float crossInset =
      (crossMaximum - crossMinimum) * stretch * 0.06f;

  ImVec2 minimum;
  ImVec2 maximum;
  if (vertical) {
    minimum = ImVec2(crossMinimum + crossInset, axisMinimum);
    maximum = ImVec2(crossMaximum - crossInset, axisMaximum);
  } else {
    minimum = ImVec2(axisMinimum, crossMinimum + crossInset);
    maximum = ImVec2(axisMaximum, crossMaximum - crossInset);
  }

  const float crossSize = vertical ? maximum.x - minimum.x
                                   : maximum.y - minimum.y;
  const float radius = options.cornerRadius < 0.0f
                           ? crossSize * 0.32f
                           : options.cornerRadius * frame.density;
  ImGui::GetWindowDrawList()->AddRectFilled(
      minimum, maximum, ImGui::GetColorU32(options.color),
      std::max(radius, 0.0f));
}

}
