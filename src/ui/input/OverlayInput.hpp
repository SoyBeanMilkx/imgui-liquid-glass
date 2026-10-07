#pragma once

#include "AndroidIme.hpp"
#include "InputRegion.hpp"
#include "InputSurface.hpp"
#include "PointerEvent.hpp"
#include "touch/TouchBridge.hpp"

namespace glass_ui {

class InputBridge;

// Owns input-source selection and session transitions at frame boundaries.
class OverlayInput final {
public:
  void initialize(InputBridge &input) noexcept;
  void configureImGui(ImGuiIO &io);
  void unconfigureImGui() noexcept;
  void poll(widget::TextInputSession &session, ImGuiIO &io,
            const InputSurface &surface);
  void commit(widget::TextInputSession &session, ImVec2 displaySize,
              const InputRegionSet &regions, ImVec2 imeCursorPosition) noexcept;
  void suspend(widget::TextInputSession &session) noexcept;

  PointerEventView events() const noexcept { return pointerEvents_.view(); }
  ImVec2 imeCursorPosition() const noexcept { return ime_.cursorPosition(); }

private:
  void publishRegions(const InputRegionSet &regions, ImVec2 displaySize) noexcept;

  InputBridge *toucher_ = nullptr;
  AndroidIme ime_;
  TouchBridge touch_;
  PointerEventBatch pointerEvents_;
  uint64_t token_ = 0;
  bool focused_ = true;
};

}
