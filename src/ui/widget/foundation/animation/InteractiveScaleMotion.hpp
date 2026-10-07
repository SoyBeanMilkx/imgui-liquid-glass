#pragma once

#include "SpringMotion.hpp"

namespace glass_ui::widget {

struct InteractiveScaleOptions {
  float restingScale = 1.0f;
  float pressedScale = 0.90f;
  float entranceScale = 0.78f;
  SpringOptions entranceSpring{0.42f, 0.52f, 0.001f};
  SpringOptions pressSpring{0.16f, 0.82f, 0.001f};
  SpringOptions releaseSpring{0.28f, 0.58f, 0.001f};
};

struct InteractiveScaleResult {
  float scale = 1.0f;
  bool active = false;
};

class InteractiveScaleMotion final {
public:
  InteractiveScaleResult update(bool presented, bool pressed,
                                const InteractiveScaleOptions &options,
                                float deltaTime, bool reduceMotion) noexcept;
  void reset() noexcept;

private:
  SpringMotion motion_{1.0f, 0.0f};
  bool presented_ = false;
  bool entering_ = false;
  bool initialized_ = false;
};

}
