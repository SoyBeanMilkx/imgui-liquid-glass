#pragma once

#include "ui/widget/effects/EffectCapabilities.hpp"
#include "ui/widget/effects/glass/GlassRequestQueue.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace glass_ui::widget {

namespace vulkan_glass {
struct Resources;
}

struct VulkanGlassCreateInfo {
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
  VkRenderPass targetRenderPass = VK_NULL_HANDLE;
  uint32_t imageCount = 0;
  bool sourceTransferSupported = false;
};

struct VulkanGlassFrameInfo {
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkImage swapchainImage = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkRenderPass targetRenderPass = VK_NULL_HANDLE;
  VkFramebuffer targetFramebuffer = VK_NULL_HANDLE;
  VkExtent2D extent{};
  VkImageLayout presentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkSurfaceTransformFlagBitsKHR preTransform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  uint32_t imageIndex = 0;
  uint64_t frameNumber = 0;
};

class VulkanGlassBackend final {
public:
  VulkanGlassBackend();
  ~VulkanGlassBackend();

  VulkanGlassBackend(const VulkanGlassBackend &) = delete;
  VulkanGlassBackend &operator=(const VulkanGlassBackend &) = delete;

  bool ensure(const VulkanGlassCreateInfo &info);
  bool available(VkSwapchainKHR swapchain) const noexcept;
  EffectCapabilities capabilities(VkSwapchainKHR swapchain) const noexcept;

  bool prepareBackdrop(const VulkanGlassFrameInfo &frame,
                       const GlassRequestQueue &queue);
  bool prepareRequestBackdrop(const VulkanGlassFrameInfo &frame,
                              GlassBackdropSource source);
  void composite(const VulkanGlassFrameInfo &frame,
                 const GlassRequest &request);
  void finishFrame(const VulkanGlassFrameInfo &frame);

  void removeSwapchain(VkSwapchainKHR swapchain);
  void removeDevice(VkDevice device);
  void shutdown();

private:
  std::unordered_map<VkSwapchainKHR, std::unique_ptr<vulkan_glass::Resources>>
      resources_;
  std::unordered_set<VkSwapchainKHR> disabled_;
};

}
