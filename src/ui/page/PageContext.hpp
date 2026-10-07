#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <array>

namespace glass_ui::page {

struct PageState {
  std::array<bool, 4> notificationSettings = {true, false, false, false};
  float glassBlurRadius = 12.0f;
};

struct PageContext {
  widget::Context &widgets;
  ImFont *regularFont = nullptr;
  ImFont *semiboldFont = nullptr;
  PageState &state;
};

}
