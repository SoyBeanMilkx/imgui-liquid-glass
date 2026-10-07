#pragma once

#include "imgui.h"
#include "ui/input/PointerEvent.hpp"

#include <array>
#include <cstdint>

namespace glass_ui::widget {

struct DragParameters {
  float touchSlop = 10.0f;
  float directionRatio = 1.2f;
  int priority = 0;
};

struct DragState {
  bool tracking = false;
  bool active = false;
  bool released = false;
  bool cancelled = false;
  bool consumeTap = false;
  ImVec2 delta{};
  ImVec2 velocity{};
  ImVec2 position{};
};

struct HorizontalDragState {
  bool tracking = false;
  bool active = false;
  bool released = false;
  bool cancelled = false;
  bool consumeTap = false;
  float deltaX = 0.0f;
  float velocityX = 0.0f;
  ImVec2 position{};
};

struct VerticalDragState {
  bool tracking = false;
  bool active = false;
  bool released = false;
  bool cancelled = false;
  bool consumeTap = false;
  float deltaY = 0.0f;
  float velocityY = 0.0f;
  ImVec2 position{};
};

using HorizontalDragParameters = DragParameters;
using VerticalDragParameters = DragParameters;

class GestureArena final {
public:
  void beginFrame(PointerEventView events, uint64_t frameNumber) noexcept;
  void endFrame() noexcept;

  HorizontalDragState horizontalDrag(
      ImGuiID id, const ImVec2 &minimum, const ImVec2 &maximum,
      const HorizontalDragParameters &parameters) noexcept;
  VerticalDragState verticalDrag(
      ImGuiID id, const ImVec2 &minimum, const ImVec2 &maximum,
      const VerticalDragParameters &parameters) noexcept;

  void pushTapScope(bool suppressed) noexcept;
  void popTapScope() noexcept;
  bool tapsSuppressed() const noexcept { return tapSuppressionCount_ > 0; }
  bool hasPointerInput() const noexcept { return pointerInputSeen_; }
  void clear() noexcept;

private:
  enum class Axis : uint8_t { Horizontal, Vertical };
  enum class SequenceState : uint8_t { Idle, Possible, Active };

  struct Contender {
    ImGuiID id = 0;
    Axis axis = Axis::Horizontal;
    DragParameters parameters{};
    uint64_t lastSeenFrame = 0;
  };

  static constexpr std::size_t ContenderCapacity = 24;
  static constexpr std::size_t TapScopeCapacity = 16;

  static bool contains(const ImVec2 &minimum, const ImVec2 &maximum,
                       const ImVec2 &point) noexcept;
  DragState drag(ImGuiID id, Axis axis, const ImVec2 &minimum,
                 const ImVec2 &maximum,
                 const DragParameters &parameters) noexcept;
  Contender *findContender(ImGuiID id, Axis axis) noexcept;
  void addContender(ImGuiID id, Axis axis,
                    const DragParameters &parameters) noexcept;
  void resolveWinner() noexcept;
  void resetSequence() noexcept;
  void updatePointer(const PointerEvent &event) noexcept;

  uint64_t frameNumber_ = 0;
  SequenceState sequenceState_ = SequenceState::Idle;
  std::array<Contender, ContenderCapacity> contenders_{};
  std::size_t contenderCount_ = 0;
  ImGuiID winner_ = 0;
  Axis winnerAxis_ = Axis::Horizontal;
  ImVec2 start_{};
  ImVec2 last_{};
  ImVec2 velocity_{};
  uint64_t lastTimestampNs_ = 0;
  bool downThisFrame_ = false;
  bool releasedThisFrame_ = false;
  bool cancelledThisFrame_ = false;
  bool pointerInputSeen_ = false;

  std::array<bool, TapScopeCapacity> tapScopes_{};
  std::size_t tapScopeDepth_ = 0;
  std::size_t tapSuppressionCount_ = 0;
};

}
