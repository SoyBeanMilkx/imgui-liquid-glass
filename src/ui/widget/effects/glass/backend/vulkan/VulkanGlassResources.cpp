#include "VulkanGlassResources.hpp"

#include "VulkanGlassBackend.hpp"
#include "VulkanGlassBlurPass.hpp"
#include "generated/blur_frag_spv.hpp"
#include "generated/fullscreen_vert_spv.hpp"
#include "generated/liquid_glass_frag_spv.hpp"
#include "generated/liquid_glass_vert_spv.hpp"

#include "utils/LogUtils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace glass_ui::widget::vulkan_glass {

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept {
  return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
}

bool check(const char *operation, VkResult result) {
  if (result == VK_SUCCESS)
    return true;
  log::error("%s failed: VkResult=%d", operation, result);
  return false;
}

bool isSrgbFormat(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_SRGB:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_SRGB:
  case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    return true;
  default:
    return false;
  }
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t bits,
                        VkMemoryPropertyFlags required) {
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
  for (uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
    if ((bits & (1u << index)) != 0 &&
        (properties.memoryTypes[index].propertyFlags & required) == required)
      return index;
  }
  return std::numeric_limits<uint32_t>::max();
}

void destroyImage(VkDevice device, ImageResource &resource) {
  if (resource.framebuffer)
    vkDestroyFramebuffer(device, resource.framebuffer, nullptr);
  if (resource.view)
    vkDestroyImageView(device, resource.view, nullptr);
  if (resource.image)
    vkDestroyImage(device, resource.image, nullptr);
  if (resource.memory)
    vkFreeMemory(device, resource.memory, nullptr);
  resource = {};
}

bool createImage(VkPhysicalDevice physicalDevice, VkDevice device,
                 VkFormat format, VkExtent2D extent,
                 VkRenderPass blurRenderPass, ImageResource &resource) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = format;
  imageInfo.extent = VkExtent3D{extent.width, extent.height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (!check("vkCreateImage(glass)",
             vkCreateImage(device, &imageInfo, nullptr, &resource.image)))
    return false;

  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(device, resource.image, &requirements);
  const uint32_t memoryType =
      findMemoryType(physicalDevice, requirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (memoryType == std::numeric_limits<uint32_t>::max()) {
    log::error("no device-local memory type for glass image");
    return false;
  }

  VkMemoryAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = requirements.size;
  allocateInfo.memoryTypeIndex = memoryType;
  if (!check(
          "vkAllocateMemory(glass)",
          vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory)) ||
      !check("vkBindImageMemory(glass)",
             vkBindImageMemory(device, resource.image, resource.memory, 0)))
    return false;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = resource.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.layerCount = 1;
  if (!check("vkCreateImageView(glass)",
             vkCreateImageView(device, &viewInfo, nullptr, &resource.view)))
    return false;

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = blurRenderPass;
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.pAttachments = &resource.view;
  framebufferInfo.width = extent.width;
  framebufferInfo.height = extent.height;
  framebufferInfo.layers = 1;
  if (!check("vkCreateFramebuffer(glass)",
             vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                 &resource.framebuffer)))
    return false;
  resource.extent = extent;
  return true;
}

bool createBlurRenderPass(VkDevice device, VkFormat format,
                          VkRenderPass *renderPass) {
  VkAttachmentDescription color{};
  color.format = format;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference reference{};
  reference.attachment = 0;
  reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &reference;

  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &color;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 2;
  info.pDependencies = dependencies;
  return check("vkCreateRenderPass(glass blur)",
               vkCreateRenderPass(device, &info, nullptr, renderPass));
}

bool createShader(VkDevice device, const unsigned char *bytes, size_t size,
                  VkShaderModule *shader) {
  if ((size % sizeof(uint32_t)) != 0)
    return false;
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = size;
  info.pCode = reinterpret_cast<const uint32_t *>(bytes);
  return check("vkCreateShaderModule(glass)",
               vkCreateShaderModule(device, &info, nullptr, shader));
}

bool createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptor,
                          VkShaderStageFlags stages, uint32_t pushSize,
                          VkPipelineLayout *layout) {
  VkPushConstantRange push{};
  push.stageFlags = stages;
  push.size = pushSize;

  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 1;
  info.pSetLayouts = &descriptor;
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &push;
  return check("vkCreatePipelineLayout(glass)",
               vkCreatePipelineLayout(device, &info, nullptr, layout));
}

bool createPipeline(VkDevice device, VkRenderPass renderPass,
                    VkPipelineLayout layout, const unsigned char *vertexBytes,
                    size_t vertexSize, const unsigned char *fragmentBytes,
                    size_t fragmentSize, bool blend, VkPipeline *pipeline) {
  VkShaderModule vertex = VK_NULL_HANDLE;
  VkShaderModule fragment = VK_NULL_HANDLE;
  if (!createShader(device, vertexBytes, vertexSize, &vertex) ||
      !createShader(device, fragmentBytes, fragmentSize, &fragment)) {
    if (vertex)
      vkDestroyShaderModule(device, vertex, nullptr);
    if (fragment)
      vkDestroyShaderModule(device, fragment, nullptr);
    return false;
  }

  const VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
       VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr},
  };
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.blendEnable = blend ? VK_TRUE : VK_FALSE;
  blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo colorBlend{};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = 1;
  colorBlend.pAttachments = &blendAttachment;

  const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                          VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;

  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.stageCount = 2;
  info.pStages = stages;
  info.pVertexInputState = &vertexInput;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pColorBlendState = &colorBlend;
  info.pDynamicState = &dynamic;
  info.layout = layout;
  info.renderPass = renderPass;
  info.subpass = 0;
  const bool success =
      check("vkCreateGraphicsPipelines(glass)",
            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                                      pipeline));
  vkDestroyShaderModule(device, fragment, nullptr);
  vkDestroyShaderModule(device, vertex, nullptr);
  return success;
}

void imageBarrier(VkCommandBuffer commandBuffer, VkImage image,
                  VkImageLayout oldLayout, VkImageLayout newLayout,
                  VkPipelineStageFlags sourceStage,
                  VkPipelineStageFlags destinationStage,
                  VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

Resources::~Resources() { destroy(); }

void Resources::destroy() {
  if (!device)
    return;
  vkDeviceWaitIdle(device);
  for (PerFrameResources &frame : frames) {
    destroyImage(device, frame.capture);
    destroyImage(device, frame.contentCapture);
    for (ImageResource &blur : frame.blur)
      destroyImage(device, blur);
  }
  if (glassPipeline)
    vkDestroyPipeline(device, glassPipeline, nullptr);
  if (blurPipeline)
    vkDestroyPipeline(device, blurPipeline, nullptr);
  if (glassPipelineLayout)
    vkDestroyPipelineLayout(device, glassPipelineLayout, nullptr);
  if (blurPipelineLayout)
    vkDestroyPipelineLayout(device, blurPipelineLayout, nullptr);
  if (descriptorPool)
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
  if (descriptorLayout)
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
  if (sampler)
    vkDestroySampler(device, sampler, nullptr);
  if (blurRenderPass)
    vkDestroyRenderPass(device, blurRenderPass, nullptr);
  frames.clear();
  device = VK_NULL_HANDLE;
}

bool createResources(const VulkanGlassCreateInfo &info, Resources &resources) {
  if (!info.sourceTransferSupported || !info.physicalDevice || !info.device ||
      !info.swapchain || !info.targetRenderPass || !info.imageCount ||
      !info.extent.width || !info.extent.height)
    return false;

  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(info.physicalDevice, info.format,
                                      &properties);
  constexpr VkFormatFeatureFlags required =
      VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
  if ((properties.optimalTilingFeatures & required) != required) {
    log::warn("swapchain format %d lacks glass image features: 0x%x",
              static_cast<int>(info.format), properties.optimalTilingFeatures);
    return false;
  }

  resources.physicalDevice = info.physicalDevice;
  resources.device = info.device;
  resources.swapchain = info.swapchain;
  resources.format = info.format;
  resources.srgbFormat = isSrgbFormat(info.format);
  resources.extent = info.extent;
  resources.blurExtent = {
      std::max(1u, info.extent.width / 2u),
      std::max(1u, info.extent.height / 2u),
  };
  resources.targetRenderPass = info.targetRenderPass;

  if (!createBlurRenderPass(info.device, info.format,
                            &resources.blurRenderPass))
    return false;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
  for (uint32_t index = 0; index < bindings.size(); ++index) {
    bindings[index].binding = index;
    bindings[index].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[index].descriptorCount = 1;
    bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  VkDescriptorSetLayoutCreateInfo descriptorInfo{};
  descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  descriptorInfo.pBindings = bindings.data();
  if (!check("vkCreateDescriptorSetLayout(glass)",
             vkCreateDescriptorSetLayout(info.device, &descriptorInfo, nullptr,
                                         &resources.descriptorLayout)) ||
      !createPipelineLayout(info.device, resources.descriptorLayout,
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            glassBlurPushConstantSize(),
                            &resources.blurPipelineLayout) ||
      !createPipelineLayout(info.device, resources.descriptorLayout,
                            VK_SHADER_STAGE_VERTEX_BIT |
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                            sizeof(GlassPush), &resources.glassPipelineLayout))
    return false;

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.maxLod = 0.0f;
  if (!check("vkCreateSampler(glass)",
             vkCreateSampler(info.device, &samplerInfo, nullptr,
                             &resources.sampler)) ||
      !createPipeline(info.device, resources.blurRenderPass,
                      resources.blurPipelineLayout, kFullscreenVertSpv,
                      sizeof(kFullscreenVertSpv), kBlurFragSpv,
                      sizeof(kBlurFragSpv), false, &resources.blurPipeline) ||
      !createPipeline(
          info.device, info.targetRenderPass, resources.glassPipelineLayout,
          kLiquidGlassVertSpv, sizeof(kLiquidGlassVertSpv), kLiquidGlassFragSpv,
          sizeof(kLiquidGlassFragSpv), true, &resources.glassPipeline))
    return false;

  resources.frames.resize(info.imageCount);
  for (PerFrameResources &frame : resources.frames) {
    if (!createImage(info.physicalDevice, info.device, info.format,
                     resources.extent, resources.blurRenderPass,
                     frame.capture) ||
        !createImage(info.physicalDevice, info.device, info.format,
                     resources.extent, resources.blurRenderPass,
                     frame.contentCapture))
      return false;
    for (ImageResource &blur : frame.blur) {
      if (!createImage(info.physicalDevice, info.device, info.format,
                       resources.blurExtent, resources.blurRenderPass, blur))
        return false;
    }
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount =
      info.imageCount * kDescriptorSetsPerFrame * bindings.size();
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets = info.imageCount * kDescriptorSetsPerFrame;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  if (!check("vkCreateDescriptorPool(glass)",
             vkCreateDescriptorPool(info.device, &poolInfo, nullptr,
                                    &resources.descriptorPool)))
    return false;

  std::vector<VkDescriptorSetLayout> layouts(poolInfo.maxSets,
                                             resources.descriptorLayout);
  std::vector<VkDescriptorSet> sets(poolInfo.maxSets);
  VkDescriptorSetAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.descriptorPool = resources.descriptorPool;
  allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  allocateInfo.pSetLayouts = layouts.data();
  if (!check("vkAllocateDescriptorSets(glass)",
             vkAllocateDescriptorSets(info.device, &allocateInfo, sets.data())))
    return false;

  for (uint32_t index = 0; index < info.imageCount; ++index) {
    PerFrameResources &frame = resources.frames[index];
    const uint32_t base = index * kDescriptorSetsPerFrame;
    frame.captureSet = sets[base];
    frame.contentCaptureSet = sets[base + kDescriptorSetsPerSource];
    for (uint32_t blur = 0; blur < kLiquidGlassBlurImageCount; ++blur) {
      frame.blurSets[blur] = sets[base + 1 + blur];
      frame.contentBlurSets[blur] =
          sets[base + kDescriptorSetsPerSource + 1 + blur];
    }

    std::array<VkDescriptorImageInfo, kDescriptorSetsPerFrame> sources{};
    std::array<VkDescriptorImageInfo, kDescriptorSetsPerFrame> rawSources{};
    const std::array<VkImageView, 2> rawViews = {
        frame.capture.view, frame.contentCapture.view};
    for (uint32_t sourceIndex = 0; sourceIndex < rawViews.size();
         ++sourceIndex) {
      const uint32_t group = sourceIndex * kDescriptorSetsPerSource;
      sources[group] = {resources.sampler, rawViews[sourceIndex],
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      for (uint32_t blur = 0; blur < kLiquidGlassBlurImageCount; ++blur) {
        sources[group + 1 + blur] = {
            resources.sampler, frame.blur[blur].view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      }
      for (uint32_t set = 0; set < kDescriptorSetsPerSource; ++set) {
        rawSources[group + set] = {
            resources.sampler, rawViews[sourceIndex],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      }
    }

    std::array<VkWriteDescriptorSet, kDescriptorSetsPerFrame * 2> writes{};
    for (uint32_t setIndex = 0; setIndex < kDescriptorSetsPerFrame;
         ++setIndex) {
      writes[setIndex * 2] = {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
          sets[base + setIndex], 0, 0, 1,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sources[setIndex],
          nullptr, nullptr};
      writes[setIndex * 2 + 1] = {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
          sets[base + setIndex], 1, 0, 1,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          &rawSources[setIndex], nullptr, nullptr};
    }
    vkUpdateDescriptorSets(info.device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  log::info("glass Vulkan backend ready: %ux%u -> %ux%u frames=%u",
            info.extent.width, info.extent.height,
            resources.blurExtent.width, resources.blurExtent.height,
            info.imageCount);
  return true;
}

}
