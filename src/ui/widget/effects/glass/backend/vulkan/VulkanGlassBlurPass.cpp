#include "VulkanGlassBlurPass.hpp"

#include "VulkanGlassResources.hpp"

#include <algorithm>
#include <iterator>

namespace glass_ui::widget::vulkan_glass {
namespace {

struct BlurPush {
  float inverseSourceResolution[2];
  float direction[2];
  float centerWeight;
  float pairCount;
  float passKind;
  float padding;
  float pairWeights[kGlassGaussianPairCapacity];
  float pairOffsets[kGlassGaussianPairCapacity];
};

static_assert(sizeof(BlurPush) == 128);

void beginBlurPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass,
                   VkFramebuffer framebuffer, VkRect2D region) {
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = renderPass;
  begin.framebuffer = framebuffer;
  begin.renderArea = region;
  vkCmdBeginRenderPass(commandBuffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
}

void drawBlur(VkCommandBuffer commandBuffer, const Resources &resources,
              VkDescriptorSet descriptorSet, VkExtent2D destinationExtent,
              VkExtent2D samplingResolution, const GlassBlurPlan &blur,
              float directionX, float directionY, float passKind) {
  VkViewport viewport{};
  viewport.width = static_cast<float>(destinationExtent.width);
  viewport.height = static_cast<float>(destinationExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  const VkRect2D region{{0, 0}, destinationExtent};
  vkCmdSetScissor(commandBuffer, 0, 1, &region);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    resources.blurPipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          resources.blurPipelineLayout, 0, 1, &descriptorSet, 0,
                          nullptr);

  BlurPush push{};
  push.inverseSourceResolution[0] = 1.0f / samplingResolution.width;
  push.inverseSourceResolution[1] = 1.0f / samplingResolution.height;
  push.direction[0] = directionX;
  push.direction[1] = directionY;
  push.centerWeight = blur.centerWeight;
  push.pairCount = static_cast<float>(blur.pairCount);
  push.passKind = passKind;
  std::copy(blur.pairWeights.begin(), blur.pairWeights.end(),
            std::begin(push.pairWeights));
  std::copy(blur.pairOffsets.begin(), blur.pairOffsets.end(),
            std::begin(push.pairOffsets));
  vkCmdPushConstants(commandBuffer, resources.blurPipelineLayout,
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void recordBlurPass(VkCommandBuffer commandBuffer, const Resources &resources,
                    ImageResource &destination, VkDescriptorSet sourceSet,
                    VkExtent2D samplingResolution,
                    const GlassBlurPlan &blur, float directionX,
                    float directionY, float passKind) {
  imageBarrier(commandBuffer, destination.image,
               destination.initialized
                   ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                   : VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               destination.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                       : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               destination.initialized ? VK_ACCESS_SHADER_READ_BIT : 0,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  const VkRect2D region{{0, 0}, destination.extent};
  beginBlurPass(commandBuffer, resources.blurRenderPass,
                destination.framebuffer, region);
  drawBlur(commandBuffer, resources, sourceSet, destination.extent,
           samplingResolution, blur, directionX, directionY, passKind);
  vkCmdEndRenderPass(commandBuffer);
  imageBarrier(commandBuffer, destination.image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
  destination.initialized = true;
}

}
uint32_t glassBlurPushConstantSize() noexcept { return sizeof(BlurPush); }

VkDescriptorSet recordGlassBlur(VkCommandBuffer commandBuffer,
                                const Resources &resources,
                                PerFrameResources &frame,
                                VkDescriptorSet sourceSet,
                                const VkDescriptorSet *blurSets,
                                VkExtent2D sourceExtent,
                                const GlassBlurPlan &blur) {
  if (blur.pairCount == 0)
    return sourceSet;

  // Downsample first, then blur the half-resolution texture on both axes.
  recordBlurPass(commandBuffer, resources, frame.blur[0], sourceSet,
                 sourceExtent, blur, 0.0f, 0.0f, 0.0f);
  recordBlurPass(commandBuffer, resources, frame.blur[1], blurSets[0],
                 frame.blur[0].extent, blur, 1.0f, 0.0f, 1.0f);
  recordBlurPass(commandBuffer, resources, frame.blur[0], blurSets[1],
                 frame.blur[1].extent, blur, 0.0f, 1.0f, 1.0f);
  return blurSets[0];
}

}
