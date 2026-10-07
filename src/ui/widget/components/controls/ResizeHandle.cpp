#include "ResizeHandle.hpp"

#include <algorithm>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kVisibleChannel = 1;
constexpr uint32_t kHoverChannel = 2;
constexpr uint32_t kActiveChannel = 3;
constexpr float kPi = 3.14159265358979323846f;

}

ResizeHandleResult ResizeHandle(Context &context, const char *id,
                                ImVec2 corner,
                                ResizeHandleCorner cornerKind,
                                const ResizeHandleOptions &options) {
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const float touchSize = std::max(options.touchSize * density, 1.0f);
  ResizeHandleResult result;
  if (cornerKind == ResizeHandleCorner::TopRight) {
    result.minimum = ImVec2(corner.x - touchSize, corner.y);
    result.maximum = ImVec2(corner.x, corner.y + touchSize);
  } else {
    result.minimum = ImVec2(corner.x - touchSize, corner.y - touchSize);
    result.maximum = corner;
  }

  result.hovered =
      options.enabled &&
      ImGui::IsMouseHoveringRect(result.minimum, result.maximum, false);
  result.pressed = result.hovered &&
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                   !ImGui::IsAnyItemActive();

  const ImGuiID itemId = ImGui::GetID(id);
  const float visible = context.animations().animate(
      itemId, kVisibleChannel, options.enabled ? 1.0f : 0.0f, 0.10f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float hover = context.animations().animate(
      itemId, kHoverChannel, result.hovered ? 1.0f : 0.0f, 0.07f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float active = context.animations().animate(
      itemId, kActiveChannel, options.active ? 1.0f : 0.0f, 0.06f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  if (visible <= 0.001f)
    return result;

  const float radius = std::max(options.radius * density, 1.0f);
  const float thickness = std::max(options.thickness * density, 1.0f);
  const float emphasis = std::max(hover, active);
  const float alpha = visible * (0.62f + 0.38f * emphasis);
  ImVec4 glowColor = options.color;
  glowColor.w *= alpha * 0.28f;
  ImVec4 lineColor = options.color;
  lineColor.w *= alpha;
  const ImVec2 center =
      cornerKind == ResizeHandleCorner::TopRight
          ? ImVec2(corner.x - radius, corner.y + radius)
          : ImVec2(corner.x - radius, corner.y - radius);
  const float startAngle =
      cornerKind == ResizeHandleCorner::TopRight ? 1.5f * kPi : 0.0f;
  const float endAngle =
      cornerKind == ResizeHandleCorner::TopRight ? 2.0f * kPi : 0.5f * kPi;
  const auto drawing = context.foreground();
  ImDrawList *drawList = drawing.drawList();
  drawList->PathArcTo(center, radius, startAngle, endAngle, 12);
  drawList->PathStroke(ImGui::GetColorU32(glowColor), ImDrawFlags_None,
                       thickness * (2.4f + 0.6f * emphasis));
  drawList->PathArcTo(center, radius, startAngle, endAngle, 12);
  drawList->PathStroke(ImGui::GetColorU32(lineColor), ImDrawFlags_None,
                       thickness);
  return result;
}

}
