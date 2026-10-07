#pragma once

#include "RenderSurfaceInfo.hpp"
#include "ui/OverlayUi.hpp"
#include "ui/input/OverlayInput.hpp"

#include <chrono>

struct ImDrawData;

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::renderer {

class OverlayFrameRuntime final {
public:
  void initialize(InputBridge &input) noexcept;
  bool ensureContext();
  void prepareFrame(const RenderSurfaceInfo &surface);
  ImDrawData *buildFrame();

  void commitInput() noexcept;
  void suspendInput() noexcept;
  void shutdownContext();

  void setEffectCapabilities(widget::EffectCapabilities capabilities) noexcept {
    ui_.setEffectCapabilities(capabilities);
  }
  const widget::GlassRequestQueue &glassRequests() const noexcept {
    return ui_.glassRequests();
  }
  bool ready() const noexcept { return ready_; }

private:
  OverlayInput input_;
  OverlayUi ui_;
  RenderSurfaceInfo surface_{};
  std::chrono::steady_clock::time_point lastFrame_ =
      std::chrono::steady_clock::now();
  bool ready_ = false;
};

}
