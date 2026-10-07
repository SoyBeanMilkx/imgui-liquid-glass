#include "NavigationItem.hpp"

#include "ui/widget/foundation/interaction/ItemBehavior.hpp"

#include <algorithm>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kSelectionChannel = 1;
constexpr uint32_t kHoverChannel = 2;
constexpr uint32_t kPressChannel = 3;

ImVec4 mix(const ImVec4 &from, const ImVec4 &to, float amount) noexcept {
  const float value = std::clamp(amount, 0.0f, 1.0f);
  return ImVec4(
      from.x + (to.x - from.x) * value, from.y + (to.y - from.y) * value,
      from.z + (to.z - from.z) * value, from.w + (to.w - from.w) * value);
}

}
bool NavigationItem(Context &context, const char *id, NavigationIcon icon,
                    const NavigationItemOptions &options) {
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const ImVec2 size(std::max(options.size.x * density, 1.0f),
                    std::max(options.size.y * density, 1.0f));
  const ItemBehaviorState item =
      buttonBehavior(context, id, size, options.enabled);
  const float selection = context.animations().animate(
      item.id, kSelectionChannel, options.selected ? 1.0f : 0.0f, 0.10f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float hover = context.animations().animate(
      item.id, kHoverChannel, item.hovered ? 1.0f : 0.0f, 0.08f,
      frame.deltaTime, frame.frameNumber, frame.reduceMotion);
  const float press = context.animations().animate(
      item.id, kPressChannel, item.active ? 1.0f : 0.0f, 0.06f, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec4 fill(1.0f, 1.0f, 1.0f,
              0.08f * hover * (1.0f - selection) + 0.05f * press);
  if (fill.w > 0.001f)
    drawList->AddRectFilled(item.minimum, item.maximum,
                            ImGui::GetColorU32(fill), size.y * 0.32f);

  ImVec4 iconColor =
      mix(ImVec4(0.84f, 0.88f, 0.92f, 0.78f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
          std::max(selection, hover));
  if (!options.enabled)
    iconColor.w *= 0.4f;
  const ImVec2 center((item.minimum.x + item.maximum.x) * 0.5f,
                      (item.minimum.y + item.maximum.y) * 0.5f);
  const float iconScale = std::clamp(options.iconScale, 0.0f, 1.0f);
  DrawIcon(context, icon, center,
           std::min(size.x, size.y) * iconScale * (1.0f - 0.06f * press) /
               density,
           iconColor);
  return item.pressed;
}

}
