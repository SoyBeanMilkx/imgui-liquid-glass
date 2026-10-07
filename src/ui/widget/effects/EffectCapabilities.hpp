#pragma once

#include <cstdint>

namespace glass_ui::widget {

struct EffectCapabilities {
  bool backdropCapture = false;
  bool blur = false;
  bool refraction = false;
  bool dispersion = false;
  uint32_t maxGlassRegions = 0;
};

}
