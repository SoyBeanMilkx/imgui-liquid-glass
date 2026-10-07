#include "ItemBehavior.hpp"
#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

ItemBehaviorState buttonBehavior(Context &context, const char *id, ImVec2 size,
                                 bool enabled) {
  const bool interactive = enabled && !context.gestures().tapsSuppressed();
  ImGui::BeginDisabled(!interactive);
  const bool pressed = ImGui::InvisibleButton(id, size);
  ItemBehaviorState state;
  state.id = ImGui::GetItemID();
  state.minimum = ImGui::GetItemRectMin();
  state.maximum = ImGui::GetItemRectMax();
  state.hovered = interactive && ImGui::IsItemHovered();
  state.active = interactive && ImGui::IsItemActive();
  state.pressed = interactive && pressed;
  ImGui::EndDisabled();
  return state;
}

}
