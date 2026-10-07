#pragma once

#include "Icon.hpp"
#include "ui/widget/runtime/Context.hpp"

#include <optional>
#include <string>

namespace glass_ui::widget {

struct TextFieldOptions {
  ImVec2 size{};
  const char *hint = "";
  std::optional<IconGlyph> icon;
  float rounding = -1.0f;
  float glassBlurMix = 0.32f;
  GlassStyle glassStyle{12.0f,
                        -24.0f,
                        0.24f,
                        0.16f,
                        0.0f,
                        0.0f,
                        1.0f,
                        ImVec4(1.0f, 1.0f, 1.0f, 0.07f),
                        GlassLighting{0.12f, 0.30f, 0.52f, 0.12f, 5.0f, 24.0f,
                                      ImVec2(-0.7071f, -0.7071f)}};
  bool focusAnimation = true;
  bool password = false;
  bool enabled = true;
};

struct TextFieldResult {
  bool changed = false;
  bool submitted = false;
  bool focused = false;
};

TextFieldResult TextField(Context &context, const char *id, std::string *value,
                          const TextFieldOptions &options = {});

}
