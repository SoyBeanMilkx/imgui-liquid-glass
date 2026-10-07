#pragma once

#include "SpringMotion.hpp"

#include "imgui.h"

namespace glass_ui::widget {

struct FluidMotionOptions {
  float responseSeconds = 0.28f;
  float dampingRatio = 0.62f;
  float velocityForFullEffect = 1500.0f;
  float maximumStretch = 0.04f;
  float maximumCompression = 0.014f;
  float maximumOffset = 3.0f;
  float maximumDeformation = 0.16f;
};

struct FluidMotionResult {
  ImVec2 scale = ImVec2(1.0f, 1.0f);
  ImVec2 offset{};
  ImVec2 deformation{};
  float intensity = 0.0f;
  bool active = false;
};

class FluidMotion final {
public:
  FluidMotionResult update(ImVec2 velocity,
                           const FluidMotionOptions &options, float density,
                           float deltaTime, bool reduceMotion) noexcept;
  void reset() noexcept;

private:
  SpringMotion horizontal_{};
  SpringMotion vertical_{};
};

}
