#include "VulkanRenderer.hpp"

#include "ImGuiVulkanRenderer.hpp"
#include "VulkanRendererTypes.hpp"
#include "SwapchainRenderer.hpp"
#include "ui/widget/effects/glass/backend/vulkan/VulkanGlassBackend.hpp"
#include "utils/LogUtils.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace glass_ui::renderer::vulkan {
namespace {

class VulkanRenderer final {
public:
  void initialize(InputBridge &input) noexcept { imgui_.initialize(input); }

  VkResult createInstance(PFN_vkCreateInstance original,
                          const VkInstanceCreateInfo *createInfo,
                          const VkAllocationCallbacks *allocator,
                          VkInstance *instance);
  VkResult enumeratePhysicalDevices(PFN_vkEnumeratePhysicalDevices original,
                                    VkInstance instance, uint32_t *count,
                                    VkPhysicalDevice *physicalDevices);
  VkResult createDevice(PFN_vkCreateDevice original,
                        VkPhysicalDevice physicalDevice,
                        const VkDeviceCreateInfo *createInfo,
                        const VkAllocationCallbacks *allocator,
                        VkDevice *device);
  void getDeviceQueue(PFN_vkGetDeviceQueue original, VkDevice device,
                      uint32_t family, uint32_t index, VkQueue *queue);
  VkResult createSwapchain(PFN_vkCreateSwapchainKHR original, VkDevice device,
                           const VkSwapchainCreateInfoKHR *createInfo,
                           const VkAllocationCallbacks *allocator,
                           VkSwapchainKHR *swapchain);
  void destroySwapchain(PFN_vkDestroySwapchainKHR original, VkDevice device,
                        VkSwapchainKHR swapchain,
                        const VkAllocationCallbacks *allocator);
  void destroyDevice(PFN_vkDestroyDevice original, VkDevice device,
                     const VkAllocationCallbacks *allocator);
  VkResult queuePresent(PFN_vkQueuePresentKHR original, VkQueue queue,
                        const VkPresentInfoKHR *present);

private:
  bool prepare(VkQueue queue, VkSwapchainKHR swapchain,
               SwapchainResources **resources);
  void removeSwapchain(VkSwapchainKHR swapchain);

  std::mutex mutex_;
  std::unordered_map<VkInstance, uint32_t> instanceApi_;
  std::unordered_map<VkPhysicalDevice, VkInstance> physicalToInstance_;
  std::unordered_map<VkDevice, DeviceState> devices_;
  std::unordered_map<VkQueue, QueueState> queues_;
  std::unordered_map<VkSwapchainKHR, SwapchainState> swapchains_;
  std::unordered_map<VkSwapchainKHR, SwapchainResources> resources_;
  ImGuiVulkanRenderer imgui_;
  widget::VulkanGlassBackend glass_;
  std::atomic<uint32_t> presentCount_{0};
};

VulkanRenderer &renderer() {
  static auto *instance = new VulkanRenderer;
  return *instance;
}

bool VulkanRenderer::prepare(VkQueue queue, VkSwapchainKHR swapchain,
                             SwapchainResources **resources) {
  const auto swapchainIt = swapchains_.find(swapchain);
  const auto queueIt = queues_.find(queue);
  if (swapchainIt == swapchains_.end() || queueIt == queues_.end())
    return false;

  const SwapchainState &state = swapchainIt->second;
  if (state.device != queueIt->second.device)
    return false;
  if ((state.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
    log::warn("swapchain usage 0x%x has no COLOR_ATTACHMENT, skip overlay",
              state.usage);
    return false;
  }
  if (state.extent.width == 0 || state.extent.height == 0)
    return false;

  SwapchainResources &current = resources_[swapchain];
  if (!buildSwapchainResources(swapchain, state, queueIt->second.family,
                               current)) {
    resources_.erase(swapchain);
    return false;
  }

  const auto deviceIt = devices_.find(state.device);
  if (deviceIt == devices_.end() ||
      !imgui_.ensure(deviceIt->second, queueIt->second, current, queue))
    return false;

  glass_.ensure(widget::VulkanGlassCreateInfo{
      deviceIt->second.physical, state.device, current.handle, current.format,
      current.extent, current.renderPass,
      static_cast<uint32_t>(current.frames.size()),
      (state.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0 &&
          state.presentMode != VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR &&
          state.presentMode != VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR});
  imgui_.setEffectCapabilities(glass_.capabilities(current.handle));

  *resources = &current;
  return true;
}

void VulkanRenderer::removeSwapchain(VkSwapchainKHR swapchain) {
  glass_.removeSwapchain(swapchain);
  const auto resourcesIt = resources_.find(swapchain);
  if (resourcesIt != resources_.end()) {
    if (imgui_.uses(resourcesIt->second.device))
      imgui_.shutdown();
    destroySwapchainResources(resourcesIt->second);
    resources_.erase(resourcesIt);
  }
  swapchains_.erase(swapchain);
}

VkResult VulkanRenderer::createInstance(PFN_vkCreateInstance original,
                                        const VkInstanceCreateInfo *createInfo,
                                        const VkAllocationCallbacks *allocator,
                                        VkInstance *instance) {
  const VkResult result = original(createInfo, allocator, instance);
  if (result == VK_SUCCESS && instance && *instance) {
    const uint32_t apiVersion = createInfo && createInfo->pApplicationInfo &&
                                        createInfo->pApplicationInfo->apiVersion
                                    ? createInfo->pApplicationInfo->apiVersion
                                    : VK_API_VERSION_1_0;
    std::lock_guard<std::mutex> lock(mutex_);
    instanceApi_[*instance] = apiVersion;
  }
  return result;
}

VkResult VulkanRenderer::enumeratePhysicalDevices(
    PFN_vkEnumeratePhysicalDevices original, VkInstance instance,
    uint32_t *count, VkPhysicalDevice *physicalDevices) {
  const VkResult result = original(instance, count, physicalDevices);
  if ((result == VK_SUCCESS || result == VK_INCOMPLETE) && physicalDevices &&
      count) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t index = 0; index < *count; ++index)
      physicalToInstance_[physicalDevices[index]] = instance;
  }
  return result;
}

VkResult VulkanRenderer::createDevice(PFN_vkCreateDevice original,
                                      VkPhysicalDevice physicalDevice,
                                      const VkDeviceCreateInfo *createInfo,
                                      const VkAllocationCallbacks *allocator,
                                      VkDevice *device) {
  const VkResult result =
      original(physicalDevice, createInfo, allocator, device);
  if (result != VK_SUCCESS || !device || !*device)
    return result;

  DeviceState state;
  state.physical = physicalDevice;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto instanceIt = physicalToInstance_.find(physicalDevice);
    if (instanceIt != physicalToInstance_.end())
      state.instance = instanceIt->second;
    else if (instanceApi_.size() == 1)
      state.instance = instanceApi_.begin()->first;

    const auto apiIt = instanceApi_.find(state.instance);
    if (apiIt != instanceApi_.end())
      state.apiVersion = apiIt->second;
    devices_[*device] = state;
  }
  return result;
}

void VulkanRenderer::getDeviceQueue(PFN_vkGetDeviceQueue original,
                                    VkDevice device, uint32_t family,
                                    uint32_t index, VkQueue *queue) {
  original(device, family, index, queue);
  if (!queue || !*queue)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  queues_[*queue] = QueueState{device, family};
}

VkResult VulkanRenderer::createSwapchain(
    PFN_vkCreateSwapchainKHR original, VkDevice device,
    const VkSwapchainCreateInfoKHR *createInfo,
    const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
  if (!createInfo)
    return original(device, createInfo, allocator, swapchain);

  VkSwapchainCreateInfoKHR patchedCreateInfo = *createInfo;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto deviceIt = devices_.find(device);
    if (deviceIt != devices_.end())
      physicalDevice = deviceIt->second.physical;
  }
  if (physicalDevice && createInfo->surface) {
    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, createInfo->surface, &capabilities) == VK_SUCCESS &&
        (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) !=
            0) {
      patchedCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
  }

  const VkResult result =
      original(device, &patchedCreateInfo, allocator, swapchain);
  if (result != VK_SUCCESS || !swapchain || !*swapchain || !createInfo)
    return result;

  SwapchainState state;
  state.device = device;
  state.format = createInfo->imageFormat;
  state.extent = createInfo->imageExtent;
  state.usage = patchedCreateInfo.imageUsage;
  state.presentMode = createInfo->presentMode;
  state.preTransform = createInfo->preTransform;

  std::lock_guard<std::mutex> lock(mutex_);
  if (createInfo->oldSwapchain)
    removeSwapchain(createInfo->oldSwapchain);
  swapchains_[*swapchain] = state;
  return result;
}

void VulkanRenderer::destroySwapchain(PFN_vkDestroySwapchainKHR original,
                                      VkDevice device, VkSwapchainKHR swapchain,
                                      const VkAllocationCallbacks *allocator) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    removeSwapchain(swapchain);
  }
  original(device, swapchain, allocator);
}

void VulkanRenderer::destroyDevice(PFN_vkDestroyDevice original,
                                   VkDevice device,
                                   const VkAllocationCallbacks *allocator) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (imgui_.uses(device))
      imgui_.shutdown();
    glass_.removeDevice(device);
    for (auto it = resources_.begin(); it != resources_.end();) {
      if (it->second.device == device) {
        destroySwapchainResources(it->second);
        it = resources_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = swapchains_.begin(); it != swapchains_.end();) {
      if (it->second.device == device) {
        glass_.removeSwapchain(it->first);
        it = swapchains_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = queues_.begin(); it != queues_.end();) {
      if (it->second.device == device)
        it = queues_.erase(it);
      else
        ++it;
    }
    devices_.erase(device);
  }
  original(device, allocator);
}

VkResult VulkanRenderer::queuePresent(PFN_vkQueuePresentKHR original,
                                      VkQueue queue,
                                      const VkPresentInfoKHR *present) {
  if (!original)
    return VK_ERROR_INITIALIZATION_FAILED;
  if (!present || present->swapchainCount == 0 || !present->pSwapchains ||
      !present->pImageIndices)
    return original(queue, present);

  const uint32_t sequence = presentCount_.fetch_add(1) + 1;
  if (sequence <= 3) {
    return original(queue, present);
  }

  std::lock_guard<std::mutex> lock(mutex_);
  SwapchainResources *resources = nullptr;
  if (!prepare(queue, present->pSwapchains[0], &resources) || !resources ||
      present->pImageIndices[0] >= resources->frames.size()) {
    imgui_.suspendInput();
    return original(queue, present);
  }

  FrameResources &frame = resources->frames[present->pImageIndices[0]];
  imgui_.draw(*resources);

  if (!submitOverlay(queue, *resources, frame, present->pImageIndices[0],
                     present->pWaitSemaphores, present->waitSemaphoreCount,
                     &glass_, imgui_.glassRequests(), sequence)) {
    imgui_.suspendInput();
    return original(queue, present);
  }

  VkPresentInfoKHR patched = *present;
  patched.waitSemaphoreCount = 1;
  patched.pWaitSemaphores = &frame.renderDone;
  const VkResult result = original(queue, &patched);
  const VkResult overlayResult = patched.pResults ? patched.pResults[0] : result;
  if ((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) &&
      (overlayResult == VK_SUCCESS || overlayResult == VK_SUBOPTIMAL_KHR))
    imgui_.commitInput();
  else
    imgui_.suspendInput();
  return result;
}

}
void initialize(InputBridge &input) { renderer().initialize(input); }

VkResult createInstance(PFN_vkCreateInstance original,
                        const VkInstanceCreateInfo *createInfo,
                        const VkAllocationCallbacks *allocator,
                        VkInstance *instance) {
  return renderer().createInstance(original, createInfo, allocator, instance);
}

VkResult enumeratePhysicalDevices(PFN_vkEnumeratePhysicalDevices original,
                                  VkInstance instance, uint32_t *count,
                                  VkPhysicalDevice *physicalDevices) {
  return renderer().enumeratePhysicalDevices(original, instance, count,
                                             physicalDevices);
}

VkResult createDevice(PFN_vkCreateDevice original,
                      VkPhysicalDevice physicalDevice,
                      const VkDeviceCreateInfo *createInfo,
                      const VkAllocationCallbacks *allocator,
                      VkDevice *device) {
  return renderer().createDevice(original, physicalDevice, createInfo,
                                 allocator, device);
}

void getDeviceQueue(PFN_vkGetDeviceQueue original, VkDevice device,
                    uint32_t family, uint32_t index, VkQueue *queue) {
  renderer().getDeviceQueue(original, device, family, index, queue);
}

VkResult createSwapchain(PFN_vkCreateSwapchainKHR original, VkDevice device,
                         const VkSwapchainCreateInfoKHR *createInfo,
                         const VkAllocationCallbacks *allocator,
                         VkSwapchainKHR *swapchain) {
  return renderer().createSwapchain(original, device, createInfo, allocator,
                                    swapchain);
}

void destroySwapchain(PFN_vkDestroySwapchainKHR original, VkDevice device,
                      VkSwapchainKHR swapchain,
                      const VkAllocationCallbacks *allocator) {
  renderer().destroySwapchain(original, device, swapchain, allocator);
}

void destroyDevice(PFN_vkDestroyDevice original, VkDevice device,
                   const VkAllocationCallbacks *allocator) {
  renderer().destroyDevice(original, device, allocator);
}

VkResult queuePresent(PFN_vkQueuePresentKHR original, VkQueue queue,
                      const VkPresentInfoKHR *present) {
  return renderer().queuePresent(original, queue, present);
}

}
