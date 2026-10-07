#include "NavigationRail.hpp"

#include <algorithm>

namespace glass_ui::widget {

NavigationRailResult NavigationRail(
    Context &context, const char *id, const NavigationRailItem *items,
    std::size_t itemCount, int *selectedIndex,
    const NavigationRailOptions &options) {
  NavigationRailResult result;
  if (!items || itemCount == 0 || !selectedIndex)
    return result;

  const ImVec2 minimum = ImGui::GetCursorScreenPos();
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 extent(std::max(available.x, 0.0f),
                      std::max(available.y, 0.0f));
  const LayoutRect bounds{
      minimum,
      ImVec2(minimum.x + extent.x, minimum.y + extent.y),
  };
  const LinearLayout layout(bounds, options.item.size, itemCount,
                            context.frame().density, options.layout);
  int selected =
      std::clamp(*selectedIndex, 0, static_cast<int>(itemCount) - 1);
  *selectedIndex = selected;

  ImGui::PushID(id ? id : "##navigation-rail");
  NavigationIndicatorOptions indicator = options.indicator;
  indicator.axis = options.layout.direction == LayoutDirection::Vertical
                       ? NavigationAxis::Vertical
                       : NavigationAxis::Horizontal;
  const LayoutRect selectedSlot =
      layout.slot(static_cast<std::size_t>(selected));
  NavigationIndicator(context, "##indicator", selectedSlot.minimum,
                      selectedSlot.maximum, indicator);

  const float density = context.frame().density;
  for (std::size_t index = 0; index < itemCount; ++index) {
    const LayoutRect slot = layout.slot(index);
    ImGui::SetCursorScreenPos(slot.minimum);
    ImGui::PushID(static_cast<int>(index));
    NavigationItemOptions item = options.item;
    item.size = ImVec2(slot.width() / density, slot.height() / density);
    item.selected = selected == static_cast<int>(index);
    item.enabled = item.enabled && items[index].enabled;
    if (NavigationItem(context,
                       items[index].id ? items[index].id : "##item",
                       items[index].icon, item)) {
      result.pressed = true;
      result.pressedIndex = static_cast<int>(index);
      result.selectionChanged = selected != result.pressedIndex;
      selected = result.pressedIndex;
      *selectedIndex = static_cast<int>(index);
    }
    ImGui::PopID();
  }
  ImGui::PopID();
  ImGui::SetCursorScreenPos(minimum);
  ImGui::Dummy(extent);
  return result;
}

}
