#include "Text.hpp"

#include "imgui.h"

#include <cfloat>

namespace glass_ui::widget {

void Text(Context &context, const char *text, const TextOptions &options) {
  if (!text)
    return;

  const float density = context.frame().density;
  const Theme &theme = context.theme();
  ImFont *font = options.font ? options.font
                              : (theme.textFont ? theme.textFont
                                                : ImGui::GetFont());
  const float configuredSize =
      options.fontSize > 0.0f ? options.fontSize : theme.textFontSize;
  const float fontSize = configuredSize > 0.0f
                             ? configuredSize * density
                             : ImGui::GetFontSize();
  const float wrapWidth =
      options.wrapWidth > 0.0f
          ? options.wrapWidth * density
          : (options.wrapWidth < 0.0f ? ImGui::GetContentRegionAvail().x
                                      : 0.0f);
  const ImVec2 size =
      font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text, nullptr);
  const ImVec4 color = options.color.value_or(theme.textPrimary);
  ImGui::GetWindowDrawList()->AddText(font, fontSize,
                                      ImGui::GetCursorScreenPos(),
                                      ImGui::GetColorU32(color), text, nullptr,
                                      wrapWidth);
  ImGui::Dummy(size);
}

}
