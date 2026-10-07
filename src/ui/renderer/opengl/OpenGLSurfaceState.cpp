#include "OpenGLSurfaceState.hpp"

namespace glass_ui::renderer::opengl {

bool OpenGLSurfaceState::valid() const noexcept {
  return display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE &&
         context != EGL_NO_CONTEXT && width > 1 && height > 1;
}

bool querySurfaceState(EGLDisplay display, EGLSurface surface,
                       OpenGLSurfaceState &state) noexcept {
  OpenGLSurfaceState next;
  next.display = display;
  next.surface = surface;
  next.context = eglGetCurrentContext();
  EGLint width = 0;
  EGLint height = 0;
  if (next.context == EGL_NO_CONTEXT ||
      eglGetCurrentDisplay() != display ||
      eglGetCurrentSurface(EGL_DRAW) != surface ||
      !eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
      !eglQuerySurface(display, surface, EGL_HEIGHT, &height) || width < 2 ||
      height < 2)
    return false;
  next.width = static_cast<uint32_t>(width);
  next.height = static_cast<uint32_t>(height);
  state = next;
  return true;
}

}
