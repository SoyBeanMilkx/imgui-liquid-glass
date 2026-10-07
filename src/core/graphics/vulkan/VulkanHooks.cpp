#include "VulkanHooks.hpp"

#include "hridhi/hridhi.h"
#include "ui/renderer/vulkan/VulkanRenderer.hpp"
#include "utils/LogUtils.hpp"

#include <dlfcn.h>
#include <vulkan/vulkan.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {

std::atomic<PFN_vkGetInstanceProcAddr> originalGetInstanceProcAddr{nullptr};
std::atomic<PFN_vkGetDeviceProcAddr> originalGetDeviceProcAddr{nullptr};
PFN_vkGetInstanceProcAddr getInstanceProcAddrExport = nullptr;
PFN_vkGetDeviceProcAddr getDeviceProcAddrExport = nullptr;
std::atomic<PFN_vkCreateInstance> originalCreateInstance{nullptr};
std::atomic<PFN_vkEnumeratePhysicalDevices> originalEnumeratePhysicalDevices{
    nullptr};
std::atomic<PFN_vkCreateDevice> originalCreateDevice{nullptr};
std::atomic<PFN_vkGetDeviceQueue> originalGetDeviceQueue{nullptr};
std::atomic<PFN_vkCreateSwapchainKHR> originalCreateSwapchain{nullptr};
std::atomic<PFN_vkDestroySwapchainKHR> originalDestroySwapchain{nullptr};
std::atomic<PFN_vkDestroyDevice> originalDestroyDevice{nullptr};
std::atomic<PFN_vkQueuePresentKHR> originalQueuePresent{nullptr};

hridhi_t getInstanceProcAddrHook = nullptr;
hridhi_t getDeviceProcAddrHook = nullptr;
hridhi_t getInstanceProcAddrBreakpoint = nullptr;
hridhi_t getDeviceProcAddrBreakpoint = nullptr;

thread_local bool inPresent = false;
thread_local int breakpointPassthrough = 0;

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hookedGetDeviceProcAddr(VkDevice device, const char *name);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hookedGetInstanceProcAddr(VkInstance instance, const char *name);

VKAPI_ATTR VkResult VKAPI_CALL hookedCreateInstance(
    const VkInstanceCreateInfo *createInfo,
    const VkAllocationCallbacks *allocator, VkInstance *instance) {
  return glass_ui::renderer::vulkan::createInstance(
      originalCreateInstance.load(std::memory_order_acquire), createInfo,
      allocator, instance);
}

VKAPI_ATTR VkResult VKAPI_CALL hookedEnumeratePhysicalDevices(
    VkInstance instance, uint32_t *count, VkPhysicalDevice *physicalDevices) {
  return glass_ui::renderer::vulkan::enumeratePhysicalDevices(
      originalEnumeratePhysicalDevices.load(std::memory_order_acquire),
      instance, count, physicalDevices);
}

VKAPI_ATTR VkResult VKAPI_CALL hookedCreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *createInfo,
    const VkAllocationCallbacks *allocator, VkDevice *device) {
  return glass_ui::renderer::vulkan::createDevice(
      originalCreateDevice.load(std::memory_order_acquire), physicalDevice,
      createInfo, allocator, device);
}

VKAPI_ATTR void VKAPI_CALL hookedGetDeviceQueue(VkDevice device,
                                                uint32_t family, uint32_t index,
                                                VkQueue *queue) {
  glass_ui::renderer::vulkan::getDeviceQueue(
      originalGetDeviceQueue.load(std::memory_order_acquire), device, family,
      index, queue);
}

VKAPI_ATTR VkResult VKAPI_CALL hookedCreateSwapchain(
    VkDevice device, const VkSwapchainCreateInfoKHR *createInfo,
    const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
  return glass_ui::renderer::vulkan::createSwapchain(
      originalCreateSwapchain.load(std::memory_order_acquire), device,
      createInfo, allocator, swapchain);
}

VKAPI_ATTR void VKAPI_CALL
hookedDestroySwapchain(VkDevice device, VkSwapchainKHR swapchain,
                       const VkAllocationCallbacks *allocator) {
  glass_ui::renderer::vulkan::destroySwapchain(
      originalDestroySwapchain.load(std::memory_order_acquire), device,
      swapchain, allocator);
}

VKAPI_ATTR void VKAPI_CALL
hookedDestroyDevice(VkDevice device, const VkAllocationCallbacks *allocator) {
  glass_ui::renderer::vulkan::destroyDevice(
      originalDestroyDevice.load(std::memory_order_acquire), device,
      allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL
hookedQueuePresent(VkQueue queue, const VkPresentInfoKHR *present) {
  const auto original = originalQueuePresent.load(std::memory_order_acquire);
  if (inPresent)
    return original(queue, present);
  inPresent = true;
  const VkResult result =
      glass_ui::renderer::vulkan::queuePresent(original, queue, present);
  inPresent = false;
  return result;
}

template <typename Fn>
bool saveOriginal(std::atomic<Fn> &slot, PFN_vkVoidFunction fn) {
  Fn expected = nullptr;
  slot.compare_exchange_strong(expected, reinterpret_cast<Fn>(fn),
                               std::memory_order_release,
                               std::memory_order_relaxed);
  return true;
}

PFN_vkVoidFunction wrapProc(const char *name, PFN_vkVoidFunction fn) {
  if (!name || !fn)
    return fn;
  if (std::strcmp(name, "vkGetInstanceProcAddr") == 0) {
    saveOriginal(originalGetInstanceProcAddr, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedGetInstanceProcAddr);
  }
  if (std::strcmp(name, "vkGetDeviceProcAddr") == 0) {
    saveOriginal(originalGetDeviceProcAddr, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedGetDeviceProcAddr);
  }
  if (std::strcmp(name, "vkCreateInstance") == 0) {
    saveOriginal(originalCreateInstance, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedCreateInstance);
  }
  if (std::strcmp(name, "vkEnumeratePhysicalDevices") == 0) {
    saveOriginal(originalEnumeratePhysicalDevices, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedEnumeratePhysicalDevices);
  }
  if (std::strcmp(name, "vkCreateDevice") == 0) {
    saveOriginal(originalCreateDevice, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedCreateDevice);
  }
  if (std::strcmp(name, "vkGetDeviceQueue") == 0) {
    saveOriginal(originalGetDeviceQueue, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedGetDeviceQueue);
  }
  if (std::strcmp(name, "vkCreateSwapchainKHR") == 0) {
    saveOriginal(originalCreateSwapchain, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedCreateSwapchain);
  }
  if (std::strcmp(name, "vkDestroySwapchainKHR") == 0) {
    saveOriginal(originalDestroySwapchain, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedDestroySwapchain);
  }
  if (std::strcmp(name, "vkDestroyDevice") == 0) {
    saveOriginal(originalDestroyDevice, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedDestroyDevice);
  }
  if (std::strcmp(name, "vkQueuePresentKHR") == 0) {
    saveOriginal(originalQueuePresent, fn);
    return reinterpret_cast<PFN_vkVoidFunction>(hookedQueuePresent);
  }
  return fn;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hookedGetDeviceProcAddr(VkDevice device, const char *name) {
  PFN_vkVoidFunction fn = nullptr;
  ++breakpointPassthrough;
  if (const auto original =
          originalGetDeviceProcAddr.load(std::memory_order_acquire))
    fn = original(device, name);
  else if (getDeviceProcAddrExport)
    fn = getDeviceProcAddrExport(device, name);
  --breakpointPassthrough;
  return wrapProc(name, fn);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hookedGetInstanceProcAddr(VkInstance instance, const char *name) {
  PFN_vkVoidFunction fn = nullptr;
  ++breakpointPassthrough;
  if (const auto original =
          originalGetInstanceProcAddr.load(std::memory_order_acquire))
    fn = original(instance, name);
  else if (getInstanceProcAddrExport)
    fn = getInstanceProcAddrExport(instance, name);
  --breakpointPassthrough;
  return wrapProc(name, fn);
}

void onGetInstanceProcAddr(hridhi_context_t *ctx) {
  if (breakpointPassthrough)
    return;
  auto instance = reinterpret_cast<VkInstance>(ctx->x[0]);
  auto name = reinterpret_cast<const char *>(ctx->x[1]);
  ++breakpointPassthrough;
  PFN_vkVoidFunction fn = getInstanceProcAddrExport
                              ? getInstanceProcAddrExport(instance, name)
                              : nullptr;
  --breakpointPassthrough;
  ctx->x[0] = reinterpret_cast<uint64_t>(wrapProc(name, fn));
  ctx->pc = ctx->x[30];
}

void onGetDeviceProcAddr(hridhi_context_t *ctx) {
  if (breakpointPassthrough)
    return;
  auto device = reinterpret_cast<VkDevice>(ctx->x[0]);
  auto name = reinterpret_cast<const char *>(ctx->x[1]);
  ++breakpointPassthrough;
  PFN_vkVoidFunction fn =
      getDeviceProcAddrExport ? getDeviceProcAddrExport(device, name) : nullptr;
  --breakpointPassthrough;
  ctx->x[0] = reinterpret_cast<uint64_t>(wrapProc(name, fn));
  ctx->pc = ctx->x[30];
}

bool installBreakpoint(void *target, hridhi_breakpoint_cb cb, const char *name,
                       hridhi_t *outHandle) {
  if (!target) {
    glass_ui::log::error("dlsym(%s) failed", name);
    return false;
  }
  hridhi_t handle = hridhi_add_breakpoint(target, cb);
  if (!handle) {
    glass_ui::log::error("hridhi_add_breakpoint(%s @ %p): %s", name, target,
                      hridhi_strerror(errno));
    return false;
  }
  *outHandle = handle;
  return true;
}

// Follow tiny BTI+B exports to hook the real GetProcAddr implementation.
void *resolveBranchThunk(void *entry, const char *name) {
  if (!entry)
    return nullptr;

  const auto *words = static_cast<const uint32_t *>(entry);
  for (size_t i = 0; i < 4; ++i) {
    const uint32_t instruction = words[i];
    if ((instruction & 0xfc000000u) == 0x14000000u) {
      int64_t displacement = instruction & 0x03ffffffu;
      if ((displacement & 0x02000000) != 0)
        displacement |= ~INT64_C(0x03ffffff);
      displacement *= 4;
      auto *branch = reinterpret_cast<const uint8_t *>(&words[i]);
      void *target = const_cast<uint8_t *>(branch + displacement);
      return target;
    }

    // Skip only landing pads and NOPs; never guess past real instructions.
    constexpr uint32_t kBtiC = 0xd503245fu;
    constexpr uint32_t kNop = 0xd503201fu;
    if (instruction != kBtiC && instruction != kNop)
      break;
  }
  glass_ui::log::warn("%s @ %p is not a supported direct-branch thunk", name,
                   entry);
  return nullptr;
}

template <typename Fn>
bool installImplementationHook(void *exportEntry, void *replacement,
                               std::atomic<Fn> &original, hridhi_t *outHandle,
                               const char *name) {
  void *implementation = resolveBranchThunk(exportEntry, name);
  if (!implementation)
    return false;

  void *gateway = nullptr;
  hridhi_t handle = hridhi_hook_install(implementation, replacement, &gateway);
  if (!handle) {
    glass_ui::log::error("hridhi_hook_install(%s impl @ %p): %s", name,
                      implementation, hridhi_strerror(errno));
    return false;
  }

  original.store(reinterpret_cast<Fn>(gateway), std::memory_order_release);
  *outHandle = handle;
  return true;
}

bool installHooks() {
  static std::once_flag once;
  static bool installed = false;
  std::call_once(once, [] {
    if (!hridhi_available()) {
      glass_ui::log::error("hridhi_available() = 0");
      return;
    }

    void *lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib)
      lib = dlopen("libvulkan.so", RTLD_NOW);
    if (!lib) {
      glass_ui::log::error("dlopen(libvulkan.so) failed: %s", dlerror());
      return;
    }

    getInstanceProcAddrExport = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(lib, "vkGetInstanceProcAddr"));
    getDeviceProcAddrExport = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        dlsym(lib, "vkGetDeviceProcAddr"));

    if (!getInstanceProcAddrExport || !getDeviceProcAddrExport) {
      glass_ui::log::error("libvulkan GetProcAddr exports are unavailable");
      return;
    }

    const bool instanceHookedInline = installImplementationHook(
        reinterpret_cast<void *>(getInstanceProcAddrExport),
        reinterpret_cast<void *>(hookedGetInstanceProcAddr),
        originalGetInstanceProcAddr, &getInstanceProcAddrHook,
        "vkGetInstanceProcAddr");
    const bool deviceHookedInline = installImplementationHook(
        reinterpret_cast<void *>(getDeviceProcAddrExport),
        reinterpret_cast<void *>(hookedGetDeviceProcAddr),
        originalGetDeviceProcAddr, &getDeviceProcAddrHook,
        "vkGetDeviceProcAddr");

    // Breakpoints cover vendor stubs that cannot be decoded or relocated.
    const bool instanceHooked =
        instanceHookedInline ||
        installBreakpoint(reinterpret_cast<void *>(getInstanceProcAddrExport),
                          onGetInstanceProcAddr, "vkGetInstanceProcAddr",
                          &getInstanceProcAddrBreakpoint);
    const bool deviceHooked =
        deviceHookedInline ||
        installBreakpoint(reinterpret_cast<void *>(getDeviceProcAddrExport),
                          onGetDeviceProcAddr, "vkGetDeviceProcAddr",
                          &getDeviceProcAddrBreakpoint);
    installed = instanceHooked && deviceHooked;

    glass_ui::log::info(
        "hook install complete: GIPA=%s GDPA=%s",
        instanceHookedInline ? "inline-implementation" : "breakpoint-export",
        deviceHookedInline ? "inline-implementation" : "breakpoint-export");
  });
  return installed;
}

}

namespace glass_ui::hooks::vulkan {

bool initialize() { return installHooks(); }

}
