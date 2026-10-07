#include "SwapchainRenderer.hpp"

#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "ui/widget/effects/glass/backend/vulkan/VulkanGlassBackend.hpp"
#include "ui/widget/effects/glass/backend/vulkan/VulkanGlassDrawCallbacks.hpp"
#include "utils/LogUtils.hpp"

#include <vector>

namespace glass_ui::renderer::vulkan {
namespace {

constexpr uint64_t kFenceTimeoutNs = 100ull * 1000ull * 1000ull;

bool check(const char *operation, VkResult result) {
  if (result == VK_SUCCESS)
    return true;
  log::error("%s failed: VkResult=%d", operation, result);
  return false;
}

VkImageLayout presentLayout(VkPresentModeKHR mode) {
  if (mode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
      mode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
    return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
  return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

bool createRenderPass(VkDevice device, VkFormat format,
                      VkImageLayout finalLayout, VkRenderPass *renderPass) {
  VkAttachmentDescription color{};
  color.format = format;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = finalLayout;
  color.finalLayout = finalLayout;

  VkAttachmentReference colorReference{};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorReference;

  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &color;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 2;
  info.pDependencies = dependencies;
  return check("vkCreateRenderPass",
               vkCreateRenderPass(device, &info, nullptr, renderPass));
}

bool createFrameResources(const SwapchainState &state,
                          SwapchainResources &resources, FrameResources &frame,
                          VkImage image) {
  frame.image = image;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = state.format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.layerCount = 1;
  if (!check("vkCreateImageView", vkCreateImageView(state.device, &viewInfo,
                                                    nullptr, &frame.imageView)))
    return false;

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = resources.renderPass;
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.pAttachments = &frame.imageView;
  framebufferInfo.width = state.extent.width;
  framebufferInfo.height = state.extent.height;
  framebufferInfo.layers = 1;
  if (!check("vkCreateFramebuffer",
             vkCreateFramebuffer(state.device, &framebufferInfo, nullptr,
                                 &frame.framebuffer)))
    return false;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  if (!check("vkCreateFence",
             vkCreateFence(state.device, &fenceInfo, nullptr, &frame.fence)))
    return false;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  return check("vkCreateSemaphore",
               vkCreateSemaphore(state.device, &semaphoreInfo, nullptr,
                                 &frame.renderDone));
}

}
void destroySwapchainResources(SwapchainResources &resources) {
  if (resources.device == VK_NULL_HANDLE)
    return;

  vkDeviceWaitIdle(resources.device);
  for (FrameResources &frame : resources.frames) {
    if (frame.framebuffer)
      vkDestroyFramebuffer(resources.device, frame.framebuffer, nullptr);
    if (frame.imageView)
      vkDestroyImageView(resources.device, frame.imageView, nullptr);
    if (frame.fence)
      vkDestroyFence(resources.device, frame.fence, nullptr);
    if (frame.renderDone)
      vkDestroySemaphore(resources.device, frame.renderDone, nullptr);
  }
  if (resources.commandPool)
    vkDestroyCommandPool(resources.device, resources.commandPool, nullptr);
  if (resources.renderPass)
    vkDestroyRenderPass(resources.device, resources.renderPass, nullptr);
  resources = {};
}

bool buildSwapchainResources(VkSwapchainKHR handle, const SwapchainState &state,
                             uint32_t queueFamily,
                             SwapchainResources &resources) {
  const bool matches =
      resources.handle == handle && resources.device == state.device &&
      resources.format == state.format &&
      resources.extent.width == state.extent.width &&
      resources.extent.height == state.extent.height &&
      resources.preTransform == state.preTransform &&
      resources.renderPass != VK_NULL_HANDLE && !resources.frames.empty();
  if (matches)
    return true;

  destroySwapchainResources(resources);
  resources.device = state.device;
  resources.handle = handle;
  resources.format = state.format;
  resources.extent = state.extent;
  resources.presentMode = state.presentMode;
  resources.preTransform = state.preTransform;
  const auto fail = [&resources] {
    destroySwapchainResources(resources);
    return false;
  };

  uint32_t imageCount = 0;
  VkResult result =
      vkGetSwapchainImagesKHR(state.device, handle, &imageCount, nullptr);
  if (!check("vkGetSwapchainImagesKHR(count)", result) || imageCount == 0)
    return fail();

  resources.images.resize(imageCount);
  result = vkGetSwapchainImagesKHR(state.device, handle, &imageCount,
                                   resources.images.data());
  if (!check("vkGetSwapchainImagesKHR(images)", result))
    return fail();

  if (!createRenderPass(state.device, state.format,
                        presentLayout(state.presentMode),
                        &resources.renderPass))
    return fail();

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamily;
  if (!check("vkCreateCommandPool",
             vkCreateCommandPool(state.device, &poolInfo, nullptr,
                                 &resources.commandPool)))
    return fail();

  resources.frames.resize(imageCount);
  for (uint32_t index = 0; index < imageCount; ++index) {
    if (!createFrameResources(state, resources, resources.frames[index],
                              resources.images[index]))
      return fail();
  }

  {
    std::vector<VkCommandBuffer> commandBuffers(imageCount);
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = resources.commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = imageCount;
    if (!check("vkAllocateCommandBuffers",
               vkAllocateCommandBuffers(state.device, &allocateInfo,
                                        commandBuffers.data())))
      return fail();
    for (uint32_t index = 0; index < imageCount; ++index)
      resources.frames[index].commandBuffer = commandBuffers[index];
  }

  log::info("overlay swapchain %p %ux%u format=%d images=%u", (void *)handle,
            state.extent.width, state.extent.height, (int)state.format,
            imageCount);
  return true;
}

bool submitOverlay(VkQueue queue, const SwapchainResources &resources,
                   const FrameResources &frame, uint32_t imageIndex,
                   const VkSemaphore *waitSemaphores, uint32_t waitCount,
                   widget::VulkanGlassBackend *glass,
                   const widget::GlassRequestQueue &glassRequests,
                   uint64_t frameNumber) {
  VkResult result = vkWaitForFences(resources.device, 1, &frame.fence, VK_TRUE,
                                    kFenceTimeoutNs);
  if (!check("vkWaitForFences", result))
    return false;
  if (!check("vkResetFences", vkResetFences(resources.device, 1, &frame.fence)))
    return false;
  if (!check("vkResetCommandBuffer",
             vkResetCommandBuffer(frame.commandBuffer, 0)))
    return false;

  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (!check("vkBeginCommandBuffer",
             vkBeginCommandBuffer(frame.commandBuffer, &begin)))
    return false;

  const widget::VulkanGlassFrameInfo glassFrame{
      frame.commandBuffer,
      frame.image,
      resources.handle,
      resources.renderPass,
      frame.framebuffer,
      resources.extent,
      presentLayout(resources.presentMode),
      resources.preTransform,
      imageIndex,
      frameNumber};
  const bool glassPrepared =
      glass && glass->prepareBackdrop(glassFrame, glassRequests);

  ImDrawData *drawData = ImGui::GetDrawData();
  widget::VulkanGlassDrawCallbacks glassCallbacks;
  if (glassPrepared && drawData)
    glassCallbacks.inject(*drawData, glassRequests, glassFrame, *glass);

  VkRenderPassBeginInfo renderPass{};
  renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPass.renderPass = resources.renderPass;
  renderPass.framebuffer = frame.framebuffer;
  renderPass.renderArea.extent = resources.extent;
  vkCmdBeginRenderPass(frame.commandBuffer, &renderPass,
                       VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(drawData, frame.commandBuffer);
  glassCallbacks.clear();
  if (glassPrepared)
    glass->finishFrame(glassFrame);
  vkCmdEndRenderPass(frame.commandBuffer);

  if (!check("vkEndCommandBuffer", vkEndCommandBuffer(frame.commandBuffer)))
    return false;

  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  const uint32_t validWaitCount = waitSemaphores ? waitCount : 0;
  std::vector<VkPipelineStageFlags> waitStages(
      validWaitCount, glassPrepared
                          ? VK_PIPELINE_STAGE_TRANSFER_BIT
                          : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  submit.waitSemaphoreCount = validWaitCount;
  submit.pWaitSemaphores = waitSemaphores;
  submit.pWaitDstStageMask = waitStages.data();
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &frame.commandBuffer;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &frame.renderDone;
  return check("vkQueueSubmit", vkQueueSubmit(queue, 1, &submit, frame.fence));
}

}
