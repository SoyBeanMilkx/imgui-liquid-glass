#include "OpenGLGlassBlurPass.hpp"

namespace glass_ui::widget::opengl_glass {
namespace {

void renderPass(Resources &resources, GLuint source, GLuint framebuffer,
                uint32_t sourceWidth, uint32_t sourceHeight, float passKind,
                float directionX, float directionY,
                const GlassBlurPlan &plan) {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, static_cast<GLsizei>(resources.blurWidth),
             static_cast<GLsizei>(resources.blurHeight));
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glUseProgram(resources.blurProgram);
  glBindVertexArray(resources.vertexArray);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, source);
  glUniform1i(resources.blur.source, 0);
  glUniform2f(resources.blur.inverseSourceResolution,
              1.0f / static_cast<float>(sourceWidth),
              1.0f / static_cast<float>(sourceHeight));
  glUniform2f(resources.blur.direction, directionX, directionY);
  glUniform1f(resources.blur.centerWeight, plan.centerWeight);
  glUniform1f(resources.blur.pairCount,
              static_cast<float>(plan.pairCount));
  glUniform1f(resources.blur.passKind, passKind);
  glUniform1fv(resources.blur.pairWeights, kGlassGaussianPairCapacity,
               plan.pairWeights.data());
  glUniform1fv(resources.blur.pairOffsets, kGlassGaussianPairCapacity,
               plan.pairOffsets.data());
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

}
bool renderGlassBlur(Resources &resources, GLuint source,
                     const GlassBlurPlan &plan) {
  renderPass(resources, source, resources.pingFramebuffer,
             resources.width, resources.height, 0.0f, 0.0f, 0.0f, plan);
  renderPass(resources, resources.pingTexture, resources.pongFramebuffer,
             resources.blurWidth, resources.blurHeight, 1.0f, 1.0f, 0.0f,
             plan);
  renderPass(resources, resources.pongTexture, resources.pingFramebuffer,
             resources.blurWidth, resources.blurHeight, 1.0f, 0.0f, 1.0f,
             plan);
  return glGetError() == GL_NO_ERROR;
}

}
