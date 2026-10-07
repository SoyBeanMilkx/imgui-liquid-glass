#include "VulkanGlassGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace glass_ui::widget::vulkan_glass {
namespace {

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept {
  return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
}

ImVec2 transformPoint(ImVec2 point, ImVec2 logicalSize, VkExtent2D rawExtent,
                      VkSurfaceTransformFlagBitsKHR transform) {
  const float x = point.x / std::max(logicalSize.x, 1.0f);
  const float y = point.y / std::max(logicalSize.y, 1.0f);
  const float width = static_cast<float>(rawExtent.width);
  const float height = static_cast<float>(rawExtent.height);
  switch (transform) {
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    return ImVec2((1.0f - y) * width, x * height);
  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
    return ImVec2((1.0f - x) * width, (1.0f - y) * height);
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    return ImVec2(y * width, (1.0f - x) * height);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
    return ImVec2((1.0f - x) * width, y * height);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
    return ImVec2((1.0f - y) * width, (1.0f - x) * height);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
    return ImVec2(x * width, (1.0f - y) * height);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
    return ImVec2(y * width, x * height);
  default:
    return point;
  }
}

ImVec2 transformDirection(ImVec2 value,
                          VkSurfaceTransformFlagBitsKHR transform) noexcept {
  switch (transform) {
  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    return ImVec2(-value.y, value.x);
  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
    return ImVec2(-value.x, -value.y);
  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    return ImVec2(value.y, -value.x);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
    return ImVec2(-value.x, value.y);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
    return ImVec2(-value.y, -value.x);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
    return ImVec2(value.x, -value.y);
  case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
    return ImVec2(value.y, value.x);
  default:
    return value;
  }
}

} // namespace
float requestDensity(const GlassRequest &request) noexcept {
  return clampFinite(request.density, 0.25f, 4.0f, 1.0f);
}

PhysicalRequest transformRequest(const GlassRequest &request,
                                 const VulkanGlassFrameInfo &frame) {
  const bool swapsAxes =
      frame.preTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
      frame.preTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR ||
      frame.preTransform ==
          VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR ||
      frame.preTransform ==
          VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;
  const ImVec2 logicalSize(
      static_cast<float>(swapsAxes ? frame.extent.height : frame.extent.width),
      static_cast<float>(swapsAxes ? frame.extent.width : frame.extent.height));

  const std::array<ImVec2, 4> logicalCorners = {
      ImVec2(request.min.x, request.min.y),
      ImVec2(request.max.x, request.min.y),
      ImVec2(request.max.x, request.max.y),
      ImVec2(request.min.x, request.max.y),
  };
  const std::array<float, 4> logicalRadii = {
      request.radii.topLeft * requestDensity(request),
      request.radii.topRight * requestDensity(request),
      request.radii.bottomRight * requestDensity(request),
      request.radii.bottomLeft * requestDensity(request)};

  PhysicalRequest result;
  result.deformation =
      transformDirection(request.deformation, frame.preTransform);
  result.lightDirection = transformDirection(
      request.style.lighting.lightDirection, frame.preTransform);
  result.min = ImVec2(std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
  result.max = ImVec2(-std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max());
  std::array<ImVec2, 4> transformed{};
  for (size_t index = 0; index < logicalCorners.size(); ++index) {
    transformed[index] = transformPoint(logicalCorners[index], logicalSize,
                                        frame.extent, frame.preTransform);
    result.min.x = std::min(result.min.x, transformed[index].x);
    result.min.y = std::min(result.min.y, transformed[index].y);
    result.max.x = std::max(result.max.x, transformed[index].x);
    result.max.y = std::max(result.max.y, transformed[index].y);
  }
  const ImVec2 midpoint((result.min.x + result.max.x) * 0.5f,
                        (result.min.y + result.max.y) * 0.5f);
  for (size_t index = 0; index < transformed.size(); ++index) {
    const bool right = transformed[index].x >= midpoint.x;
    const bool bottom = transformed[index].y >= midpoint.y;
    const size_t target = bottom ? (right ? 2u : 3u) : (right ? 1u : 0u);
    result.radii[target] = logicalRadii[index];
  }

  const std::array<ImVec2, 4> clipCorners = {
      ImVec2(request.clipRect.x, request.clipRect.y),
      ImVec2(request.clipRect.z, request.clipRect.y),
      ImVec2(request.clipRect.z, request.clipRect.w),
      ImVec2(request.clipRect.x, request.clipRect.w),
  };
  ImVec2 clipMin(std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max());
  ImVec2 clipMax(-std::numeric_limits<float>::max(),
                 -std::numeric_limits<float>::max());
  for (const ImVec2 &corner : clipCorners) {
    const ImVec2 value =
        transformPoint(corner, logicalSize, frame.extent, frame.preTransform);
    clipMin.x = std::min(clipMin.x, value.x);
    clipMin.y = std::min(clipMin.y, value.y);
    clipMax.x = std::max(clipMax.x, value.x);
    clipMax.y = std::max(clipMax.y, value.y);
  }
  result.clip = ImVec4(clipMin.x, clipMin.y, clipMax.x, clipMax.y);
  return result;
}

bool hasVisibleRequest(const VulkanGlassFrameInfo &frame,
                       const std::vector<GlassRequest> &requests) {
  for (const GlassRequest &request : requests) {
    const PhysicalRequest physical = transformRequest(request, frame);
    const float density = requestDensity(request);
    const float shadowExtent =
        request.style.lighting.outerShadow > 0.0f
            ? clampFinite(request.style.lighting.shadowExtent * density, 0.0f,
                          32.0f * density, 5.0f * density)
            : 0.0f;
    const float drawExtent = std::max(shadowExtent, 1.0f);
    const float visibleMinX =
        std::max({0.0f, physical.min.x - drawExtent, physical.clip.x});
    const float visibleMinY =
        std::max({0.0f, physical.min.y - drawExtent, physical.clip.y});
    const float visibleMaxX =
        std::min({static_cast<float>(frame.extent.width),
                  physical.max.x + drawExtent, physical.clip.z});
    const float visibleMaxY =
        std::min({static_cast<float>(frame.extent.height),
                  physical.max.y + drawExtent, physical.clip.w});
    if (!std::isfinite(visibleMinX) || !std::isfinite(visibleMinY) ||
        !std::isfinite(visibleMaxX) || !std::isfinite(visibleMaxY) ||
        visibleMaxX <= visibleMinX || visibleMaxY <= visibleMinY)
      continue;
    return true;
  }
  return false;
}

} // namespace glass_ui::widget::vulkan_glass
