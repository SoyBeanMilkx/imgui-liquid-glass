#pragma once

#include "core/graphics/GraphicsBackend.hpp"
#include "ui/input/touch/InputBridge.hpp"

#include <mutex>

namespace glass_ui {

class OverlayRuntime final {
public:
  static OverlayRuntime &instance();

  OverlayRuntime(const OverlayRuntime &) = delete;
  OverlayRuntime &operator=(const OverlayRuntime &) = delete;
  OverlayRuntime(OverlayRuntime &&) = delete;
  OverlayRuntime &operator=(OverlayRuntime &&) = delete;

  bool initialize(graphics::GraphicsBackend backend);

private:
  OverlayRuntime() = default;

  InputBridge input_;
  std::mutex mutex_;
  bool initialized_ = false;
  graphics::GraphicsBackend backend_ = graphics::GraphicsBackend::Vulkan;
};

}
