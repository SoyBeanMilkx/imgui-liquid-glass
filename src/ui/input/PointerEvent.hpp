#pragma once

#include "imgui.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace glass_ui {

enum class PointerPhase : uint8_t {
  Down,
  Move,
  Up,
  Cancel,
};

struct PointerEvent {
  PointerPhase phase = PointerPhase::Move;
  ImVec2 position{};
  uint64_t timestampNs = 0;
};

struct PointerEventView {
  const PointerEvent *events = nullptr;
  std::size_t count = 0;
};

class PointerEventBatch final {
public:
  static constexpr std::size_t Capacity = 64;

  void clear() noexcept { count_ = 0; }

  bool push(const PointerEvent &event) noexcept {
    if (count_ >= events_.size())
      return false;
    events_[count_++] = event;
    return true;
  }

  PointerEventView view() const noexcept {
    return PointerEventView{events_.data(), count_};
  }

private:
  std::array<PointerEvent, Capacity> events_{};
  std::size_t count_ = 0;
};

}
