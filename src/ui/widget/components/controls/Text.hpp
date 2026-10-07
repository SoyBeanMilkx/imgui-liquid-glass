#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <optional>

namespace glass_ui::widget {

struct TextOptions {
  ImFont *font = nullptr;
  std::optional<ImVec4> color;
  float fontSize = 0.0f;
  float wrapWidth = 0.0f;
};

void Text(Context &context, const char *text,
          const TextOptions &options = {});

}
