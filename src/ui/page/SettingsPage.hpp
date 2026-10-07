#pragma once

#include "PageContext.hpp"

namespace glass_ui::page {

struct SettingsPageResult {
  bool resizeMode = false;
  bool resizeModeChanged = false;
};

class SettingsPage final {
public:
  SettingsPageResult draw(PageContext &context, bool resizeMode);
};

}
