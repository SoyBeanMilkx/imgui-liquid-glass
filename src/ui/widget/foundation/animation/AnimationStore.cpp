#include "AnimationStore.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {

uint64_t AnimationStore::key(ImGuiID id, uint32_t channel) noexcept {
  return (static_cast<uint64_t>(id) << 32u) | channel;
}

float AnimationStore::animate(ImGuiID id, uint32_t channel, float target,
                              float responseSeconds, float deltaTime,
                              uint64_t frameNumber, bool reduceMotion) {
  const auto [iterator, inserted] =
      entries_.try_emplace(key(id, channel), Entry{target, frameNumber});
  Entry &entry = iterator->second;
  entry.lastFrame = frameNumber;
  if (inserted)
    return entry.value;
  if (reduceMotion || responseSeconds <= 0.0f) {
    entry.value = target;
    return entry.value;
  }

  const float response = std::max(responseSeconds, 0.001f);
  const float step = 1.0f - std::exp(-std::max(deltaTime, 0.0f) / response);
  entry.value += (target - entry.value) * step;
  if (std::abs(target - entry.value) < 0.001f)
    entry.value = target;
  return entry.value;
}

AnimationTransition AnimationStore::transition(
    ImGuiID id, uint32_t channel, float target, float durationSeconds,
    float deltaTime, uint64_t frameNumber, bool reduceMotion) {
  const auto [iterator, inserted] = transitions_.try_emplace(
      key(id, channel),
      TransitionEntry{target, target, target, 0.0f, frameNumber});
  TransitionEntry &entry = iterator->second;
  entry.lastFrame = frameNumber;
  if (inserted)
    return AnimationTransition{target, 1.0f, false};

  if (target != entry.target) {
    entry.start = entry.value;
    entry.target = target;
    entry.elapsed = 0.0f;
  }

  if (reduceMotion || durationSeconds <= 0.0f) {
    entry.value = target;
    entry.start = target;
    entry.target = target;
    entry.elapsed = 0.0f;
    return AnimationTransition{target, 1.0f, false};
  }

  const float duration = std::max(durationSeconds, 0.001f);
  entry.elapsed = std::min(entry.elapsed + std::max(deltaTime, 0.0f),
                           duration);
  const float progress = entry.elapsed / duration;
  const float eased = progress * progress * (3.0f - 2.0f * progress);
  entry.value = entry.start + (entry.target - entry.start) * eased;
  if (progress >= 1.0f) {
    entry.value = entry.target;
    return AnimationTransition{entry.value, 1.0f, false};
  }
  return AnimationTransition{entry.value, progress, true};
}

SpringTransition AnimationStore::spring(
    ImGuiID id, uint32_t channel, float target, const SpringOptions &options,
    float deltaTime, uint64_t frameNumber, bool reduceMotion) {
  const auto [iterator, inserted] = springs_.try_emplace(
      key(id, channel), SpringEntry{SpringMotion{target, 0.0f}, frameNumber});
  SpringEntry &entry = iterator->second;
  entry.lastFrame = frameNumber;
  if (inserted)
    return SpringTransition{target, 0.0f, false};

  const bool active =
      stepSpring(entry.motion, target, options, deltaTime, reduceMotion);
  return SpringTransition{entry.motion.value, entry.motion.velocity, active};
}

void AnimationStore::endFrame(uint64_t frameNumber) {
  constexpr uint64_t kCollectionInterval = 120;
  constexpr uint64_t kUnusedFrameLifetime = 300;
  if (frameNumber < lastCollectionFrame_ + kCollectionInterval)
    return;

  for (auto iterator = entries_.begin(); iterator != entries_.end();) {
    if (frameNumber > iterator->second.lastFrame + kUnusedFrameLifetime)
      iterator = entries_.erase(iterator);
    else
      ++iterator;
  }
  for (auto iterator = transitions_.begin(); iterator != transitions_.end();) {
    if (frameNumber > iterator->second.lastFrame + kUnusedFrameLifetime)
      iterator = transitions_.erase(iterator);
    else
      ++iterator;
  }
  for (auto iterator = springs_.begin(); iterator != springs_.end();) {
    if (frameNumber > iterator->second.lastFrame + kUnusedFrameLifetime)
      iterator = springs_.erase(iterator);
    else
      ++iterator;
  }
  lastCollectionFrame_ = frameNumber;
}

void AnimationStore::clear() noexcept {
  entries_.clear();
  transitions_.clear();
  springs_.clear();
  lastCollectionFrame_ = 0;
}

}
