#include "SwitchRow.hpp"

#include "imgui.h"

#include <algorithm>
#include <cfloat>

namespace glass_ui::widget {
namespace {

ImFont *resolveFont(ImFont *requested, ImFont *themed) noexcept {
  return requested ? requested : (themed ? themed : ImGui::GetFont());
}

ImVec4 disabledColor(ImVec4 color, bool enabled) noexcept {
  if (!enabled)
    color.w *= 0.45f;
  return color;
}

}
bool SwitchRow(Context &context, const char *id, const char *title,
               const char *description, bool *value,
               const SwitchRowOptions &options) {
  if (!id || !title || !description || !value)
    return false;

  const float density = context.frame().density;
  const Theme &theme = context.theme();
  ImFont *titleFont =
      resolveFont(options.titleFont, theme.buttonFont ? theme.buttonFont
                                                     : theme.textFont);
  ImFont *descriptionFont =
      resolveFont(options.descriptionFont, theme.textFont);
  const float titleSize = options.titleFontSize * density;
  const float descriptionSize = options.descriptionFontSize * density;
  const float minimumHeight = options.minimumHeight * density;
  const float horizontalSpacing = options.horizontalSpacing * density;
  const float textSpacing = options.textSpacing * density;
  const float verticalPadding = options.verticalPadding * density;
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float availableWidth = ImGui::GetContentRegionAvail().x;

  SwitchOptions switchOptions = options.switchOptions;
  if (switchOptions.size.x <= 0.0f)
    switchOptions.size.x = 28.0f;
  if (switchOptions.size.y <= 0.0f)
    switchOptions.size.y = 17.0f;
  if (switchOptions.minimumTouchSize < 0.0f)
    switchOptions.minimumTouchSize = options.minimumHeight;
  const float touchWidth =
      std::max(switchOptions.size.x, switchOptions.minimumTouchSize) * density;
  const float touchHeight =
      std::max(switchOptions.size.y, switchOptions.minimumTouchSize) * density;
  const float textX = start.x + touchWidth + horizontalSpacing;
  const float wrapWidth =
      std::max(1.0f, start.x + availableWidth - textX);
  const ImVec2 titleExtent =
      titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
  const ImVec2 descriptionExtent = descriptionFont->CalcTextSizeA(
      descriptionSize, FLT_MAX, wrapWidth, description);
  const float textHeight =
      titleExtent.y + textSpacing + descriptionExtent.y;
  const float rowHeight =
      std::max(minimumHeight, textHeight + 2.0f * verticalPadding);

  ImGui::SetCursorScreenPos(
      ImVec2(start.x, start.y + (rowHeight - touchHeight) * 0.5f));
  const bool changed = Switch(context, id, value, switchOptions);

  const float textY = start.y + (rowHeight - textHeight) * 0.5f;
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const ImVec4 titleColor = disabledColor(
      options.titleColor.value_or(theme.textPrimary), switchOptions.enabled);
  const ImVec4 descriptionColor = disabledColor(
      options.descriptionColor.value_or(
          ImVec4(theme.textPrimary.x, theme.textPrimary.y,
                 theme.textPrimary.z, theme.textPrimary.w * 0.68f)),
      switchOptions.enabled);
  drawList->AddText(titleFont, titleSize, ImVec2(textX, textY),
                    ImGui::GetColorU32(titleColor), title);
  drawList->AddText(descriptionFont, descriptionSize,
                    ImVec2(textX, textY + titleExtent.y + textSpacing),
                    ImGui::GetColorU32(descriptionColor), description,
                    nullptr, wrapWidth);

  ImGui::SetCursorScreenPos(start);
  ImGui::Dummy(ImVec2(availableWidth, rowHeight));
  return changed;
}

}
