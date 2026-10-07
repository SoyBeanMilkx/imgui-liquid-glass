#pragma once

#include "VulkanRendererTypes.hpp"

#include <cstdint>

struct ImDrawData;

namespace glass_ui::renderer::vulkan {

VkExtent2D logicalExtent(const SwapchainResources &swapchain) noexcept;
uint32_t inputTransform(VkSurfaceTransformFlagBitsKHR transform) noexcept;
void applySurfaceTransform(ImDrawData *drawData,
                           const SwapchainResources &swapchain);

}
