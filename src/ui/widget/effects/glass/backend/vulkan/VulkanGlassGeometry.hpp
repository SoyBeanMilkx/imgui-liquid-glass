#pragma once

#include "VulkanGlassBackend.hpp"

#include <array>
#include <vector>

namespace glass_ui::widget::vulkan_glass {

struct PhysicalRequest {
  ImVec2 min;
  ImVec2 max;
  ImVec4 clip;
  ImVec2 deformation;
  ImVec2 lightDirection;
  std::array<float, 4> radii{};
};

float requestDensity(const GlassRequest &request) noexcept;
PhysicalRequest transformRequest(const GlassRequest &request,
                                 const VulkanGlassFrameInfo &frame);
bool hasVisibleRequest(const VulkanGlassFrameInfo &frame,
                       const std::vector<GlassRequest> &requests);

} // namespace glass_ui::widget::vulkan_glass
