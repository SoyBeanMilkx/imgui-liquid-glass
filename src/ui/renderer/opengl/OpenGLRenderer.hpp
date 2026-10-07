#pragma once

#include <EGL/egl.h>

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::renderer::opengl {

void initialize(InputBridge &input) noexcept;
void beforeSwap(EGLDisplay display, EGLSurface surface) noexcept;
void destroyContext(EGLContext context) noexcept;
void destroySurface(EGLSurface surface) noexcept;

}
