#pragma once

#include "imgui.h"
#include "ui/widget/foundation/animation/SpringMotion.hpp"

#include <cstdint>
#include <unordered_map>

namespace glass_ui::widget {

struct AnimationTransition {
  float value = 0.0f;
  float progress = 1.0f;
  bool active = false;
};

struct SpringTransition {
  float value = 0.0f;
  float velocity = 0.0f;
  bool active = false;
};

class AnimationStore final {
public:
  float animate(ImGuiID id, uint32_t channel, float target,
                float responseSeconds, float deltaTime, uint64_t frameNumber,
                bool reduceMotion);
  AnimationTransition transition(ImGuiID id, uint32_t channel, float target,
                                 float durationSeconds, float deltaTime,
                                 uint64_t frameNumber, bool reduceMotion);
  SpringTransition spring(ImGuiID id, uint32_t channel, float target,
                          const SpringOptions &options, float deltaTime,
                          uint64_t frameNumber, bool reduceMotion);
  void endFrame(uint64_t frameNumber);
  void clear() noexcept;

private:
  struct Entry {
    float value = 0.0f;
    uint64_t lastFrame = 0;
  };

  struct TransitionEntry {
    float value = 0.0f;
    float start = 0.0f;
    float target = 0.0f;
    float elapsed = 0.0f;
    uint64_t lastFrame = 0;
  };

  struct SpringEntry {
    SpringMotion motion;
    uint64_t lastFrame = 0;
  };

  static uint64_t key(ImGuiID id, uint32_t channel) noexcept;

  std::unordered_map<uint64_t, Entry> entries_;
  std::unordered_map<uint64_t, TransitionEntry> transitions_;
  std::unordered_map<uint64_t, SpringEntry> springs_;
  uint64_t lastCollectionFrame_ = 0;
};

}
