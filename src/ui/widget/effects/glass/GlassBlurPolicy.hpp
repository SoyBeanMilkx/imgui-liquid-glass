#pragma once

#include "GlassStyle.hpp"

#include <array>
#include <cstdint>

namespace glass_ui::widget {

inline constexpr uint32_t kGlassGaussianPairCapacity = 12;

struct GlassBlurPlan {
  uint32_t pairCount = 0;
  float centerWeight = 1.0f;
  std::array<float, kGlassGaussianPairCapacity> pairWeights{};
  std::array<float, kGlassGaussianPairCapacity> pairOffsets{};
};

GlassBlurPlan makeGlassBlurPlan(float physicalRadius,
                                GlassQuality quality) noexcept;

}
