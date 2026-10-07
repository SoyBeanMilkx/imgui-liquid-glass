#pragma once

#include "LayoutTypes.hpp"

namespace glass_ui::widget {

struct BoxLayoutOptions {
  ContentGravity gravity{};
  LayoutInsets padding{};
};

LayoutRect resolveBoxLayout(const LayoutRect &bounds, ImVec2 contentSize,
                            float density,
                            const BoxLayoutOptions &options = {}) noexcept;

}
