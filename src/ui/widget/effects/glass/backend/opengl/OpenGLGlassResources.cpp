#include "OpenGLGlassResources.hpp"

#include "generated/opengl_blur_frag.hpp"
#include "generated/opengl_blur_kernel.hpp"
#include "generated/opengl_fullscreen_vert.hpp"
#include "generated/opengl_glass_color.hpp"
#include "generated/opengl_glass_frag.hpp"
#include "generated/opengl_glass_lighting.hpp"
#include "generated/opengl_glass_math.hpp"
#include "generated/opengl_glass_vert.hpp"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <string>

namespace glass_ui::widget::opengl_glass {
namespace {

template <size_t Size> std::string text(const unsigned char (&bytes)[Size]) {
  return std::string(reinterpret_cast<const char *>(bytes), Size);
}

bool replaceMarker(std::string &source, const char *marker,
                   const std::string &replacement) {
  const size_t position = source.find(marker);
  if (position == std::string::npos)
    return false;
  source.replace(position, std::char_traits<char>::length(marker), replacement);
  return true;
}

GLuint compile(GLenum type, const std::string &source, const char *name) {
  const GLuint shader = glCreateShader(type);
  const char *data = source.data();
  const GLint length = static_cast<GLint>(source.size());
  glShaderSource(shader, 1, &data, &length);
  glCompileShader(shader);
  GLint success = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_TRUE)
    return shader;
  GLint logLength = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  std::string message(static_cast<size_t>(std::max(logLength, 1)), '\0');
  glGetShaderInfoLog(shader, logLength, nullptr, message.data());
  log::error("OpenGL shader compile failed (%s): %s", name, message.c_str());
  glDeleteShader(shader);
  return 0;
}

GLuint link(const std::string &vertexSource, const std::string &fragmentSource,
            const char *name) {
  const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource, name);
  const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource, name);
  if (!vertex || !fragment) {
    if (vertex)
      glDeleteShader(vertex);
    if (fragment)
      glDeleteShader(fragment);
    return 0;
  }
  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint success = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == GL_TRUE)
    return program;
  GLint logLength = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  std::string message(static_cast<size_t>(std::max(logLength, 1)), '\0');
  glGetProgramInfoLog(program, logLength, nullptr, message.data());
  log::error("OpenGL program link failed (%s): %s", name, message.c_str());
  glDeleteProgram(program);
  return 0;
}

bool createTarget(uint32_t width, uint32_t height, GLuint &texture,
                  GLuint &framebuffer) {
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height));
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

} // namespace
void destroyResources(Resources &resources) noexcept {
  if (resources.blurProgram)
    glDeleteProgram(resources.blurProgram);
  if (resources.glassProgram)
    glDeleteProgram(resources.glassProgram);
  if (resources.vertexArray)
    glDeleteVertexArrays(1, &resources.vertexArray);
  const GLuint textures[] = {resources.captureTexture,
                             resources.contentCaptureTexture,
                             resources.pingTexture, resources.pongTexture};
  glDeleteTextures(4, textures);
  const GLuint framebuffers[] = {
      resources.captureFramebuffer, resources.contentCaptureFramebuffer,
      resources.pingFramebuffer, resources.pongFramebuffer};
  glDeleteFramebuffers(4, framebuffers);
  resources = {};
}

bool createResources(EGLContext context, uint32_t width, uint32_t height,
                     Resources &resources) {
  destroyResources(resources);
  if (context == EGL_NO_CONTEXT || width < 2 || height < 2)
    return false;

  resources.context = context;
  resources.width = width;
  resources.height = height;
  resources.blurWidth = std::max(1u, (width + 1u) / 2u);
  resources.blurHeight = std::max(1u, (height + 1u) / 2u);

  std::string blurFragment = text(kOpenGLBlurFrag);
  std::string glassFragment = text(kOpenGLGlassFrag);
  if (!replaceMarker(blurFragment,
                     "// The runtime shader loader prepends "
                     "common/blur_kernel.glslinc here.",
                     text(kOpenGLBlurKernel)) ||
      !replaceMarker(glassFragment, "// Inject common glass modules here.",
                     text(kOpenGLGlassMath) + "\n" + text(kOpenGLGlassColor) +
                         "\n" + text(kOpenGLGlassLighting))) {
    log::error("OpenGL glass shader markers are missing");
    destroyResources(resources);
    return false;
  }

  resources.blurProgram =
      link(text(kOpenGLFullscreenVert), blurFragment, "glass blur");
  resources.glassProgram =
      link(text(kOpenGLGlassVert), glassFragment, "liquid glass");
  if (!resources.blurProgram || !resources.glassProgram ||
      !createTarget(width, height, resources.captureTexture,
                    resources.captureFramebuffer) ||
      !createTarget(width, height, resources.contentCaptureTexture,
                    resources.contentCaptureFramebuffer) ||
      !createTarget(resources.blurWidth, resources.blurHeight,
                    resources.pingTexture, resources.pingFramebuffer) ||
      !createTarget(resources.blurWidth, resources.blurHeight,
                    resources.pongTexture, resources.pongFramebuffer)) {
    log::error("OpenGL glass resources could not be created");
    destroyResources(resources);
    return false;
  }

  glGenVertexArrays(1, &resources.vertexArray);
  resources.blur.source =
      glGetUniformLocation(resources.blurProgram, "uSource");
  resources.blur.inverseSourceResolution =
      glGetUniformLocation(resources.blurProgram, "uInverseSourceResolution");
  resources.blur.direction =
      glGetUniformLocation(resources.blurProgram, "uDirection");
  resources.blur.centerWeight =
      glGetUniformLocation(resources.blurProgram, "uCenterWeight");
  resources.blur.pairCount =
      glGetUniformLocation(resources.blurProgram, "uPairCount");
  resources.blur.passKind =
      glGetUniformLocation(resources.blurProgram, "uPassKind");
  resources.blur.pairWeights =
      glGetUniformLocation(resources.blurProgram, "uPairWeights");
  resources.blur.pairOffsets =
      glGetUniformLocation(resources.blurProgram, "uPairOffsets");

  resources.glass.backdrop =
      glGetUniformLocation(resources.glassProgram, "uBackdrop");
  resources.glass.rawBackdrop =
      glGetUniformLocation(resources.glassProgram, "uRawBackdrop");
  resources.glass.bounds =
      glGetUniformLocation(resources.glassProgram, "uBounds");
  resources.glass.radii =
      glGetUniformLocation(resources.glassProgram, "uRadii");
  resources.glass.refraction =
      glGetUniformLocation(resources.glassProgram, "uRefraction");
  resources.glass.filter =
      glGetUniformLocation(resources.glassProgram, "uFilter");
  resources.glass.tint = glGetUniformLocation(resources.glassProgram, "uTint");
  resources.glass.framebuffer =
      glGetUniformLocation(resources.glassProgram, "uFramebuffer");
  resources.glass.deformation =
      glGetUniformLocation(resources.glassProgram, "uDeformation");
  resources.glass.lighting =
      glGetUniformLocation(resources.glassProgram, "uLighting");
  resources.glass.light =
      glGetUniformLocation(resources.glassProgram, "uLight");
  resources.glass.framebufferSize =
      glGetUniformLocation(resources.glassProgram, "uFramebufferSize");
  resources.prepared = false;
  return glGetError() == GL_NO_ERROR;
}

} // namespace glass_ui::widget::opengl_glass
