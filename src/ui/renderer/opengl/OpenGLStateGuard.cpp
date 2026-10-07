#include "OpenGLStateGuard.hpp"

namespace glass_ui::renderer::opengl {
namespace {

void restoreCapability(GLenum capability, bool enabled) noexcept {
  if (enabled)
    glEnable(capability);
  else
    glDisable(capability);
}

}
OpenGLStateGuard::OpenGLStateGuard() noexcept {
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTextureBinding_);
  if (activeTexture_ != GL_TEXTURE0) {
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0Binding_);
    glActiveTexture(static_cast<GLenum>(activeTexture_));
  } else {
    texture0Binding_ = activeTextureBinding_;
  }
  if (activeTexture_ != GL_TEXTURE1) {
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1Binding_);
    glActiveTexture(static_cast<GLenum>(activeTexture_));
  } else {
    texture1Binding_ = activeTextureBinding_;
  }
  glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer_);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
  glGetIntegerv(GL_VIEWPORT, viewport_);
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox_);
  glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
  glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb_);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha_);
  glGetBooleanv(GL_COLOR_WRITEMASK, colorMask_);
  blend_ = glIsEnabled(GL_BLEND);
  cullFace_ = glIsEnabled(GL_CULL_FACE);
  depthTest_ = glIsEnabled(GL_DEPTH_TEST);
  stencilTest_ = glIsEnabled(GL_STENCIL_TEST);
  scissorTest_ = glIsEnabled(GL_SCISSOR_TEST);
}

OpenGLStateGuard::~OpenGLStateGuard() {
  glUseProgram(static_cast<GLuint>(program_));
  glBindVertexArray(static_cast<GLuint>(vertexArray_));
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLuint>(elementArrayBuffer_));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                    static_cast<GLuint>(drawFramebuffer_));
  glBindFramebuffer(GL_READ_FRAMEBUFFER,
                    static_cast<GLuint>(readFramebuffer_));
  glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
  glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
  glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb_),
                          static_cast<GLenum>(blendEquationAlpha_));
  glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb_),
                      static_cast<GLenum>(blendDstRgb_),
                      static_cast<GLenum>(blendSrcAlpha_),
                      static_cast<GLenum>(blendDstAlpha_));
  glColorMask(colorMask_[0], colorMask_[1], colorMask_[2], colorMask_[3]);
  restoreCapability(GL_BLEND, blend_);
  restoreCapability(GL_CULL_FACE, cullFace_);
  restoreCapability(GL_DEPTH_TEST, depthTest_);
  restoreCapability(GL_STENCIL_TEST, stencilTest_);
  restoreCapability(GL_SCISSOR_TEST, scissorTest_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0Binding_));
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture1Binding_));
  glActiveTexture(static_cast<GLenum>(activeTexture_));
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(activeTextureBinding_));
}

}
