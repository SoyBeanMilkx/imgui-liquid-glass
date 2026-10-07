#include "FadeThroughTransition.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {
namespace {

float smoothstep(float value) noexcept {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}

}

FadeThroughResult FadeThroughTransition::update(
    AnimationStore &animations, ImGuiID id, uint32_t channel,
    int targetValue, float durationSeconds, float deltaTime,
    uint64_t frameNumber, bool reduceMotion) {
  if (!initialized_) {
    visibleValue_ = targetValue;
    initialized_ = true;
  }

  const AnimationTransition transition = animations.transition(
      id, channel, static_cast<float>(targetValue), durationSeconds,
      deltaTime, frameNumber, reduceMotion);
  if (!transition.active) {
    visibleValue_ = targetValue;
    return FadeThroughResult{visibleValue_, 1.0f, false};
  }
  if (transition.progress >= 0.5f)
    visibleValue_ = targetValue;
  const float opacity =
      smoothstep(std::abs(transition.progress * 2.0f - 1.0f));
  return FadeThroughResult{visibleValue_, opacity, true};
}

void FadeThroughTransition::reset(int visibleValue) noexcept {
  visibleValue_ = visibleValue;
  initialized_ = true;
}

}
