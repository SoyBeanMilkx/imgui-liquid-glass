#include "ImGuiVulkanRenderer.hpp"

#include "SurfaceTransform.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "utils/LogUtils.hpp"

#include <algorithm>

namespace glass_ui::renderer::vulkan {
namespace {

void checkVkResult(VkResult result) {
  if (result != VK_SUCCESS)
    log::error("imgui failed: VkResult=%d", result);
}

}

void ImGuiVulkanRenderer::initialize(InputBridge &input) noexcept {
  runtime_.initialize(input);
}

bool ImGuiVulkanRenderer::ensure(const DeviceState &deviceState,
                                 const QueueState &queueState,
                                 const SwapchainResources &swapchain,
                                 VkQueue queue) {
  if (ready_ && device_ == swapchain.device)
    return true;
  if (ready_)
    shutdown();

  if (!runtime_.ensureContext())
    return false;
  ImGuiIO &io = ImGui::GetIO();
  const VkExtent2D extent = logicalExtent(swapchain);
  io.DisplaySize =
      ImVec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
  ImGui_ImplVulkan_InitInfo init{};
  init.ApiVersion = deviceState.apiVersion;
  init.Instance = deviceState.instance;
  init.PhysicalDevice = deviceState.physical;
  init.Device = swapchain.device;
  init.QueueFamily = queueState.family;
  init.Queue = queue;
  init.DescriptorPoolSize = 32;
  init.MinImageCount =
      std::max(2u, static_cast<uint32_t>(swapchain.frames.size()));
  init.ImageCount = static_cast<uint32_t>(swapchain.frames.size());
  init.PipelineInfoMain.RenderPass = swapchain.renderPass;
  init.PipelineInfoMain.Subpass = 0;
  init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init.CheckVkResultFn = checkVkResult;

  if (!ImGui_ImplVulkan_Init(&init)) {
    log::error("ImGui_ImplVulkan_Init failed");
    runtime_.shutdownContext();
    return false;
  }

  ready_ = true;
  device_ = swapchain.device;

  // Prime the font atlas outside the host's present semaphore chain.
  ImGui_ImplVulkan_NewFrame();
  ImGui::NewFrame();
  ImGui::Render();
  if (ImDrawData *draw = ImGui::GetDrawData()) {
    if (draw->Textures) {
      for (ImTextureData *texture : *draw->Textures) {
        if (texture && texture->Status != ImTextureStatus_OK)
          ImGui_ImplVulkan_UpdateTexture(texture);
      }
    }
  }
  log::info("imgui vulkan backend ready");
  return true;
}

void ImGuiVulkanRenderer::draw(const SwapchainResources &swapchain) {
  const VkExtent2D extent = logicalExtent(swapchain);
  runtime_.prepareFrame(RenderSurfaceInfo{
      extent.width, extent.height, inputTransform(swapchain.preTransform)});
  ImGui_ImplVulkan_NewFrame();
  applySurfaceTransform(runtime_.buildFrame(), swapchain);
}

void ImGuiVulkanRenderer::commitInput() noexcept {
  runtime_.commitInput();
}

void ImGuiVulkanRenderer::suspendInput() noexcept {
  runtime_.suspendInput();
}

void ImGuiVulkanRenderer::shutdown() {
  suspendInput();
  if (!ready_)
    return;
  if (device_)
    vkDeviceWaitIdle(device_);
  ImGui_ImplVulkan_Shutdown();
  runtime_.shutdownContext();
  ready_ = false;
  device_ = VK_NULL_HANDLE;
}

}
