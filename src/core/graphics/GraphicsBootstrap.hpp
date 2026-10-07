#pragma once

#include "GraphicsBackend.hpp"

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::graphics {

bool initialize(GraphicsBackend backend, InputBridge &input) noexcept;

}
