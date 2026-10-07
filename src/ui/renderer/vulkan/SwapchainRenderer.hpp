#pragma once

#include "VulkanRendererTypes.hpp"
#include "ui/widget/effects/glass/GlassRequestQueue.hpp"

namespace glass_ui::widget {
class VulkanGlassBackend;
}

namespace glass_ui::renderer::vulkan {

void destroySwapchainResources(SwapchainResources &resources);
bool buildSwapchainResources(VkSwapchainKHR handle, const SwapchainState &state,
                             uint32_t queueFamily,
                             SwapchainResources &resources);
bool submitOverlay(VkQueue queue, const SwapchainResources &resources,
                   const FrameResources &frame, uint32_t imageIndex,
                   const VkSemaphore *waitSemaphores, uint32_t waitCount,
                   widget::VulkanGlassBackend *glass,
                   const widget::GlassRequestQueue &glassRequests,
                   uint64_t frameNumber);

}
