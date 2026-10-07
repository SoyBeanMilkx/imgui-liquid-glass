#pragma once

#include "ui/widget/components/controls/switch/Switch.hpp"

#include <optional>

namespace glass_ui::widget {

struct SwitchRowOptions {
  ImFont *titleFont = nullptr;
  ImFont *descriptionFont = nullptr;
  float titleFontSize = 12.0f;
  float descriptionFontSize = 10.0f;
  float minimumHeight = 40.0f;
  float textSpacing = 2.0f;
  float horizontalSpacing = 12.0f;
  float verticalPadding = 4.0f;
  std::optional<ImVec4> titleColor;
  std::optional<ImVec4> descriptionColor;
  SwitchOptions switchOptions{};
};

bool SwitchRow(Context &context, const char *id, const char *title,
               const char *description, bool *value,
               const SwitchRowOptions &options = {});

}
