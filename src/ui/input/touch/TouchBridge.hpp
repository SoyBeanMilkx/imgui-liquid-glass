#pragma once

#include "ui/input/InputRegion.hpp"
#include "ui/input/PointerEvent.hpp"

#include <memory>

namespace glass_ui {

// Android application-window touches, independent of the text editor bridge.
class TouchBridge final {
public:
  TouchBridge();
  ~TouchBridge();
  TouchBridge(const TouchBridge &) = delete;
  TouchBridge &operator=(const TouchBridge &) = delete;

  bool poll(ImGuiIO &io, PointerEventBatch &events) noexcept;
  bool commit(bool enabled, const InputRegionSet &regions, ImVec2 displaySize);
  void clear() noexcept;
  void suspend() noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};

}
