#include "OverlayRuntime.hpp"

#include "core/graphics/GraphicsBootstrap.hpp"
#include "core/hooks/ElfLoadMonitor.hpp"
#include "utils/LogUtils.hpp"

namespace glass_ui {

OverlayRuntime &OverlayRuntime::instance() {
  static auto *runtime = new OverlayRuntime;
  return *runtime;
}

bool OverlayRuntime::initialize(graphics::GraphicsBackend backend) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_) {
    if (backend_ != backend)
      log::error("graphics backend already fixed to %s; rejected %s",
                 graphics::backendName(backend_), graphics::backendName(backend));
    return backend_ == backend;
  }
  log::info("libglass_ui.so loaded, backend=%s", graphics::backendName(backend));
  if (!hooks::initializeElfLoadMonitor())
    log::warn("ELF load monitoring unavailable");
  input_.initialize();
  if (!graphics::initialize(backend, input_)) {
    log::error("graphics backend initialization failed: %s",
               graphics::backendName(backend));
    return false;
  }
  backend_ = backend;
  initialized_ = true;
  return true;
}

}
