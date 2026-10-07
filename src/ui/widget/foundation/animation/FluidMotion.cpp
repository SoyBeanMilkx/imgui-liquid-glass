#include "FluidMotion.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {

FluidMotionResult FluidMotion::update(ImVec2 velocity,
                                      const FluidMotionOptions &options,
                                      float density, float deltaTime,
                                      bool reduceMotion) noexcept {
  const float safeDensity = std::max(density, 0.25f);
  const float velocityScale =
      std::max(options.velocityForFullEffect * safeDensity, 1.0f);
  const float targetX = std::clamp(velocity.x / velocityScale, -1.0f, 1.0f);
  const float targetY = std::clamp(velocity.y / velocityScale, -1.0f, 1.0f);
  const SpringOptions spring{options.responseSeconds, options.dampingRatio,
                             0.001f};
  const bool horizontalActive = stepSpring(
      horizontal_, targetX, spring, deltaTime, reduceMotion);
  const bool verticalActive =
      stepSpring(vertical_, targetY, spring, deltaTime, reduceMotion);

  const float horizontal = std::clamp(horizontal_.value, -1.25f, 1.25f);
  const float vertical = std::clamp(vertical_.value, -1.25f, 1.25f);
  const float stretch = std::max(options.maximumStretch, 0.0f);
  const float compression = std::max(options.maximumCompression, 0.0f);
  FluidMotionResult result;
  result.scale.x = std::max(
      0.85f, 1.0f + std::abs(horizontal) * stretch -
                 std::abs(vertical) * compression);
  result.scale.y = std::max(
      0.85f, 1.0f + std::abs(vertical) * stretch -
                 std::abs(horizontal) * compression);
  const float offset = std::max(options.maximumOffset, 0.0f) * safeDensity;
  result.offset = ImVec2(-horizontal * offset, -vertical * offset);
  const float deformation =
      std::clamp(options.maximumDeformation, 0.0f, 0.35f);
  result.deformation =
      ImVec2(horizontal * deformation, vertical * deformation);
  result.intensity =
      std::clamp(std::sqrt(horizontal * horizontal + vertical * vertical),
                 0.0f, 1.0f);
  result.active =
      horizontalActive || verticalActive || result.intensity > 0.001f;
  return result;
}

void FluidMotion::reset() noexcept {
  horizontal_ = {};
  vertical_ = {};
}

}
