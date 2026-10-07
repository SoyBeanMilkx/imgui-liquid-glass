#pragma once

#include "OpenGLSurfaceState.hpp"
#include "ui/renderer/OverlayFrameRuntime.hpp"

struct ImDrawData;

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::renderer::opengl {

class ImGuiOpenGLRenderer final {
public:
  void initialize(InputBridge &input) noexcept;
  bool ensure(EGLContext context);
  ImDrawData *draw(const OpenGLSurfaceState &surface);
  void render(ImDrawData *drawData);
  void commitInput() noexcept;
  void suspendInput() noexcept;
  void shutdown();

  void setEffectCapabilities(widget::EffectCapabilities capabilities) noexcept {
    runtime_.setEffectCapabilities(capabilities);
  }
  const widget::GlassRequestQueue &glassRequests() const noexcept {
    return runtime_.glassRequests();
  }
  bool uses(EGLContext context) const noexcept { return context_ == context; }

private:
  OverlayFrameRuntime runtime_;
  EGLContext context_ = EGL_NO_CONTEXT;
  bool ready_ = false;
};

}
