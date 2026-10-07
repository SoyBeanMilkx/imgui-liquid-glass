#pragma once

#include "ui/widget/components/controls/Icon.hpp"

namespace glass_ui::widget {

using NavigationIcon = IconGlyph;

struct NavigationItemOptions {
  ImVec2 size = ImVec2(38.0f, 38.0f);
  float iconScale = 0.54f;
  bool selected = false;
  bool enabled = true;
};

bool NavigationItem(Context &context, const char *id, NavigationIcon icon,
                    const NavigationItemOptions &options = {});

}
