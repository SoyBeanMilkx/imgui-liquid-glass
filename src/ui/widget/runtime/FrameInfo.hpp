#pragma once

#include "imgui.h"
#include "ui/input/PointerEvent.hpp"

#include <cstdint>

namespace glass_ui::widget {

struct FrameInfo {
  ImVec2 displaySize{};
  float deltaTime = 1.0f / 60.0f;
  float density = 1.0f;
  uint64_t frameNumber = 0;
  bool reduceMotion = false;
  PointerEventView pointerEvents{};
};

}
