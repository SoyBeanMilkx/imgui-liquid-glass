#pragma once

#include <cstdint>

namespace glass_ui::display {

enum class Rotation : uint32_t {
  Rotate0 = 0,
  Rotate90 = 1,
  Rotate180 = 2,
  Rotate270 = 3,
};

struct DisplayInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  Rotation rotation = Rotation::Rotate0;
  bool detected = false;
};

// Returns a cached Java Display snapshot; failures set detected to false.
void initializeDisplayInfo() noexcept;
DisplayInfo queryDisplayInfo() noexcept;

// Converts display rotation to touch transform, falling back to landscape.
uint32_t inputTransform(const DisplayInfo &info) noexcept;

}
