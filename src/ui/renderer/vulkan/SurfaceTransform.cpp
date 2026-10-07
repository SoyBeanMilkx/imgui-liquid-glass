#include "SurfaceTransform.hpp"

#include "imgui.h"
#include "toucher.h"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <atomic>
#include <cfloat>

namespace glass_ui::renderer::vulkan {
namespace {

ImVec2 logicalToSwapchain(const ImVec2 &point,
                          const SwapchainResources &swapchain) {
  const float rawWidth = static_cast<float>(swapchain.extent.width);
  const float rawHeight = static_cast<float>(swapchain.extent.height);
  const VkExtent2D logical = logicalExtent(swapchain);
  const float x = point.x / static_cast<float>(logical.width);
  const float y = point.y / static_cast<float>(logical.height);
  switch (swapchain.preTransform) {
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    return ImVec2((1.0f - y) * rawWidth, x * rawHeight);
  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
    return ImVec2((1.0f - x) * rawWidth, (1.0f - y) * rawHeight);
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    return ImVec2(y * rawWidth, (1.0f - x) * rawHeight);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
    return ImVec2((1.0f - x) * rawWidth, y * rawHeight);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
    return ImVec2((1.0f - y) * rawWidth, (1.0f - x) * rawHeight);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
    return ImVec2(x * rawWidth, (1.0f - y) * rawHeight);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
    return ImVec2(y * rawWidth, x * rawHeight);
  default:
    return point;
  }
}

}

VkExtent2D logicalExtent(const SwapchainResources &swapchain) noexcept {
  switch (swapchain.preTransform) {
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
    return VkExtent2D{swapchain.extent.height, swapchain.extent.width};
  default:
    return swapchain.extent;
  }
}

uint32_t inputTransform(VkSurfaceTransformFlagBitsKHR transform) noexcept {
  switch (transform) {
  case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
    return TOUCHER_TRANSFORM_IDENTITY;
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    return TOUCHER_TRANSFORM_ROTATE_270;
  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
    return TOUCHER_TRANSFORM_ROTATE_180;
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    return TOUCHER_TRANSFORM_ROTATE_90;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
    return TOUCHER_TRANSFORM_MIRROR_HORIZONTAL;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
    return TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_90;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
    return TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_180;
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
    return TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_270;
  default:
    return TOUCHER_TRANSFORM_IDENTITY;
  }
}

void applySurfaceTransform(ImDrawData *drawData,
                           const SwapchainResources &swapchain) {
  if (!drawData ||
      swapchain.preTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    return;
  if (swapchain.preTransform == VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR) {
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true))
      log::warn("inherited preTransform cannot be resolved: 0x%x",
                swapchain.preTransform);
    return;
  }

  for (ImDrawList *drawList : drawData->CmdLists) {
    for (ImDrawVert &vertex : drawList->VtxBuffer)
      vertex.pos = logicalToSwapchain(vertex.pos, swapchain);

    for (ImDrawCmd &command : drawList->CmdBuffer) {
      const ImVec2 corners[4] = {
          ImVec2(command.ClipRect.x, command.ClipRect.y),
          ImVec2(command.ClipRect.z, command.ClipRect.y),
          ImVec2(command.ClipRect.x, command.ClipRect.w),
          ImVec2(command.ClipRect.z, command.ClipRect.w),
      };
      ImVec2 minimum(FLT_MAX, FLT_MAX);
      ImVec2 maximum(-FLT_MAX, -FLT_MAX);
      for (const ImVec2 &corner : corners) {
        const ImVec2 transformed = logicalToSwapchain(corner, swapchain);
        minimum.x = std::min(minimum.x, transformed.x);
        minimum.y = std::min(minimum.y, transformed.y);
        maximum.x = std::max(maximum.x, transformed.x);
        maximum.y = std::max(maximum.y, transformed.y);
      }
      command.ClipRect = ImVec4(minimum.x, minimum.y, maximum.x, maximum.y);
    }
  }

  drawData->DisplayPos = ImVec2(0.0f, 0.0f);
  drawData->DisplaySize = ImVec2(static_cast<float>(swapchain.extent.width),
                                 static_cast<float>(swapchain.extent.height));
  drawData->FramebufferScale = ImVec2(1.0f, 1.0f);
}

}
