#pragma once

#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"
#include "ui/widget/effects/glass/GlassRequest.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace glass_ui::widget {
struct VulkanGlassCreateInfo;
}

namespace glass_ui::widget::vulkan_glass {

constexpr uint32_t kLiquidGlassBlurImageCount = 2;
constexpr uint32_t kDescriptorSetsPerSource = kLiquidGlassBlurImageCount + 1;
constexpr uint32_t kDescriptorSetsPerFrame = kDescriptorSetsPerSource * 2;
constexpr uint32_t kMaxGlassRegions = 64;
constexpr float kMaximumBlurRadius = 50.0f;

struct ImageResource {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkExtent2D extent{};
  bool initialized = false;
};

struct PerFrameResources {
  ImageResource capture;
  ImageResource contentCapture;
  std::array<ImageResource, kLiquidGlassBlurImageCount> blur;
  VkDescriptorSet captureSet = VK_NULL_HANDLE;
  VkDescriptorSet contentCaptureSet = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, kLiquidGlassBlurImageCount> blurSets{};
  std::array<VkDescriptorSet, kLiquidGlassBlurImageCount> contentBlurSets{};
  VkDescriptorSet preparedBackdropSet = VK_NULL_HANDLE;
  GlassBlurPlan blurPlan{};
  GlassBackdropSource activeSource = GlassBackdropSource::Frame;
  bool prepared = false;
};

struct GlassPush {
  float bounds[4];
  float radii[4];
  float refraction[4];
  float filter[4];
  float tint[4];
  float framebuffer[4];
  float deformation[4];
  float lighting[4];
  float light[4];
};

static_assert(sizeof(GlassPush) == 144);

struct Resources {
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  bool srgbFormat = false;
  VkExtent2D extent{};
  VkExtent2D blurExtent{};
  VkRenderPass targetRenderPass = VK_NULL_HANDLE;
  VkRenderPass blurRenderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
  VkPipelineLayout blurPipelineLayout = VK_NULL_HANDLE;
  VkPipelineLayout glassPipelineLayout = VK_NULL_HANDLE;
  VkPipeline blurPipeline = VK_NULL_HANDLE;
  VkPipeline glassPipeline = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  std::vector<PerFrameResources> frames;

  ~Resources();
  void destroy();
};

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept;
bool check(const char *operation, VkResult result);
bool isSrgbFormat(VkFormat format) noexcept;
void destroyImage(VkDevice device, ImageResource &resource);
bool createImage(VkPhysicalDevice physicalDevice, VkDevice device,
                 VkFormat format, VkExtent2D extent,
                 VkRenderPass blurRenderPass, ImageResource &resource);
bool createBlurRenderPass(VkDevice device, VkFormat format,
                          VkRenderPass *renderPass);
bool createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptor,
                          VkShaderStageFlags stages, uint32_t pushSize,
                          VkPipelineLayout *layout);
bool createPipeline(VkDevice device, VkRenderPass renderPass,
                    VkPipelineLayout layout, const unsigned char *vertexBytes,
                    size_t vertexSize, const unsigned char *fragmentBytes,
                    size_t fragmentSize, bool blend, VkPipeline *pipeline);
void imageBarrier(VkCommandBuffer commandBuffer, VkImage image,
                  VkImageLayout oldLayout, VkImageLayout newLayout,
                  VkPipelineStageFlags sourceStage,
                  VkPipelineStageFlags destinationStage,
                  VkAccessFlags sourceAccess, VkAccessFlags destinationAccess);
bool createResources(const VulkanGlassCreateInfo &info, Resources &resources);

} // namespace glass_ui::widget::vulkan_glass
