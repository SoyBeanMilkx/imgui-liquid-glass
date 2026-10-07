#include "OverlayInput.hpp"

#include "WindowFocusInfo.hpp"
#include "touch/InputBridge.hpp"
#include "ui/widget/foundation/interaction/TextInputSession.hpp"

#include <algorithm>

namespace glass_ui {

void OverlayInput::initialize(InputBridge &input) noexcept {
  window::initializeFocusInfo();
  toucher_ = &input;
}

void OverlayInput::configureImGui(ImGuiIO &io) {
  if (toucher_)
    toucher_->configureImGui(io);
  ime_.configureImGui();
}

void OverlayInput::unconfigureImGui() noexcept { ime_.unconfigureImGui(); }

void OverlayInput::poll(widget::TextInputSession &session, ImGuiIO &io,
                        const InputSurface &surface) {
  pointerEvents_.clear();
  const bool focused = window::queryFocusState() != window::FocusState::Unfocused;
  if (focused != focused_)
    io.AddFocusEvent(focused);
  focused_ = focused;
  if (!focused_)
    session.close();
  if (toucher_)
    toucher_->poll(io, surface, pointerEvents_, focused_ && !token_);
  if (focused_ && token_) {
    if (!touch_.poll(io, pointerEvents_))
      session.close();
  } else {
    touch_.clear();
  }
  // Pointer resets must finish before the IME appends its keyboard events.
  ime_.poll(session, io);
}

void OverlayInput::commit(widget::TextInputSession &session, ImVec2 displaySize,
                          const InputRegionSet &regions,
                          ImVec2 imeCursorPosition) noexcept {
  if (!focused_)
    session.close();
  const auto &request = session.request();
  ImGuiIO &io = ImGui::GetIO();
  // Retain the current source and editor through the whole pointer gesture.
  if (request.token != token_ && io.MouseDown[0]) {
    publishRegions(regions, displaySize);
    return;
  }
  if (request.token) {
    // Pause exclusive capture and enqueue app touch windows before showing IME.
    const bool paused = !toucher_ || toucher_->setCaptureEnabled(false);
    if (paused && touch_.commit(true, regions, displaySize) &&
        ime_.commit(request, displaySize, imeCursorPosition)) {
      token_ = request.token;
    } else if (!token_) {
      touch_.suspend();
      if (toucher_)
        toucher_->setCaptureEnabled(focused_);
    }
  } else if (ime_.commit(request, displaySize, imeCursorPosition) &&
             touch_.commit(false, {}, displaySize)) {
    if (token_)
      io.AddMouseButtonEvent(0, false);
    token_ = 0;
  }
  publishRegions(regions, displaySize);
}

void OverlayInput::publishRegions(const InputRegionSet &regions,
                                  ImVec2 displaySize) noexcept {
  if (!toucher_)
    return;
  const ImVec4 area = ime_.visibleArea();
  InputRegionSet clipped;
  for (std::size_t index = 0; index < regions.size(); ++index) {
    InputRegion region = regions.data()[index];
    const ImVec2 minimum(std::max(region.minimum.x, area.x * displaySize.x),
                         std::max(region.minimum.y, area.y * displaySize.y));
    const ImVec2 maximum(std::min(region.maximum.x, area.z * displaySize.x),
                         std::min(region.maximum.y, area.w * displaySize.y));
    if (maximum.x <= minimum.x || maximum.y <= minimum.y)
      continue;
    if (region.shape == InputRegionShape::Ellipse &&
        (minimum.x != region.minimum.x || minimum.y != region.minimum.y ||
         maximum.x != region.maximum.x || maximum.y != region.maximum.y))
      continue;
    region.minimum = minimum;
    region.maximum = maximum;
    clipped.add(region);
  }
  toucher_->publishRegions(clipped);
}

void OverlayInput::suspend(widget::TextInputSession &session) noexcept {
  session.close();
  if (ImGui::GetCurrentContext())
    ImGui::GetIO().ClearInputMouse();
  ime_.suspend();
  touch_.suspend();
  if (toucher_)
    toucher_->suspend();
  token_ = 0;
  pointerEvents_.clear();
}

}
