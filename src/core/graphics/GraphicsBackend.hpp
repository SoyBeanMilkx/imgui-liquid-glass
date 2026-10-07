#pragma once

namespace glass_ui::graphics {

enum class GraphicsBackend {
  Vulkan,
  OpenGLES,
};

const char *backendName(GraphicsBackend backend) noexcept;

}
