#pragma once

#include <GLES3/gl3.h>

namespace glass_ui::renderer::opengl {

class OpenGLStateGuard final {
public:
  OpenGLStateGuard() noexcept;
  ~OpenGLStateGuard();

  OpenGLStateGuard(const OpenGLStateGuard &) = delete;
  OpenGLStateGuard &operator=(const OpenGLStateGuard &) = delete;

private:
  GLint activeTexture_ = GL_TEXTURE0;
  GLint activeTextureBinding_ = 0;
  GLint texture0Binding_ = 0;
  GLint texture1Binding_ = 0;
  GLint program_ = 0;
  GLint vertexArray_ = 0;
  GLint arrayBuffer_ = 0;
  GLint elementArrayBuffer_ = 0;
  GLint drawFramebuffer_ = 0;
  GLint readFramebuffer_ = 0;
  GLint viewport_[4]{};
  GLint scissorBox_[4]{};
  GLint blendSrcRgb_ = GL_ONE;
  GLint blendDstRgb_ = GL_ZERO;
  GLint blendSrcAlpha_ = GL_ONE;
  GLint blendDstAlpha_ = GL_ZERO;
  GLint blendEquationRgb_ = GL_FUNC_ADD;
  GLint blendEquationAlpha_ = GL_FUNC_ADD;
  GLboolean colorMask_[4]{};
  bool blend_ = false;
  bool cullFace_ = false;
  bool depthTest_ = false;
  bool stencilTest_ = false;
  bool scissorTest_ = false;
};

}
