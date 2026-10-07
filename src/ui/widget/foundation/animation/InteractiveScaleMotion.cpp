#include "InteractiveScaleMotion.hpp"

#include <algorithm>

namespace glass_ui::widget {

InteractiveScaleResult InteractiveScaleMotion::update(
    bool presented, bool pressed, const InteractiveScaleOptions &options,
    float deltaTime, bool reduceMotion) noexcept {
  const float restingScale = std::max(options.restingScale, 0.01f);
  const float pressedScale = std::max(options.pressedScale, 0.01f);
  const float entranceScale = std::max(options.entranceScale, 0.01f);

  if (!initialized_) {
    motion_ = SpringMotion{presented ? restingScale : entranceScale, 0.0f};
    presented_ = presented;
    initialized_ = true;
  }

  if (presented && !presented_) {
    motion_ = SpringMotion{entranceScale, 0.0f};
    entering_ = true;
  } else if (!presented) {
    motion_ = SpringMotion{entranceScale, 0.0f};
    entering_ = false;
  }
  presented_ = presented;

  if (!presented)
    return InteractiveScaleResult{restingScale, false};

  const SpringOptions &spring =
      pressed ? options.pressSpring
              : (entering_ ? options.entranceSpring : options.releaseSpring);
  const bool active = stepSpring(
      motion_, pressed ? pressedScale : restingScale, spring, deltaTime,
      reduceMotion);
  if (!pressed && !active)
    entering_ = false;
  return InteractiveScaleResult{motion_.value, active};
}

void InteractiveScaleMotion::reset() noexcept {
  motion_ = SpringMotion{1.0f, 0.0f};
  presented_ = false;
  entering_ = false;
  initialized_ = false;
}

}
