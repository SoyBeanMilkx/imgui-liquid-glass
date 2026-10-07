#pragma once

#include "imgui.h"

#include <array>
#include <cstdint>
#include <unordered_map>

namespace glass_ui::widget {

enum class ScrollGestureState : uint8_t {
  Idle,
  Possible,
  Dragging,
};

struct ScrollState {
  float offset = 0.0f;
  float maximum = 0.0f;
  float velocity = 0.0f;
  float gestureDelta = 0.0f;
  uint64_t lastFrame = 0;
  ScrollGestureState gesture = ScrollGestureState::Idle;
  bool suppressTap = false;
  bool releaseSuppressionNextFrame = false;
};

struct ScrollScope {
  ScrollState *state = nullptr;
  ImVec2 minimum{};
  ImVec2 maximum{};
  int vertexStart = 0;
  float visualOverscroll = 0.0f;
  float indicatorWidth = 0.0f;
  float indicatorMinimumLength = 0.0f;
  float indicatorInset = 0.0f;
  float indicatorMinimumVelocity = 0.0f;
  ImVec4 indicatorColor{};
};

class ScrollStateStore final {
public:
  void beginFrame() noexcept;
  ScrollState &acquire(ImGuiID id, uint64_t frameNumber);
  void pushScope(const ScrollScope &scope) noexcept;
  ScrollScope *currentScope() noexcept;
  void popScope() noexcept;
  void endFrame(uint64_t frameNumber);
  void clear() noexcept;

private:
  static constexpr std::size_t ScopeCapacity = 16;
  std::unordered_map<ImGuiID, ScrollState> states_;
  std::array<ScrollScope, ScopeCapacity> scopes_{};
  std::size_t scopeDepth_ = 0;
  uint64_t lastCollectionFrame_ = 0;
};

}
