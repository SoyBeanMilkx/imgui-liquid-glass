#pragma once

#include <EGL/egl.h>

#include <cstdint>

namespace glass_ui::renderer::opengl {

struct OpenGLSurfaceState {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  uint32_t width = 0;
  uint32_t height = 0;

  bool valid() const noexcept;
  bool usesContext(EGLContext value) const noexcept { return context == value; }
  bool usesSurface(EGLSurface value) const noexcept { return surface == value; }
};

bool querySurfaceState(EGLDisplay display, EGLSurface surface,
                       OpenGLSurfaceState &state) noexcept;

}
