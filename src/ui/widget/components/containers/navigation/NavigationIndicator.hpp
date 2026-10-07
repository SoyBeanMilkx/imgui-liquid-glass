#pragma once

#include "ui/widget/foundation/animation/SpringMotion.hpp"
#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

enum class NavigationAxis {
  Horizontal,
  Vertical,
};

struct NavigationIndicatorOptions {
  NavigationAxis axis = NavigationAxis::Vertical;
  ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
  float cornerRadius = -1.0f;
  SpringOptions leadingSpring = {0.25f, 0.64f, 0.02f};
  SpringOptions trailingSpring = {0.38f, 0.78f, 0.02f};
};

void NavigationIndicator(Context &context, const char *id,
                         ImVec2 targetMinimum, ImVec2 targetMaximum,
                         const NavigationIndicatorOptions &options = {});

}
