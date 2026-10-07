#pragma once

#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace glass_ui::widget::vulkan_glass {

struct PerFrameResources;
struct Resources;

uint32_t glassBlurPushConstantSize() noexcept;

// Records shared blur passes, or returns the full-resolution capture when disabled.
VkDescriptorSet recordGlassBlur(VkCommandBuffer commandBuffer,
                                const Resources &resources,
                                PerFrameResources &frame,
                                VkDescriptorSet sourceSet,
                                const VkDescriptorSet *blurSets,
                                VkExtent2D sourceExtent,
                                const GlassBlurPlan &blur);

}
