#pragma once

namespace glass_ui::widget {

struct SpringOptions {
  float responseSeconds = 0.30f;
  float dampingRatio = 0.75f;
  float precision = 0.01f;
};

struct SpringMotion {
  float value = 0.0f;
  float velocity = 0.0f;
};

bool stepSpring(SpringMotion &motion, float target,
                const SpringOptions &options, float deltaTime,
                bool reduceMotion) noexcept;

}
