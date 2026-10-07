#include "VulkanGlassBackend.hpp"
#include "VulkanGlassBlurPass.hpp"
#include "VulkanGlassGeometry.hpp"
#include "VulkanGlassResources.hpp"

#include "ui/widget/effects/glass/GlassBlurPolicy.hpp"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

namespace glass_ui::widget {
using vulkan_glass::clampFinite;
using vulkan_glass::createResources;
using vulkan_glass::GlassPush;
using vulkan_glass::imageBarrier;
using vulkan_glass::kMaxGlassRegions;
using vulkan_glass::kMaximumBlurRadius;
using vulkan_glass::PerFrameResources;
using vulkan_glass::recordGlassBlur;
using vulkan_glass::Resources;

namespace {

void captureBackdrop(const VulkanGlassFrameInfo &frame,
                     vulkan_glass::ImageResource &destination) {
  VkCommandBuffer commandBuffer = frame.commandBuffer;
  imageBarrier(commandBuffer, frame.swapchainImage, frame.presentLayout,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_READ_BIT);
  imageBarrier(commandBuffer, destination.image,
               destination.initialized
                   ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                   : VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               destination.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                       : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               destination.initialized ? VK_ACCESS_SHADER_READ_BIT : 0,
               VK_ACCESS_TRANSFER_WRITE_BIT);

  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.dstSubresource.layerCount = 1;
  copy.extent = VkExtent3D{frame.extent.width, frame.extent.height, 1};
  vkCmdCopyImage(commandBuffer, frame.swapchainImage,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  imageBarrier(commandBuffer, frame.swapchainImage,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frame.presentLayout,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_TRANSFER_READ_BIT, 0);
  imageBarrier(
      commandBuffer, destination.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT);
  destination.initialized = true;
}

void beginTargetPass(const VulkanGlassFrameInfo &frame) {
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = frame.targetRenderPass;
  begin.framebuffer = frame.targetFramebuffer;
  begin.renderArea.extent = frame.extent;
  vkCmdBeginRenderPass(frame.commandBuffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
}

} // namespace

VulkanGlassBackend::VulkanGlassBackend() = default;

VulkanGlassBackend::~VulkanGlassBackend() { shutdown(); }

bool VulkanGlassBackend::ensure(const VulkanGlassCreateInfo &info) {
  const auto current = resources_.find(info.swapchain);
  if (current != resources_.end()) {
    const Resources &resources = *current->second;
    if (resources.device == info.device && resources.format == info.format &&
        resources.extent.width == info.extent.width &&
        resources.extent.height == info.extent.height &&
        resources.targetRenderPass == info.targetRenderPass &&
        resources.frames.size() == info.imageCount)
      return true;
    resources_.erase(current);
  }
  if (disabled_.find(info.swapchain) != disabled_.end())
    return false;

  auto resources = std::make_unique<Resources>();
  if (!createResources(info, *resources)) {
    disabled_.insert(info.swapchain);
    log::warn("liquid glass unavailable for swapchain %p",
              reinterpret_cast<void *>(info.swapchain));
    return false;
  }
  resources_[info.swapchain] = std::move(resources);
  return true;
}

bool VulkanGlassBackend::available(VkSwapchainKHR swapchain) const noexcept {
  return resources_.find(swapchain) != resources_.end();
}

EffectCapabilities
VulkanGlassBackend::capabilities(VkSwapchainKHR swapchain) const noexcept {
  if (!available(swapchain))
    return {};
  return EffectCapabilities{true, true, true, true, kMaxGlassRegions};
}

bool VulkanGlassBackend::prepareBackdrop(const VulkanGlassFrameInfo &frame,
                                         const GlassRequestQueue &queue) {
  const std::vector<GlassRequest> &requests = queue.requests();
  const auto found = resources_.find(frame.swapchain);
  if (found == resources_.end() || requests.empty())
    return false;
  Resources &resources = *found->second;
  if (frame.imageIndex >= resources.frames.size() ||
      frame.presentLayout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR)
    return false;
  PerFrameResources &perFrame = resources.frames[frame.imageIndex];
  perFrame.prepared = false;
  perFrame.preparedBackdropSet = VK_NULL_HANDLE;
  if (!vulkan_glass::hasVisibleRequest(frame, requests))
    return false;
  const float physicalRadius =
      clampFinite(queue.backdropStyle().blurRadius * queue.density(), 0.0f,
                  kMaximumBlurRadius, 0.01f);
  const GlassBlurPlan blur =
      makeGlassBlurPlan(physicalRadius, queue.backdropStyle().quality);

  captureBackdrop(frame, perFrame.capture);
  perFrame.blurPlan = blur;
  perFrame.preparedBackdropSet = recordGlassBlur(
      frame.commandBuffer, resources, perFrame, perFrame.captureSet,
      perFrame.blurSets.data(), perFrame.capture.extent, blur);
  perFrame.activeSource = GlassBackdropSource::Frame;
  perFrame.prepared = true;
  return true;
}

bool VulkanGlassBackend::prepareRequestBackdrop(
    const VulkanGlassFrameInfo &frame, GlassBackdropSource source) {
  const auto found = resources_.find(frame.swapchain);
  if (found == resources_.end() ||
      frame.imageIndex >= found->second->frames.size())
    return false;
  Resources &resources = *found->second;
  PerFrameResources &perFrame = resources.frames[frame.imageIndex];
  if (!perFrame.prepared || !frame.targetRenderPass || !frame.targetFramebuffer)
    return false;
  if (source == GlassBackdropSource::Frame && perFrame.activeSource == source)
    return true;

  vkCmdEndRenderPass(frame.commandBuffer);
  VkDescriptorSet sourceSet = perFrame.captureSet;
  const VkDescriptorSet *blurSets = perFrame.blurSets.data();
  VkExtent2D sourceExtent = perFrame.capture.extent;
  if (source == GlassBackdropSource::PreviousContent) {
    captureBackdrop(frame, perFrame.contentCapture);
    sourceSet = perFrame.contentCaptureSet;
    blurSets = perFrame.contentBlurSets.data();
    sourceExtent = perFrame.contentCapture.extent;
  }
  perFrame.preparedBackdropSet =
      recordGlassBlur(frame.commandBuffer, resources, perFrame, sourceSet,
                      blurSets, sourceExtent, perFrame.blurPlan);
  perFrame.activeSource = source;
  beginTargetPass(frame);
  return perFrame.preparedBackdropSet != VK_NULL_HANDLE;
}

void VulkanGlassBackend::composite(const VulkanGlassFrameInfo &frame,
                                   const GlassRequest &request) {
  const auto found = resources_.find(frame.swapchain);
  if (found == resources_.end() ||
      frame.imageIndex >= found->second->frames.size())
    return;
  Resources &resources = *found->second;
  PerFrameResources &perFrame = resources.frames[frame.imageIndex];
  if (!perFrame.prepared || !perFrame.preparedBackdropSet)
    return;

  VkViewport viewport{};
  viewport.width = static_cast<float>(frame.extent.width);
  viewport.height = static_cast<float>(frame.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
  vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    resources.glassPipeline);
  const VkDescriptorSet glassSet = perFrame.preparedBackdropSet;
  vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          resources.glassPipelineLayout, 0, 1, &glassSet, 0,
                          nullptr);

  const vulkan_glass::PhysicalRequest physical =
      vulkan_glass::transformRequest(request, frame);
  const float width = physical.max.x - physical.min.x;
  const float height = physical.max.y - physical.min.y;
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f ||
      height <= 0.0f)
    return;

  const float density = vulkan_glass::requestDensity(request);
  const float shadowExtent =
      request.style.lighting.outerShadow > 0.0f
          ? clampFinite(request.style.lighting.shadowExtent * density, 0.0f,
                        32.0f * density, 5.0f * density)
          : 0.0f;
  const float drawExtent = std::max(shadowExtent, 1.0f);
  const float clipMinX =
      std::max({0.0f, physical.min.x - drawExtent, physical.clip.x});
  const float clipMinY =
      std::max({0.0f, physical.min.y - drawExtent, physical.clip.y});
  const float clipMaxX =
      std::min({static_cast<float>(frame.extent.width),
                physical.max.x + drawExtent, physical.clip.z});
  const float clipMaxY =
      std::min({static_cast<float>(frame.extent.height),
                physical.max.y + drawExtent, physical.clip.w});
  if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
    return;

  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(std::floor(clipMinX));
  scissor.offset.y = static_cast<int32_t>(std::floor(clipMinY));
  scissor.extent.width = static_cast<uint32_t>(std::ceil(clipMaxX)) -
                         static_cast<uint32_t>(scissor.offset.x);
  scissor.extent.height = static_cast<uint32_t>(std::ceil(clipMaxY)) -
                          static_cast<uint32_t>(scissor.offset.y);
  vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

  GlassPush push{};
  push.bounds[0] = physical.min.x;
  push.bounds[1] = physical.min.y;
  push.bounds[2] = physical.max.x;
  push.bounds[3] = physical.max.y;
  const float maximumRadius = std::min(width, height) * 0.5f;
  for (size_t radius = 0; radius < physical.radii.size(); ++radius) {
    push.radii[radius] =
        clampFinite(physical.radii[radius], 0.0f, maximumRadius, 0.0f);
  }
  if (request.shape == GlassShapeKind::Capsule) {
    std::fill(std::begin(push.radii), std::end(push.radii), maximumRadius);
  }
  push.refraction[0] =
      clampFinite(request.style.refractionHeight * density, 0.001f,
                  std::max(0.001f, maximumRadius), 20.0f * density);
  push.refraction[1] = clampFinite(request.style.refractionAmount * density,
                                   -960.0f, 960.0f, -70.0f * density);
  push.refraction[2] =
      clampFinite(request.style.depthEffect, -2.0f, 2.0f, 0.3f);
  push.refraction[3] =
      clampFinite(request.style.chromaticAberration, 0.0f, 1.0f, 0.5f);
  push.filter[0] = clampFinite(request.style.contrast, -1.0f, 1.0f, 0.0f);
  push.filter[1] = clampFinite(request.style.whitePoint, -1.0f, 1.0f, 0.0f);
  push.filter[2] =
      clampFinite(request.style.chromaMultiplier, 0.0f, 2.0f, 1.0f);
  push.filter[3] = clampFinite(request.opacity, 0.0f, 1.0f, 1.0f);
  push.tint[0] = clampFinite(request.style.tint.x, 0.0f, 1.0f, 1.0f);
  push.tint[1] = clampFinite(request.style.tint.y, 0.0f, 1.0f, 1.0f);
  push.tint[2] = clampFinite(request.style.tint.z, 0.0f, 1.0f, 1.0f);
  push.tint[3] = clampFinite(request.style.tint.w, 0.0f, 1.0f, 0.0f);
  push.framebuffer[0] = static_cast<float>(frame.extent.width);
  push.framebuffer[1] = static_cast<float>(frame.extent.height);
  push.framebuffer[2] = resources.srgbFormat ? 1.0f : 0.0f;
  push.framebuffer[3] = request.shape == GlassShapeKind::Circle ? 1.0f : 0.0f;
  push.deformation[0] =
      clampFinite(physical.deformation.x, -0.35f, 0.35f, 0.0f);
  push.deformation[1] =
      clampFinite(physical.deformation.y, -0.35f, 0.35f, 0.0f);
  push.deformation[2] = clampFinite(request.blurMix, 0.0f, 1.0f, 1.0f);
  const GlassLighting &lighting = request.style.lighting;
  push.lighting[0] = clampFinite(lighting.innerShadow, 0.0f, 1.0f, 0.12f);
  push.lighting[1] = clampFinite(lighting.rimGlow, 0.0f, 1.0f, 0.20f);
  push.lighting[2] = clampFinite(lighting.specular, 0.0f, 2.0f, 0.35f);
  push.lighting[3] = clampFinite(lighting.outerShadow, 0.0f, 1.0f, 0.14f);
  push.light[0] = clampFinite(physical.lightDirection.x, -1.0f, 1.0f, -0.7071f);
  push.light[1] = clampFinite(physical.lightDirection.y, -1.0f, 1.0f, -0.7071f);
  push.light[2] = shadowExtent;
  push.light[3] = clampFinite(lighting.specularPower, 1.0f, 128.0f, 28.0f);
  vkCmdPushConstants(frame.commandBuffer, resources.glassPipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(push), &push);
  vkCmdDraw(frame.commandBuffer, 6, 1, 0, 0);
}

void VulkanGlassBackend::finishFrame(const VulkanGlassFrameInfo &frame) {
  const auto found = resources_.find(frame.swapchain);
  if (found == resources_.end() ||
      frame.imageIndex >= found->second->frames.size())
    return;
  PerFrameResources &perFrame = found->second->frames[frame.imageIndex];
  perFrame.prepared = false;
  perFrame.preparedBackdropSet = VK_NULL_HANDLE;
}

void VulkanGlassBackend::removeSwapchain(VkSwapchainKHR swapchain) {
  resources_.erase(swapchain);
  disabled_.erase(swapchain);
}

void VulkanGlassBackend::removeDevice(VkDevice device) {
  for (auto iterator = resources_.begin(); iterator != resources_.end();) {
    if (iterator->second->device == device) {
      disabled_.erase(iterator->first);
      iterator = resources_.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

void VulkanGlassBackend::shutdown() {
  resources_.clear();
  disabled_.clear();
}

} // namespace glass_ui::widget
