#pragma once

#include "ui/input/PointerEvent.hpp"

namespace glass_ui {

// Normalized Android window events. Preserve button edges across ImGui frames.
// The caller synchronizes access between the UI and render threads.
class TouchQueue final {
public:
  void push(const PointerEvent &event) noexcept;
  void clear() noexcept;
  void poll(ImGuiIO &io, PointerEventBatch &events) noexcept;

private:
  std::array<PointerEvent, PointerEventBatch::Capacity> pending_{};
  std::size_t begin_ = 0;
  std::size_t count_ = 0;
  bool overflow_ = false;
};

}
