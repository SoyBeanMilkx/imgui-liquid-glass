#include "GlassBlurPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

constexpr uint32_t maximumPairs(GlassQuality quality) noexcept {
  switch (quality) {
  case GlassQuality::Minimal:
    return 5;
  case GlassQuality::Balanced:
    return 8;
  case GlassQuality::Full:
  case GlassQuality::Auto:
    return kGlassGaussianPairCapacity;
  }
  return kGlassGaussianPairCapacity;
}

}
GlassBlurPlan makeGlassBlurPlan(float physicalRadius,
                                GlassQuality quality) noexcept {
  if (!std::isfinite(physicalRadius) || physicalRadius <= 0.01f)
    return {};

  // Blur runs at half resolution. Sigma is expressed in working texels.
  const float sigma = std::max(0.5f, physicalRadius * 0.25f);
  const uint32_t support =
      std::max(1u, static_cast<uint32_t>(std::ceil(sigma * 3.0f)));
  GlassBlurPlan plan;
  plan.pairCount = std::min((support + 1u) / 2u, maximumPairs(quality));

  float normalization = 1.0f;
  for (uint32_t pair = 0; pair < plan.pairCount; ++pair) {
    const uint32_t firstIndex = pair * 2u + 1u;
    const uint32_t secondIndex = firstIndex + 1u;
    const float firstWeight = std::exp(
        -0.5f * static_cast<float>(firstIndex * firstIndex) / (sigma * sigma));
    const float secondWeight =
        secondIndex <= support
            ? std::exp(-0.5f * static_cast<float>(secondIndex * secondIndex) /
                       (sigma * sigma))
            : 0.0f;
    const float combinedWeight = firstWeight + secondWeight;
    plan.pairWeights[pair] = combinedWeight;
    plan.pairOffsets[pair] = (static_cast<float>(firstIndex) * firstWeight +
                              static_cast<float>(secondIndex) * secondWeight) /
                             combinedWeight;
    normalization += combinedWeight * 2.0f;
  }

  plan.centerWeight = 1.0f / normalization;
  for (uint32_t pair = 0; pair < plan.pairCount; ++pair)
    plan.pairWeights[pair] /= normalization;
  return plan;
}

}
