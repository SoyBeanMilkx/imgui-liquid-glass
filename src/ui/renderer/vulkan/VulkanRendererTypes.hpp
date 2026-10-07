#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace glass_ui::renderer::vulkan {

struct QueueState {
  VkDevice device = VK_NULL_HANDLE;
  uint32_t family = 0;
};

struct DeviceState {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  uint32_t apiVersion = VK_API_VERSION_1_0;
};

struct SwapchainState {
  VkDevice device = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
  VkImageUsageFlags usage = 0;
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  VkSurfaceTransformFlagBitsKHR preTransform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
};

struct FrameResources {
  VkImage image = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkSemaphore renderDone = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

struct SwapchainResources {
  VkDevice device = VK_NULL_HANDLE;
  VkSwapchainKHR handle = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  VkSurfaceTransformFlagBitsKHR preTransform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  std::vector<VkImage> images;
  std::vector<FrameResources> frames;
};

}
