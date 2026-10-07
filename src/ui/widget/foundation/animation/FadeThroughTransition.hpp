#pragma once

#include "AnimationStore.hpp"

namespace glass_ui::widget {

struct FadeThroughResult {
  int visibleValue = 0;
  float opacity = 1.0f;
  bool active = false;
};

class FadeThroughTransition final {
public:
  FadeThroughResult update(AnimationStore &animations, ImGuiID id,
                           uint32_t channel, int targetValue,
                           float durationSeconds, float deltaTime,
                           uint64_t frameNumber, bool reduceMotion);
  void reset(int visibleValue) noexcept;

private:
  int visibleValue_ = 0;
  bool initialized_ = false;
};

}
