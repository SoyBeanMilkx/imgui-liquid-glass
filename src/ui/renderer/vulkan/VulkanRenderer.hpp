#pragma once

#include <vulkan/vulkan.h>

namespace glass_ui {
class InputBridge;
}

namespace glass_ui::renderer::vulkan {

void initialize(InputBridge &input);
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
                      const VkAllocationCallbacks *allocator, VkDevice *device);
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

}
