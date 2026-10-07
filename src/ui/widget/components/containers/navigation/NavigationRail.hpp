#pragma once

#include "NavigationIndicator.hpp"
#include "NavigationItem.hpp"
#include "ui/widget/foundation/layout/LinearLayout.hpp"

#include <cstddef>

namespace glass_ui::widget {

struct NavigationRailItem {
  const char *id = nullptr;
  NavigationIcon icon = NavigationIcon::Grid;
  bool enabled = true;
};

struct NavigationRailOptions {
  LinearLayoutOptions layout = {
      LayoutDirection::Vertical,
      LayoutDistribution::SpaceEvenly,
      ContentGravity{LayoutAlignment::Center, LayoutAlignment::Center},
      LayoutInsets{},
      0.0f,
  };
  NavigationItemOptions item{};
  NavigationIndicatorOptions indicator{};
};

struct NavigationRailResult {
  bool pressed = false;
  bool selectionChanged = false;
  int pressedIndex = -1;
};

NavigationRailResult NavigationRail(
    Context &context, const char *id, const NavigationRailItem *items,
    std::size_t itemCount, int *selectedIndex,
    const NavigationRailOptions &options = {});

}
