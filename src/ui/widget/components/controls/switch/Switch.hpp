#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <optional>

namespace glass_ui::widget {

struct SwitchOptions {
  ImVec2 size{};
  float minimumTouchSize = -1.0f;
  std::optional<ImVec4> trackOffColor;
  std::optional<ImVec4> trackOnColor;
  std::optional<ImVec4> thumbColor;
  std::optional<ImVec4> glowColor;
  float glowSpread = -1.0f;
  float glowStrength = -1.0f;
  bool enabled = true;
};

bool Switch(Context &context, const char *id, bool *value,
            const SwitchOptions &options = {});

}
