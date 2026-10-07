#pragma once

#include "ui/input/PointerEvent.hpp"

namespace glass_ui {

struct OverlayUiFrame {
  float width = 0.0f;
  float height = 0.0f;
  PointerEventView pointerEvents{};
};

}
