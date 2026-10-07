#include "TouchQueue.hpp"

#include <cmath>

namespace glass_ui {

void TouchQueue::push(const PointerEvent &event) noexcept {
  if (!std::isfinite(event.position.x) || !std::isfinite(event.position.y))
    return;
  if (overflow_) {
    if (event.phase != PointerPhase::Down)
      return;
    overflow_ = false;
  }
  if (count_ && event.phase == PointerPhase::Move) {
    PointerEvent &last = pending_[(begin_ + count_ - 1) % pending_.size()];
    if (last.phase == PointerPhase::Move) {
      last = event;
      return;
    }
  }
  if (count_ == pending_.size()) {
    clear();
    pending_[0] = PointerEvent{PointerPhase::Cancel, event.position,
                                event.timestampNs};
    count_ = 1;
    overflow_ = true;
    return;
  }
  pending_[(begin_ + count_) % pending_.size()] = event;
  ++count_;
}

void TouchQueue::clear() noexcept {
  begin_ = count_ = 0;
  overflow_ = false;
}

void TouchQueue::poll(ImGuiIO &io, PointerEventBatch &events) noexcept {
  events.clear();
  while (count_) {
    PointerEvent event = pending_[begin_];
    begin_ = (begin_ + 1) % pending_.size();
    --count_;
    event.position.x *= io.DisplaySize.x;
    event.position.y *= io.DisplaySize.y;
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMousePosEvent(event.position.x, event.position.y);
    events.push(event);
    if (event.phase == PointerPhase::Move)
      continue;
    if (event.phase == PointerPhase::Cancel)
      io.ClearInputMouse();
    else
      io.AddMouseButtonEvent(0, event.phase == PointerPhase::Down);
    break;
  }
}

}
