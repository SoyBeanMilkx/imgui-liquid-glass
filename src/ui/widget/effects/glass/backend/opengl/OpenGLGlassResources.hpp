#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cstdint>

namespace glass_ui::widget::opengl_glass {

inline constexpr uint32_t kMaxGlassRegions = 32;
inline constexpr float kMaximumBlurRadius = 96.0f;

struct BlurUniforms {
  GLint source = -1;
  GLint inverseSourceResolution = -1;
  GLint direction = -1;
  GLint centerWeight = -1;
  GLint pairCount = -1;
  GLint passKind = -1;
  GLint pairWeights = -1;
  GLint pairOffsets = -1;
};

struct GlassUniforms {
  GLint backdrop = -1;
  GLint rawBackdrop = -1;
  GLint bounds = -1;
  GLint radii = -1;
  GLint refraction = -1;
  GLint filter = -1;
  GLint tint = -1;
  GLint framebuffer = -1;
  GLint deformation = -1;
  GLint lighting = -1;
  GLint light = -1;
  GLint framebufferSize = -1;
};

struct Resources {
  EGLContext context = EGL_NO_CONTEXT;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t blurWidth = 0;
  uint32_t blurHeight = 0;
  GLuint captureTexture = 0;
  GLuint captureFramebuffer = 0;
  GLuint contentCaptureTexture = 0;
  GLuint contentCaptureFramebuffer = 0;
  GLuint pingTexture = 0;
  GLuint pingFramebuffer = 0;
  GLuint pongTexture = 0;
  GLuint pongFramebuffer = 0;
  GLuint blurProgram = 0;
  GLuint glassProgram = 0;
  GLuint vertexArray = 0;
  BlurUniforms blur{};
  GlassUniforms glass{};
  bool prepared = false;
};

bool createResources(EGLContext context, uint32_t width, uint32_t height,
                     Resources &resources);
void destroyResources(Resources &resources) noexcept;

} // namespace glass_ui::widget::opengl_glass
