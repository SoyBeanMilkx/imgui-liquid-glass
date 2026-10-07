#pragma once

#include <cstdint>

namespace glass_ui {

struct InputSurface {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t transform = 0;
};

}
