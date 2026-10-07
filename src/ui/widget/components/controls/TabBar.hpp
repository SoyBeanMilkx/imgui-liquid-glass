#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <cstddef>
#include <optional>

namespace glass_ui::widget {

struct TabBarOptions {
  ImFont *font = nullptr;
  float fontSize = 0.0f;
  float height = 0.0f;
  float horizontalPadding = -1.0f;
  float spacing = -1.0f;
  float indicatorHeight = -1.0f;
  float indicatorAnimationDuration = -1.0f;
  float indicatorStretch = -1.0f;
  std::optional<ImVec4> textColor;
  std::optional<ImVec4> selectedTextColor;
  std::optional<ImVec4> indicatorColor;
  std::optional<ImVec4> dividerColor;
  bool showDivider = true;
};

bool TabBar(Context &context, const char *id, const char *const *labels,
            std::size_t count, int *selectedIndex,
            const TabBarOptions &options = {});

}
