#include "GraphicsBootstrap.hpp"

#include "core/graphics/opengl/EglHooks.hpp"
#include "core/graphics/vulkan/VulkanHooks.hpp"
#include "ui/renderer/opengl/OpenGLRenderer.hpp"
#include "ui/renderer/vulkan/VulkanRenderer.hpp"
#include "utils/LogUtils.hpp"

namespace glass_ui::graphics {

const char *backendName(GraphicsBackend backend) noexcept {
  switch (backend) {
  case GraphicsBackend::Vulkan:
    return "vulkan";
  case GraphicsBackend::OpenGLES:
    return "opengl-es";
  }
  return "unknown";
}

bool initialize(GraphicsBackend backend, InputBridge &input) noexcept {
  log::info("initializing explicit graphics backend: %s",
            backendName(backend));
  switch (backend) {
  case GraphicsBackend::Vulkan:
    renderer::vulkan::initialize(input);
    return hooks::vulkan::initialize();
  case GraphicsBackend::OpenGLES:
    renderer::opengl::initialize(input);
    return hooks::opengl::initialize();
  }
  return false;
}

}
