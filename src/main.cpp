#include "OverlayRuntime.hpp"

namespace {

constexpr auto kGraphicsBackend = glass_ui::graphics::GraphicsBackend::Vulkan;

void initialize() {
  glass_ui::OverlayRuntime::instance().initialize(kGraphicsBackend);
}

}

extern "C" {

__attribute__((constructor)) void glass_ui_overlay_init() { initialize(); }

}
