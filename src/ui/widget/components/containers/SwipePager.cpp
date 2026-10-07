#include "SwipePager.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

int wrappedIndex(int value, int count) noexcept {
  const int remainder = value % count;
  return remainder < 0 ? remainder + count : remainder;
}

void applyContentLayout(const Context &context,
                        const SwipePagerOptions &options) {
  const ImVec2 minimum = ImGui::GetCursorScreenPos();
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const LayoutRect bounds{
      minimum,
      ImVec2(minimum.x + std::max(available.x, 0.0f),
             minimum.y + std::max(available.y, 0.0f)),
  };
  const LayoutRect content =
      resolveBoxLayout(bounds, options.contentSize, context.frame().density,
                       options.contentLayout);
  ImGui::SetCursorScreenPos(content.minimum);
  ImGui::PushClipRect(content.minimum, content.maximum, true);
}

}
SwipePagerResult BeginSwipePager(Context &context, const char *id,
                                 int pageCount, int *selectedIndex,
                                 const SwipePagerOptions &options) {
  SwipePagerResult result;
  if (!id || !selectedIndex || pageCount <= 0) {
    context.gestures().pushTapScope(false);
    result.visible = ImGui::BeginChild(
        id ? id : "##invalid-swipe-pager", ImVec2(1.0f, 1.0f),
        options.childFlags, options.windowFlags);
    context.drawTransforms().attach(ImGui::GetWindowDrawList());
    applyContentLayout(context, options);
    return result;
  }

  *selectedIndex = std::clamp(*selectedIndex, 0, pageCount - 1);
  const float density = context.frame().density;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 size(resolveLayoutExtent(options.size.x, available.x, density),
                    resolveLayoutExtent(options.size.y, available.y, density));
  const ImVec2 minimum = ImGui::GetCursorScreenPos();
  const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
  const ImGuiID pagerId = ImGui::GetID(id);
  const HorizontalDragState swipe = context.gestures().horizontalDrag(
      pagerId, minimum, maximum,
      HorizontalDragParameters{options.touchSlop * density,
                               options.directionRatio});

  result.dragging = swipe.active;
  result.dragProgress =
      std::clamp(swipe.deltaX / std::max(size.x, 1.0f), -1.0f, 1.0f);
  if (swipe.released) {
    const float distance = std::max(options.minimumDistance * density,
                                    size.x * std::clamp(
                                                 options.distanceThreshold,
                                                 0.0f, 1.0f));
    const float velocity = std::max(options.flingVelocity, 0.0f) * density;
    int direction = 0;
    if (swipe.deltaX <= -distance || swipe.velocityX <= -velocity)
      direction = 1;
    else if (swipe.deltaX >= distance || swipe.velocityX >= velocity)
      direction = -1;

    if (direction != 0) {
      const int previous = *selectedIndex;
      const int requested = previous + direction;
      *selectedIndex = options.wrap
                           ? wrappedIndex(requested, pageCount)
                           : std::clamp(requested, 0, pageCount - 1);
      result.selectionChanged = *selectedIndex != previous;
    }
  }

  context.gestures().pushTapScope(swipe.consumeTap);
  result.visible = ImGui::BeginChild(id, size, options.childFlags,
                                     options.windowFlags);
  context.drawTransforms().attach(ImGui::GetWindowDrawList());
  applyContentLayout(context, options);
  return result;
}

void EndSwipePager(Context &context) {
  ImGui::PopClipRect();
  ImGui::EndChild();
  context.gestures().popTapScope();
}

}
