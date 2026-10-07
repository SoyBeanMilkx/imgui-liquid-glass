#include "OpenGLRenderer.hpp"

#include "ImGuiOpenGLRenderer.hpp"
#include "OpenGLStateGuard.hpp"
#include "OpenGLSurfaceState.hpp"
#include "ui/widget/effects/glass/backend/opengl/OpenGLGlassBackend.hpp"
#include "ui/widget/effects/glass/backend/opengl/OpenGLGlassDrawCallbacks.hpp"
#include "utils/LogUtils.hpp"

#include <atomic>
#include <mutex>

namespace glass_ui::renderer::opengl {
namespace {

class OpenGLRenderer final {
public:
  void initialize(InputBridge &input) noexcept { imgui_.initialize(input); }
  void beforeSwap(EGLDisplay display, EGLSurface surface) noexcept;
  void destroyContext(EGLContext context) noexcept;
  void destroySurface(EGLSurface surface) noexcept;

private:
  std::mutex mutex_;
  ImGuiOpenGLRenderer imgui_;
  widget::OpenGLGlassBackend glass_;
  widget::OpenGLGlassDrawCallbacks glassCallbacks_;
  OpenGLSurfaceState surface_{};
  std::atomic<uint32_t> presentCount_{0};
};

OpenGLRenderer &renderer() {
  static auto *instance = new OpenGLRenderer;
  return *instance;
}

void OpenGLRenderer::beforeSwap(EGLDisplay display,
                                EGLSurface surface) noexcept {
  OpenGLSurfaceState current;
  if (!querySurfaceState(display, surface, current))
    return;
  const uint32_t sequence = presentCount_.fetch_add(1) + 1;
  if (sequence <= 3)
    return;

  std::lock_guard<std::mutex> lock(mutex_);
  if (surface_.surface != EGL_NO_SURFACE && surface_.surface != surface)
    return;
  surface_ = current;

  {
    OpenGLStateGuard state;
    if (!imgui_.ensure(current.context)) {
      imgui_.suspendInput();
      return;
    }
    glass_.ensure(current.context, current.width, current.height);
  }
  imgui_.setEffectCapabilities(glass_.capabilities());
  ImDrawData *drawData = imgui_.draw(current);
  if (!drawData) {
    imgui_.suspendInput();
    return;
  }

  bool glassPrepared = false;
  {
    OpenGLStateGuard state;
    glassPrepared = glass_.prepareBackdrop(imgui_.glassRequests());
  }
  if (glassPrepared)
    glassCallbacks_.inject(*drawData, imgui_.glassRequests(), glass_);
  {
    OpenGLStateGuard state;
    imgui_.render(drawData);
  }
  glassCallbacks_.clear();
  if (glassPrepared)
    glass_.finishFrame();
  imgui_.commitInput();
}

void OpenGLRenderer::destroyContext(EGLContext context) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!imgui_.uses(context))
    return;
  glassCallbacks_.clear();
  glass_.removeContext(context);
  imgui_.shutdown();
  surface_ = {};
  presentCount_.store(0);
}

void OpenGLRenderer::destroySurface(EGLSurface surface) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!surface_.usesSurface(surface))
    return;
  imgui_.suspendInput();
  surface_.surface = EGL_NO_SURFACE;
  surface_.width = 0;
  surface_.height = 0;
  presentCount_.store(0);
}

}
void initialize(InputBridge &input) noexcept { renderer().initialize(input); }

void beforeSwap(EGLDisplay display, EGLSurface surface) noexcept {
  renderer().beforeSwap(display, surface);
}

void destroyContext(EGLContext context) noexcept {
  renderer().destroyContext(context);
}

void destroySurface(EGLSurface surface) noexcept {
  renderer().destroySurface(surface);
}

}
