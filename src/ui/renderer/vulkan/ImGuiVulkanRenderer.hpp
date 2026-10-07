#pragma once

#include "VulkanRendererTypes.hpp"
#include "ui/renderer/OverlayFrameRuntime.hpp"

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::renderer::vulkan {

class ImGuiVulkanRenderer final {
public:
  void initialize(InputBridge &input) noexcept;
  bool ensure(const DeviceState &deviceState, const QueueState &queueState,
              const SwapchainResources &swapchain, VkQueue queue);
  void draw(const SwapchainResources &swapchain);
  void commitInput() noexcept;
  void suspendInput() noexcept;
  void shutdown();
  void setEffectCapabilities(widget::EffectCapabilities capabilities) noexcept {
    runtime_.setEffectCapabilities(capabilities);
  }
  const widget::GlassRequestQueue &glassRequests() const noexcept {
    return runtime_.glassRequests();
  }

  bool uses(VkDevice device) const noexcept { return device_ == device; }

private:
  OverlayFrameRuntime runtime_;
  VkDevice device_ = VK_NULL_HANDLE;
  bool ready_ = false;
};

}
