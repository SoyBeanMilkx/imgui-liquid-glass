#include "Button.hpp"

#include "ui/widget/foundation/interaction/ItemBehavior.hpp"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kHoverAnimationChannel = 1;
constexpr uint32_t kPressAnimationChannel = 2;

ImVec4 mix(const ImVec4 &from, const ImVec4 &to, float amount) noexcept {
  const float value = std::clamp(amount, 0.0f, 1.0f);
  return ImVec4(from.x + (to.x - from.x) * value,
                from.y + (to.y - from.y) * value,
                from.z + (to.z - from.z) * value,
                from.w + (to.w - from.w) * value);
}

const char *renderedTextEnd(const char *label) noexcept {
  const char *end = label + std::strlen(label);
  for (const char *cursor = label; cursor + 1 < end; ++cursor) {
    if (cursor[0] == '#' && cursor[1] == '#')
      return cursor;
  }
  return end;
}

float resolveWidth(float requested, float contentWidth, float available,
                   float density) noexcept {
  if (requested > 0.0f)
    return requested * density;
  if (requested < 0.0f)
    return std::max(1.0f, available + requested * density);
  return contentWidth;
}

}
bool Button(Context &context, const char *label,
            const ButtonOptions &options) {
  if (!label)
    return false;

  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  ImFont *font = options.font ? options.font
                              : (theme.buttonFont ? theme.buttonFont
                                                  : ImGui::GetFont());
  const float configuredSize =
      options.fontSize > 0.0f ? options.fontSize : theme.buttonFontSize;
  const float fontSize = configuredSize > 0.0f
                             ? configuredSize * density
                             : ImGui::GetFontSize();
  const char *textEnd = renderedTextEnd(label);
  const ImVec2 textSize =
      font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label, textEnd);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float naturalWidth =
      textSize.x + 2.0f * theme.buttonHorizontalPadding * density;
  const float width =
      resolveWidth(options.size.x, naturalWidth, available.x, density);
  const float height = options.size.y > 0.0f
                           ? options.size.y * density
                           : theme.buttonHeight * density;
  const ItemBehaviorState item =
      buttonBehavior(context, label, ImVec2(width, height), options.enabled);

  const float hover = context.animations().animate(
      item.id, kHoverAnimationChannel, item.hovered ? 1.0f : 0.0f,
      theme.buttonAnimationResponse, frame.deltaTime, frame.frameNumber,
      frame.reduceMotion);
  const float press = context.animations().animate(
      item.id, kPressAnimationChannel, item.active ? 1.0f : 0.0f,
      theme.buttonAnimationResponse * 0.65f, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);

  const ImVec4 base = options.color.value_or(theme.buttonFill);
  const ImVec4 hovered = options.hoveredColor.value_or(theme.buttonHovered);
  const ImVec4 pressed = options.pressedColor.value_or(theme.buttonPressed);
  ImVec4 fill = mix(mix(base, hovered, hover), pressed, press);
  ImVec4 textColor = options.textColor.value_or(theme.buttonText);
  if (!options.enabled) {
    fill.w *= 0.42f;
    textColor.w *= 0.42f;
  }

  const float inset = std::min(width, height) * 0.018f * press;
  const ImVec2 visualMinimum(item.minimum.x + inset, item.minimum.y + inset);
  const ImVec2 visualMaximum(item.maximum.x - inset, item.maximum.y - inset);
  const float rounding =
      (options.rounding >= 0.0f ? options.rounding : theme.buttonRounding) *
      density;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(visualMinimum, visualMaximum,
                          ImGui::GetColorU32(fill), rounding);

  const ImVec2 textPosition(
      (item.minimum.x + item.maximum.x - textSize.x) * 0.5f,
      (item.minimum.y + item.maximum.y - textSize.y) * 0.5f);
  drawList->AddText(font, fontSize, textPosition,
                    ImGui::GetColorU32(textColor), label, textEnd);
  return item.pressed;
}

}
