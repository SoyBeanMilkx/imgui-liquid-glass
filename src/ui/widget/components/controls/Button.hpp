#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <optional>

namespace glass_ui::widget {

struct ButtonOptions {
  ImVec2 size{};
  ImFont *font = nullptr;
  float fontSize = 0.0f;
  float rounding = -1.0f;
  std::optional<ImVec4> color;
  std::optional<ImVec4> hoveredColor;
  std::optional<ImVec4> pressedColor;
  std::optional<ImVec4> textColor;
  bool enabled = true;
};

bool Button(Context &context, const char *label,
            const ButtonOptions &options = {});

}
