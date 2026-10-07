#pragma once

#include <cstdint>

namespace glass_ui::renderer {

struct RenderSurfaceInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t inputTransform = 0;
};

}
