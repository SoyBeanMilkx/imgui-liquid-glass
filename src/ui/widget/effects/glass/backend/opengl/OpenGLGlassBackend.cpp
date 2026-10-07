#include "OpenGLGlassBackend.hpp"

#include "OpenGLGlassBlurPass.hpp"
#include "OpenGLGlassResources.hpp"
#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"
#include "utils/LogUtils.hpp"

#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept {
  return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

bool hasVisibleRequest(const GlassRequestQueue &queue) noexcept {
  for (const GlassRequest &request : queue.requests()) {
    if (request.opacity > 0.0f && request.max.x > request.min.x &&
        request.max.y > request.min.y)
      return true;
  }
  return false;
}

} // namespace
OpenGLGlassBackend::OpenGLGlassBackend() = default;

OpenGLGlassBackend::~OpenGLGlassBackend() { shutdown(); }

bool OpenGLGlassBackend::ensure(EGLContext context, uint32_t width,
                                uint32_t height) {
  if (resources_ && resources_->context == context &&
      resources_->width == width && resources_->height == height)
    return true;
  if (resources_) {
    if (resources_->context == eglGetCurrentContext())
      opengl_glass::destroyResources(*resources_);
    resources_.reset();
  }
  disabled_ = false;
  auto resources = std::make_unique<opengl_glass::Resources>();
  if (!opengl_glass::createResources(context, width, height, *resources)) {
    disabled_ = true;
    log::warn("liquid glass unavailable for OpenGL context %p", context);
    return false;
  }
  resources_ = std::move(resources);
  return true;
}

EffectCapabilities OpenGLGlassBackend::capabilities() const noexcept {
  if (!resources_ || disabled_)
    return {};
  return EffectCapabilities{true, true, true, true,
                            opengl_glass::kMaxGlassRegions};
}

bool OpenGLGlassBackend::prepareBackdrop(const GlassRequestQueue &queue) {
  if (!resources_ || disabled_ || !hasVisibleRequest(queue))
    return false;
  opengl_glass::Resources &resources = *resources_;
  resources.prepared = false;

  GLint sourceFramebuffer = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &sourceFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,
                    static_cast<GLuint>(sourceFramebuffer));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.captureFramebuffer);
  glBlitFramebuffer(0, 0, static_cast<GLint>(resources.width),
                    static_cast<GLint>(resources.height), 0, 0,
                    static_cast<GLint>(resources.width),
                    static_cast<GLint>(resources.height), GL_COLOR_BUFFER_BIT,
                    GL_NEAREST);

  const float physicalRadius =
      clampFinite(queue.backdropStyle().blurRadius * queue.density(), 0.0f,
                  opengl_glass::kMaximumBlurRadius, 0.01f);
  const GlassBlurPlan plan =
      makeGlassBlurPlan(physicalRadius, queue.backdropStyle().quality);
  blurPlan_ = plan;
  resources.prepared = opengl_glass::renderGlassBlur(
      resources, resources.captureTexture, blurPlan_);
  activeSource_ = GlassBackdropSource::Frame;
  return resources.prepared;
}

bool OpenGLGlassBackend::prepareRequestBackdrop(GlassBackdropSource source) {
  if (!resources_ || !resources_->prepared)
    return false;
  opengl_glass::Resources &resources = *resources_;
  if (source == GlassBackdropSource::Frame && activeSource_ == source)
    return true;

  GLint readFramebuffer = 0;
  GLint drawFramebuffer = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);

  GLuint rawTexture = resources.captureTexture;
  if (source == GlassBackdropSource::PreviousContent) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(readFramebuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.contentCaptureFramebuffer);
    glBlitFramebuffer(0, 0, static_cast<GLint>(resources.width),
                      static_cast<GLint>(resources.height), 0, 0,
                      static_cast<GLint>(resources.width),
                      static_cast<GLint>(resources.height), GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    rawTexture = resources.contentCaptureTexture;
  }

  const bool prepared =
      opengl_glass::renderGlassBlur(resources, rawTexture, blurPlan_);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
  if (prepared)
    activeSource_ = source;
  return prepared;
}

void OpenGLGlassBackend::composite(const GlassRequest &request) {
  if (!resources_ || !resources_->prepared)
    return;
  opengl_glass::Resources &resources = *resources_;
  const float width = request.max.x - request.min.x;
  const float height = request.max.y - request.min.y;
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f ||
      height <= 0.0f)
    return;

  const float density = std::max(request.density, 0.001f);
  const float shadowExtent =
      request.style.lighting.outerShadow > 0.0f
          ? clampFinite(request.style.lighting.shadowExtent * density, 0.0f,
                        32.0f * density, 5.0f * density)
          : 0.0f;
  const float drawExtent = std::max(shadowExtent, 1.0f);
  const float clipMinX =
      std::max({0.0f, request.min.x - drawExtent, request.clipRect.x});
  const float clipMinY =
      std::max({0.0f, request.min.y - drawExtent, request.clipRect.y});
  const float clipMaxX =
      std::min({static_cast<float>(resources.width),
                request.max.x + drawExtent, request.clipRect.z});
  const float clipMaxY =
      std::min({static_cast<float>(resources.height),
                request.max.y + drawExtent, request.clipRect.w});
  if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
    return;

  glViewport(0, 0, static_cast<GLsizei>(resources.width),
             static_cast<GLsizei>(resources.height));
  glEnable(GL_SCISSOR_TEST);
  glScissor(static_cast<GLint>(std::floor(clipMinX)),
            static_cast<GLint>(std::floor(resources.height - clipMaxY)),
            static_cast<GLsizei>(std::ceil(clipMaxX) - std::floor(clipMinX)),
            static_cast<GLsizei>(std::ceil(clipMaxY) - std::floor(clipMinY)));
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glUseProgram(resources.glassProgram);
  glBindVertexArray(resources.vertexArray);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, resources.pingTexture);
  glUniform1i(resources.glass.backdrop, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D,
                activeSource_ == GlassBackdropSource::PreviousContent
                    ? resources.contentCaptureTexture
                    : resources.captureTexture);
  glUniform1i(resources.glass.rawBackdrop, 1);
  glUniform4f(resources.glass.bounds, request.min.x, request.min.y,
              request.max.x, request.max.y);

  const float maximumRadius = std::min(width, height) * 0.5f;
  float radii[4] = {
      clampFinite(request.radii.topLeft * density, 0.0f, maximumRadius, 0.0f),
      clampFinite(request.radii.topRight * density, 0.0f, maximumRadius, 0.0f),
      clampFinite(request.radii.bottomRight * density, 0.0f, maximumRadius,
                  0.0f),
      clampFinite(request.radii.bottomLeft * density, 0.0f, maximumRadius,
                  0.0f)};
  if (request.shape == GlassShapeKind::Capsule)
    std::fill(std::begin(radii), std::end(radii), maximumRadius);
  glUniform4fv(resources.glass.radii, 1, radii);
  glUniform4f(resources.glass.refraction,
              clampFinite(request.style.refractionHeight * density, 0.001f,
                          std::max(0.001f, maximumRadius), 20.0f * density),
              clampFinite(request.style.refractionAmount * density, -960.0f,
                          960.0f, -70.0f * density),
              clampFinite(request.style.depthEffect, -2.0f, 2.0f, 0.3f),
              clampFinite(request.style.chromaticAberration, 0.0f, 1.0f, 0.5f));
  glUniform4f(resources.glass.filter,
              clampFinite(request.style.contrast, -1.0f, 1.0f, 0.0f),
              clampFinite(request.style.whitePoint, -1.0f, 1.0f, 0.0f),
              clampFinite(request.style.chromaMultiplier, 0.0f, 2.0f, 1.0f),
              clampFinite(request.opacity, 0.0f, 1.0f, 1.0f));
  glUniform4f(resources.glass.tint,
              clampFinite(request.style.tint.x, 0.0f, 1.0f, 1.0f),
              clampFinite(request.style.tint.y, 0.0f, 1.0f, 1.0f),
              clampFinite(request.style.tint.z, 0.0f, 1.0f, 1.0f),
              clampFinite(request.style.tint.w, 0.0f, 1.0f, 0.0f));
  glUniform4f(resources.glass.framebuffer, static_cast<float>(resources.width),
              static_cast<float>(resources.height), 0.0f,
              request.shape == GlassShapeKind::Circle ? 1.0f : 0.0f);
  glUniform4f(resources.glass.deformation,
              clampFinite(request.deformation.x, -0.35f, 0.35f, 0.0f),
              clampFinite(request.deformation.y, -0.35f, 0.35f, 0.0f),
              clampFinite(request.blurMix, 0.0f, 1.0f, 1.0f), 0.0f);
  const GlassLighting &lighting = request.style.lighting;
  glUniform4f(resources.glass.lighting,
              clampFinite(lighting.innerShadow, 0.0f, 1.0f, 0.12f),
              clampFinite(lighting.rimGlow, 0.0f, 1.0f, 0.20f),
              clampFinite(lighting.specular, 0.0f, 2.0f, 0.35f),
              clampFinite(lighting.outerShadow, 0.0f, 1.0f, 0.14f));
  glUniform4f(resources.glass.light,
              clampFinite(lighting.lightDirection.x, -1.0f, 1.0f, -0.7071f),
              clampFinite(lighting.lightDirection.y, -1.0f, 1.0f, -0.7071f),
              shadowExtent,
              clampFinite(lighting.specularPower, 1.0f, 128.0f, 28.0f));
  glUniform2f(resources.glass.framebufferSize,
              static_cast<float>(resources.width),
              static_cast<float>(resources.height));
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glActiveTexture(GL_TEXTURE0);
}

void OpenGLGlassBackend::finishFrame() noexcept {
  if (resources_)
    resources_->prepared = false;
}

void OpenGLGlassBackend::removeContext(EGLContext context) noexcept {
  if (!resources_ || resources_->context != context)
    return;
  if (eglGetCurrentContext() == context)
    opengl_glass::destroyResources(*resources_);
  resources_.reset();
  disabled_ = false;
}

void OpenGLGlassBackend::shutdown() noexcept {
  if (resources_ && resources_->context == eglGetCurrentContext())
    opengl_glass::destroyResources(*resources_);
  resources_.reset();
  disabled_ = false;
}

} // namespace glass_ui::widget
