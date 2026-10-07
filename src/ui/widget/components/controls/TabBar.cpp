#include "TabBar.hpp"

#include "ui/widget/foundation/interaction/ItemBehavior.hpp"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kHoverAnimationChannel = 30;
constexpr uint32_t kIndicatorCenterTransitionChannel = 31;
constexpr uint32_t kIndicatorWidthAnimationChannel = 32;
constexpr uint32_t kSelectionAnimationChannel = 33;

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

float tabWidth(ImFont *font, float fontSize, const char *label,
               float horizontalPadding) {
  const char *safeLabel = label ? label : "";
  const char *textEnd = renderedTextEnd(safeLabel);
  return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, safeLabel, textEnd).x +
         2.0f * horizontalPadding;
}

}
bool TabBar(Context &context, const char *id, const char *const *labels,
            std::size_t count, int *selectedIndex,
            const TabBarOptions &options) {
  if (!id || !labels || count == 0 || !selectedIndex)
    return false;

  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  ImFont *font = options.font
                     ? options.font
                     : (theme.tabFont ? theme.tabFont : ImGui::GetFont());
  const float configuredFontSize =
      options.fontSize > 0.0f ? options.fontSize : theme.tabFontSize;
  const float fontSize = configuredFontSize > 0.0f
                             ? configuredFontSize * density
                             : ImGui::GetFontSize();
  const float height =
      (options.height > 0.0f ? options.height : theme.tabHeight) * density;
  const float horizontalPadding =
      (options.horizontalPadding >= 0.0f ? options.horizontalPadding
                                         : theme.tabHorizontalPadding) *
      density;
  const float spacing =
      (options.spacing >= 0.0f ? options.spacing : theme.tabSpacing) * density;
  const float indicatorHeight =
      (options.indicatorHeight >= 0.0f ? options.indicatorHeight
                                       : theme.tabIndicatorHeight) *
      density;
  const float indicatorAnimationDuration =
      options.indicatorAnimationDuration >= 0.0f
          ? options.indicatorAnimationDuration
          : theme.tabIndicatorAnimationDuration;
  const float indicatorStretch =
      (options.indicatorStretch >= 0.0f ? options.indicatorStretch
                                       : theme.tabIndicatorStretch) *
      density;
  const ImVec4 normalColor = options.textColor.value_or(theme.tabText);
  const ImVec4 selectedColor =
      options.selectedTextColor.value_or(theme.tabTextSelected);
  const ImVec4 indicatorColor =
      options.indicatorColor.value_or(theme.tabIndicator);
  const ImVec4 dividerColor = options.dividerColor.value_or(theme.divider);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float availableWidth = ImGui::GetContentRegionAvail().x;
  const ImGuiID tabBarId = ImGui::GetID(id);
  *selectedIndex =
      std::clamp(*selectedIndex, 0, static_cast<int>(count) - 1);

  bool changed = false;
  ImGui::PushID(id);
  for (std::size_t index = 0; index < count; ++index) {
    if (index > 0)
      ImGui::SameLine(0.0f, spacing);

    const char *label = labels[index] ? labels[index] : "";
    const char *textEnd = renderedTextEnd(label);
    const ImVec2 textSize =
        font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label, textEnd);
    const float width = textSize.x + 2.0f * horizontalPadding;
    ImGui::PushID(static_cast<int>(index));
    const ItemBehaviorState item =
        buttonBehavior(context, "##tab", ImVec2(width, height), true);
    ImGui::PopID();
    if (item.pressed && *selectedIndex != static_cast<int>(index)) {
      *selectedIndex = static_cast<int>(index);
      changed = true;
    }

    const float hover = context.animations().animate(
        item.id, kHoverAnimationChannel, item.hovered ? 1.0f : 0.0f,
        theme.tabAnimationResponse, frame.deltaTime, frame.frameNumber,
        frame.reduceMotion);
    const bool selected = *selectedIndex == static_cast<int>(index);
    const float selection = context.animations().animate(
        item.id, kSelectionAnimationChannel, selected ? 1.0f : 0.0f,
        theme.tabAnimationResponse, frame.deltaTime, frame.frameNumber,
        frame.reduceMotion);
    const ImVec4 idle = mix(normalColor, selectedColor, selection);
    const ImVec4 color = mix(idle, selectedColor, hover * 0.55f);
    const ImVec2 textPosition(
        item.minimum.x + (width - textSize.x) * 0.5f,
        item.minimum.y + (height - textSize.y) * 0.5f -
            indicatorHeight * 0.35f);
    ImGui::GetWindowDrawList()->AddText(font, fontSize, textPosition,
                                        ImGui::GetColorU32(color), label,
                                        textEnd);
  }
  ImGui::PopID();

  float targetCenter = 0.0f;
  float targetWidth = 0.0f;
  float cursorX = start.x;
  float tabBarWidth = 0.0f;
  for (std::size_t index = 0; index < count; ++index) {
    const float width = tabWidth(font, fontSize, labels[index],
                                 horizontalPadding);
    if (*selectedIndex == static_cast<int>(index)) {
      targetCenter = cursorX - start.x + width * 0.5f;
      targetWidth = width;
    }
    cursorX += width + spacing;
    tabBarWidth += width;
    if (index + 1 < count)
      tabBarWidth += spacing;
  }

  const AnimationTransition center = context.animations().transition(
      tabBarId, kIndicatorCenterTransitionChannel, targetCenter,
      indicatorAnimationDuration, frame.deltaTime, frame.frameNumber,
      frame.reduceMotion);
  const float baseIndicatorWidth = context.animations().animate(
      tabBarId, kIndicatorWidthAnimationChannel, targetWidth,
      theme.tabAnimationResponse, frame.deltaTime, frame.frameNumber,
      frame.reduceMotion);
  constexpr float kPi = 3.14159265358979323846f;
  const float stretch = center.active
                            ? std::sin(center.progress * kPi) *
                                  indicatorStretch
                            : 0.0f;
  const float indicatorWidth =
      std::min(std::max(baseIndicatorWidth + stretch, indicatorHeight),
               tabBarWidth);
  const float indicatorX = std::clamp(
      center.value - indicatorWidth * 0.5f, 0.0f,
      std::max(0.0f, tabBarWidth - indicatorWidth));
  const float baseline = start.y + height;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  if (options.showDivider) {
    drawList->AddLine(ImVec2(start.x, baseline),
                      ImVec2(start.x + availableWidth, baseline),
                      ImGui::GetColorU32(dividerColor), 1.0f * density);
  }
  drawList->AddRectFilled(
      ImVec2(start.x + indicatorX, baseline - indicatorHeight),
      ImVec2(start.x + indicatorX + indicatorWidth, baseline),
      ImGui::GetColorU32(indicatorColor), indicatorHeight * 0.5f);
  return changed;
}

}
