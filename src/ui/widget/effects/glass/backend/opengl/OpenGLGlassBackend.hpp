#pragma once

#include "ui/widget/effects/EffectCapabilities.hpp"
#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"
#include "ui/widget/effects/glass/GlassRequestQueue.hpp"

#include <EGL/egl.h>

#include <memory>

namespace glass_ui::widget {

namespace opengl_glass {
struct Resources;
}

class OpenGLGlassBackend final {
public:
  OpenGLGlassBackend();
  ~OpenGLGlassBackend();

  OpenGLGlassBackend(const OpenGLGlassBackend &) = delete;
  OpenGLGlassBackend &operator=(const OpenGLGlassBackend &) = delete;

  bool ensure(EGLContext context, uint32_t width, uint32_t height);
  EffectCapabilities capabilities() const noexcept;
  bool prepareBackdrop(const GlassRequestQueue &queue);
  bool prepareRequestBackdrop(GlassBackdropSource source);
  void composite(const GlassRequest &request);
  void finishFrame() noexcept;
  void removeContext(EGLContext context) noexcept;
  void shutdown() noexcept;

private:
  std::unique_ptr<opengl_glass::Resources> resources_;
  GlassBlurPlan blurPlan_{};
  GlassBackdropSource activeSource_ = GlassBackdropSource::Frame;
  bool disabled_ = false;
};

}
