#include "SpringMotion.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {

bool stepSpring(SpringMotion &motion, float target,
                const SpringOptions &options, float deltaTime,
                bool reduceMotion) noexcept {
  if (reduceMotion) {
    motion.value = target;
    motion.velocity = 0.0f;
    return false;
  }

  const float response = std::max(options.responseSeconds, 0.02f);
  const float dampingRatio = std::clamp(options.dampingRatio, 0.0f, 4.0f);
  const float precision = std::max(options.precision, 0.0001f);
  const float duration = std::clamp(deltaTime, 0.0f, 0.05f);
  constexpr float kPi = 3.14159265358979323846f;
  constexpr float kMaximumStep = 1.0f / 240.0f;
  const float angularFrequency = 2.0f * kPi / response;
  const float stiffness = angularFrequency * angularFrequency;
  const float damping = 2.0f * dampingRatio * angularFrequency;
  const int stepCount =
      std::max(1, static_cast<int>(std::ceil(duration / kMaximumStep)));
  const float step = duration / static_cast<float>(stepCount);

  for (int index = 0; index < stepCount; ++index) {
    const float acceleration =
        stiffness * (target - motion.value) - damping * motion.velocity;
    motion.velocity += acceleration * step;
    motion.value += motion.velocity * step;
  }

  const float velocityPrecision = std::max(precision / response, 0.05f);
  const bool active = std::abs(target - motion.value) > precision ||
                      std::abs(motion.velocity) > velocityPrecision;
  if (!active) {
    motion.value = target;
    motion.velocity = 0.0f;
  }
  return active;
}

}
