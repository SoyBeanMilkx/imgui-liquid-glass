#include "GestureArena.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui::widget {

void GestureArena::beginFrame(PointerEventView events,
                              uint64_t frameNumber) noexcept {
  frameNumber_ = frameNumber;
  downThisFrame_ = false;
  releasedThisFrame_ = false;
  cancelledThisFrame_ = false;
  tapScopeDepth_ = 0;
  tapSuppressionCount_ = 0;

  for (std::size_t index = 0; index < events.count; ++index) {
    const PointerEvent &event = events.events[index];
    pointerInputSeen_ = true;
    switch (event.phase) {
    case PointerPhase::Down:
      resetSequence();
      sequenceState_ = SequenceState::Possible;
      start_ = event.position;
      last_ = event.position;
      lastTimestampNs_ = event.timestampNs;
      downThisFrame_ = true;
      break;
    case PointerPhase::Move:
      if (sequenceState_ != SequenceState::Idle)
        updatePointer(event);
      break;
    case PointerPhase::Up:
      if (sequenceState_ != SequenceState::Idle) {
        updatePointer(event);
        releasedThisFrame_ = true;
      }
      break;
    case PointerPhase::Cancel:
      if (sequenceState_ != SequenceState::Idle)
        cancelledThisFrame_ = true;
      break;
    }
  }
}

void GestureArena::endFrame() noexcept {
  for (std::size_t index = 0; index < contenderCount_;) {
    if (contenders_[index].lastSeenFrame != frameNumber_) {
      contenders_[index] = contenders_[--contenderCount_];
      continue;
    }
    ++index;
  }
  if (releasedThisFrame_ || cancelledThisFrame_ || contenderCount_ == 0)
    resetSequence();
  tapScopeDepth_ = 0;
  tapSuppressionCount_ = 0;
}

HorizontalDragState GestureArena::horizontalDrag(
    ImGuiID id, const ImVec2 &minimum, const ImVec2 &maximum,
    const HorizontalDragParameters &parameters) noexcept {
  const DragState state = drag(id, Axis::Horizontal, minimum, maximum,
                               parameters);
  return HorizontalDragState{state.tracking, state.active, state.released,
                             state.cancelled, state.consumeTap, state.delta.x,
                             state.velocity.x, state.position};
}

VerticalDragState GestureArena::verticalDrag(
    ImGuiID id, const ImVec2 &minimum, const ImVec2 &maximum,
    const VerticalDragParameters &parameters) noexcept {
  const DragState state =
      drag(id, Axis::Vertical, minimum, maximum, parameters);
  return VerticalDragState{state.tracking, state.active, state.released,
                           state.cancelled, state.consumeTap, state.delta.y,
                           state.velocity.y, state.position};
}

DragState GestureArena::drag(ImGuiID id, Axis axis, const ImVec2 &minimum,
                             const ImVec2 &maximum,
                             const DragParameters &parameters) noexcept {
  DragState result;
  result.position = last_;
  if (id == 0 || maximum.x <= minimum.x || maximum.y <= minimum.y ||
      sequenceState_ == SequenceState::Idle)
    return result;

  Contender *contender = findContender(id, axis);
  if (!contender && downThisFrame_ && contains(minimum, maximum, start_)) {
    addContender(id, axis, parameters);
    contender = findContender(id, axis);
  }
  if (!contender)
    return result;

  contender->parameters = parameters;
  contender->lastSeenFrame = frameNumber_;
  if (!downThisFrame_)
    resolveWinner();

  result.delta = ImVec2(last_.x - start_.x, last_.y - start_.y);
  result.velocity = velocity_;
  result.position = last_;
  if (cancelledThisFrame_) {
    result.cancelled = true;
    return result;
  }
  if (sequenceState_ == SequenceState::Possible) {
    result.tracking = !releasedThisFrame_;
    result.cancelled = releasedThisFrame_;
    return result;
  }
  if (winner_ != id || winnerAxis_ != axis) {
    result.cancelled = true;
    return result;
  }

  result.tracking = !releasedThisFrame_;
  result.active = !releasedThisFrame_;
  result.released = releasedThisFrame_;
  result.consumeTap = true;
  return result;
}

void GestureArena::pushTapScope(bool suppressed) noexcept {
  if (tapScopeDepth_ >= tapScopes_.size())
    return;
  tapScopes_[tapScopeDepth_++] = suppressed;
  if (suppressed)
    ++tapSuppressionCount_;
}

void GestureArena::popTapScope() noexcept {
  if (tapScopeDepth_ == 0)
    return;
  if (tapScopes_[--tapScopeDepth_] && tapSuppressionCount_ > 0)
    --tapSuppressionCount_;
}

void GestureArena::clear() noexcept {
  frameNumber_ = 0;
  resetSequence();
  pointerInputSeen_ = false;
  tapScopeDepth_ = 0;
  tapSuppressionCount_ = 0;
}

bool GestureArena::contains(const ImVec2 &minimum, const ImVec2 &maximum,
                            const ImVec2 &point) noexcept {
  return point.x >= minimum.x && point.x <= maximum.x &&
         point.y >= minimum.y && point.y <= maximum.y;
}

GestureArena::Contender *GestureArena::findContender(ImGuiID id,
                                                     Axis axis) noexcept {
  for (std::size_t index = 0; index < contenderCount_; ++index) {
    Contender &contender = contenders_[index];
    if (contender.id == id && contender.axis == axis)
      return &contender;
  }
  return nullptr;
}

void GestureArena::addContender(
    ImGuiID id, Axis axis, const DragParameters &parameters) noexcept {
  if (contenderCount_ >= contenders_.size())
    return;
  contenders_[contenderCount_++] =
      Contender{id, axis, parameters, frameNumber_};
}

void GestureArena::resolveWinner() noexcept {
  if (sequenceState_ != SequenceState::Possible || downThisFrame_)
    return;

  const float deltaX = last_.x - start_.x;
  const float deltaY = last_.y - start_.y;
  const float absoluteX = std::abs(deltaX);
  const float absoluteY = std::abs(deltaY);
  const Axis dominant =
      absoluteX >= absoluteY ? Axis::Horizontal : Axis::Vertical;
  const Contender *winner = nullptr;
  for (std::size_t index = 0; index < contenderCount_; ++index) {
    const Contender &candidate = contenders_[index];
    if (candidate.axis != dominant)
      continue;
    const float primary = dominant == Axis::Horizontal ? absoluteX : absoluteY;
    const float secondary = dominant == Axis::Horizontal ? absoluteY : absoluteX;
    const float slop = std::max(candidate.parameters.touchSlop, 0.0f);
    const float ratio = std::max(candidate.parameters.directionRatio, 1.0f);
    if (primary < slop || primary < secondary * ratio)
      continue;
    if (!winner || candidate.parameters.priority > winner->parameters.priority)
      winner = &candidate;
  }
  if (!winner)
    return;
  winner_ = winner->id;
  winnerAxis_ = winner->axis;
  sequenceState_ = SequenceState::Active;
}

void GestureArena::resetSequence() noexcept {
  sequenceState_ = SequenceState::Idle;
  contenderCount_ = 0;
  winner_ = 0;
  winnerAxis_ = Axis::Horizontal;
  start_ = {};
  last_ = {};
  velocity_ = {};
  lastTimestampNs_ = 0;
  downThisFrame_ = false;
  releasedThisFrame_ = false;
  cancelledThisFrame_ = false;
}

void GestureArena::updatePointer(const PointerEvent &event) noexcept {
  if (event.timestampNs > lastTimestampNs_) {
    const double seconds =
        static_cast<double>(event.timestampNs - lastTimestampNs_) * 1.0e-9;
    if (seconds > 0.0 && seconds <= 0.25) {
      const ImVec2 instantaneous(
          static_cast<float>((event.position.x - last_.x) / seconds),
          static_cast<float>((event.position.y - last_.y) / seconds));
      velocity_.x = velocity_.x * 0.65f + instantaneous.x * 0.35f;
      velocity_.y = velocity_.y * 0.65f + instantaneous.y * 0.35f;
    }
  }
  last_ = event.position;
  lastTimestampNs_ = event.timestampNs;
}

}
