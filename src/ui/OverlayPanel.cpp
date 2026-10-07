#include "OverlayPanel.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui {
namespace {

constexpr float kMaximumTransitionOvershoot = 0.06f;

bool contains(ImVec2 minimum, ImVec2 maximum, ImVec2 point) noexcept {
  return point.x >= minimum.x && point.x <= maximum.x &&
         point.y >= minimum.y && point.y <= maximum.y;
}

}

void OverlayPanel::configure(ImVec2 position, ImVec2 size) {
  State &state = state_;
  const bool firstConfiguration = !state.configured;
  state.expandedPosition = position;
  state.expandedSize = size;
  if (firstConfiguration) {
    state.floatingPosition =
        ImVec2(position.x + (size.x - state.floatingDiameter) * 0.5f,
               position.y + (size.y - state.floatingDiameter) * 0.5f);
    state.position = position;
    state.size = size;
    state.floating = false;
    state.floatingAmount = 0.0f;
    state.visualFloatingAmount = 0.0f;
  } else {
    updateGeometry();
  }
  state.motionVelocity = {};
  state.gesture = Gesture::None;
  state.configured = true;
}

void OverlayPanel::setTransition(float floatingAmount, float floatingDiameter,
                                 bool active) {
  State &state = state_;
  state.floatingDiameter = std::max(floatingDiameter, 1.0f);
  state.visualFloatingAmount =
      std::clamp(floatingAmount, -kMaximumTransitionOvershoot,
                 1.0f + kMaximumTransitionOvershoot);
  state.floatingAmount = std::clamp(floatingAmount, 0.0f, 1.0f);
  state.transitionActive = active;
  clampEndpoints();
  updateGeometry();
}

void OverlayPanel::setExpandedMargins(const ImVec4 &margins) noexcept {
  state_.expandedMargins =
      ImVec4(std::max(margins.x, 0.0f), std::max(margins.y, 0.0f),
             std::max(margins.z, 0.0f), std::max(margins.w, 0.0f));
}

void OverlayPanel::setMinimumExpandedSize(ImVec2 size) noexcept {
  state_.minimumExpandedSize =
      ImVec2(std::max(size.x, 1.0f), std::max(size.y, 1.0f));
}

void OverlayPanel::setResizeMode(bool enabled) noexcept {
  State &state = state_;
  state.resizeMode = enabled && !state.floating && !state.transitionActive &&
                     state.floatingAmount <= 0.001f;
  if (!state.resizeMode &&
      (state.gesture == Gesture::ResizeTopRight ||
       state.gesture == Gesture::ResizeBottomRight))
    state.gesture = Gesture::None;
}

void OverlayPanel::beginFrame(ImVec2 displaySize, PointerEventView events,
                              float deltaTime) {
  displaySize_ = displaySize;
  deltaTime_ = std::clamp(deltaTime, 1.0f / 1000.0f, 0.05f);
  pointerReleased_ = false;
  for (std::size_t index = 0; index < events.count; ++index) {
    const PointerEvent &event = events.events[index];
    pointerPosition_ = event.position;
    if (event.phase == PointerPhase::Down)
      pointerDown_ = true;
    else if (event.phase == PointerPhase::Up) {
      pointerDown_ = false;
      pointerReleased_ = true;
    } else if (event.phase == PointerPhase::Cancel) {
      cancelInput();
      pointerPosition_ = ImVec2(-FLT_MAX, -FLT_MAX);
    }
  }
  clampEndpoints();
  updateGeometry();
  update();
}

void OverlayPanel::handleTitleBarInput(const ImVec2 &minimum,
                                       const ImVec2 &maximum, float dragSlop) {
  State &state = state_;
  if (state.floating || state.transitionActive ||
      state.floatingAmount > 0.001f ||
      state.resizeMode ||
      maximum.x <= minimum.x || maximum.y <= minimum.y)
    return;

  const bool hovered = contains(minimum, maximum, pointerPosition_);
  if (hovered && state.dragTravel <= state.dragSlop &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    setFloating(true);
    return;
  }

  if (state.gesture == Gesture::None && hovered &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
    beginMove(dragSlop);
}

void OverlayPanel::handleFloatingInput(const ImVec2 &minimum,
                                       const ImVec2 &maximum, float dragSlop) {
  State &state = state_;
  if (!state.floating || state.transitionActive ||
      state.floatingAmount < 0.999f)
    return;

  if (state.gesture == Gesture::None && maximum.x > minimum.x &&
      maximum.y > minimum.y &&
      contains(minimum, maximum, pointerPosition_)) {
    const ImVec2 mouse = pointerPosition_;
    const ImVec2 center((minimum.x + maximum.x) * 0.5f,
                        (minimum.y + maximum.y) * 0.5f);
    const float radius =
        std::min(maximum.x - minimum.x, maximum.y - minimum.y) * 0.5f;
    const float deltaX = mouse.x - center.x;
    const float deltaY = mouse.y - center.y;
    if (deltaX * deltaX + deltaY * deltaY <= radius * radius &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      beginMove(dragSlop);
  }
}

void OverlayPanel::handleResizeInput(PanelResizeCorner corner, bool pressed) {
  if (!pressed || !state_.resizeMode || state_.floating ||
      state_.transitionActive ||
      state_.floatingAmount > 0.001f || state_.gesture != Gesture::None)
    return;
  beginResize(corner);
}

bool OverlayPanel::isResizing(PanelResizeCorner corner) const noexcept {
  const Gesture gesture = state_.gesture;
  return (corner == PanelResizeCorner::TopRight &&
          gesture == Gesture::ResizeTopRight) ||
         (corner == PanelResizeCorner::BottomRight &&
          gesture == Gesture::ResizeBottomRight);
}

void OverlayPanel::reset() noexcept {
  state_ = {};
  displaySize_ = {};
  pointerPosition_ = ImVec2(-FLT_MAX, -FLT_MAX);
  pointerDown_ = pointerReleased_ = false;
}

void OverlayPanel::cancelInput() noexcept {
  state_.gesture = Gesture::None;
  state_.motionVelocity = {};
  state_.dragTravel = 0.0f;
  pointerDown_ = pointerReleased_ = false;
}

void OverlayPanel::update() {
  State &state = state_;
  state.motionVelocity = {};
  const bool moving = state.gesture == Gesture::MoveExpanded ||
                      state.gesture == Gesture::MoveFloating;
  const bool resizing = state.gesture == Gesture::ResizeTopRight ||
                        state.gesture == Gesture::ResizeBottomRight;
  if (moving && ImGui::IsMousePosValid(&pointerPosition_) &&
      (pointerDown_ || pointerReleased_)) {
    const ImVec2 delta(pointerPosition_.x - state.lastMouse.x,
                       pointerPosition_.y - state.lastMouse.y);
    state.dragTravel += std::sqrt(delta.x * delta.x + delta.y * delta.y);
    ImVec2 &position = state.gesture == Gesture::MoveFloating
                           ? state.floatingPosition
                           : state.expandedPosition;
    const ImVec2 previousPosition = position;
    position.x += delta.x;
    position.y += delta.y;
    state.lastMouse = pointerPosition_;
    clampEndpoints();
    state.motionVelocity =
        ImVec2((position.x - previousPosition.x) / deltaTime_,
               (position.y - previousPosition.y) / deltaTime_);
    updateGeometry();
  }
  if (resizing && ImGui::IsMousePosValid(&pointerPosition_) &&
      (pointerDown_ || pointerReleased_)) {
    updateResize(pointerPosition_);
    updateGeometry();
  }
  if (state.gesture != Gesture::None && !pointerDown_) {
    const bool activate = state.gesture == Gesture::MoveFloating &&
                          state.dragTravel <= state.dragSlop;
    state.gesture = Gesture::None;
    if (activate)
      setFloating(false);
  }
}

void OverlayPanel::updateResize(ImVec2 pointer) noexcept {
  State &state = state_;
  const ImVec2 delta(pointer.x - state.gestureStartPointer.x,
                     pointer.y - state.gestureStartPointer.y);
  const ImVec4 frame = expandedFrame();
  const float minimumWidth =
      std::min(state.minimumExpandedSize.x, frame.z - frame.x);
  const float minimumHeight =
      std::min(state.minimumExpandedSize.y, frame.w - frame.y);
  const float right = state.gestureStartPosition.x + state.gestureStartSize.x;
  const float bottom =
      state.gestureStartPosition.y + state.gestureStartSize.y;
  const float resizedRight =
      std::clamp(right + delta.x,
                 state.gestureStartPosition.x + minimumWidth, frame.z);
  if (state.gesture == Gesture::ResizeTopRight) {
    const float top = std::clamp(state.gestureStartPosition.y + delta.y,
                                 frame.y, bottom - minimumHeight);
    state.expandedPosition.y = top;
    state.expandedSize =
        ImVec2(resizedRight - state.gestureStartPosition.x, bottom - top);
  } else {
    const float resizedBottom =
        std::clamp(bottom + delta.y,
                   state.gestureStartPosition.y + minimumHeight, frame.w);
    state.expandedSize =
        ImVec2(resizedRight - state.gestureStartPosition.x,
               resizedBottom - state.gestureStartPosition.y);
  }
}

void OverlayPanel::updateGeometry() noexcept {
  State &state = state_;
  const float amount = state.visualFloatingAmount;
  state.position = ImVec2(
      state.expandedPosition.x +
          (state.floatingPosition.x - state.expandedPosition.x) * amount,
      state.expandedPosition.y +
          (state.floatingPosition.y - state.expandedPosition.y) * amount);
  state.size =
      ImVec2(state.expandedSize.x +
                 (state.floatingDiameter - state.expandedSize.x) * amount,
             state.expandedSize.y +
                 (state.floatingDiameter - state.expandedSize.y) * amount);
}

void OverlayPanel::setFloating(bool floating) noexcept {
  State &state = state_;
  if (state.floating == floating)
    return;
  if (floating) {
    state.floatingPosition =
        ImVec2(state.expandedPosition.x +
                   (state.expandedSize.x - state.floatingDiameter) * 0.5f,
               state.expandedPosition.y +
                   (state.expandedSize.y - state.floatingDiameter) * 0.5f);
  } else {
    const ImVec2 floatingCenter(
        state.floatingPosition.x + state.floatingDiameter * 0.5f,
        state.floatingPosition.y + state.floatingDiameter * 0.5f);
    state.expandedPosition =
        ImVec2(floatingCenter.x - state.expandedSize.x * 0.5f,
               floatingCenter.y - state.expandedSize.y * 0.5f);
  }
  state.floating = floating;
  state.gesture = Gesture::None;
  state.resizeMode = false;
  clampEndpoints();
}

void OverlayPanel::beginMove(float dragSlop) noexcept {
  State &state = state_;
  state.gesture = state.floating ? Gesture::MoveFloating
                                 : Gesture::MoveExpanded;
  state.lastMouse = pointerPosition_;
  state.dragTravel = 0.0f;
  state.dragSlop = std::max(dragSlop, 0.0f);
}

void OverlayPanel::beginResize(PanelResizeCorner corner) noexcept {
  State &state = state_;
  state.gesture = corner == PanelResizeCorner::TopRight
                      ? Gesture::ResizeTopRight
                      : Gesture::ResizeBottomRight;
  state.gestureStartPointer = pointerPosition_;
  state.gestureStartPosition = state.expandedPosition;
  state.gestureStartSize = state.expandedSize;
}

ImVec4 OverlayPanel::expandedFrame() const noexcept {
  const State &state = state_;
  const float displayWidth = std::max(displaySize_.x - 1.0f, 1.0f);
  const float displayHeight = std::max(displaySize_.y - 1.0f, 1.0f);
  const float left =
      std::clamp(state.expandedMargins.x, 0.0f, displayWidth - 1.0f);
  const float right =
      std::clamp(state.expandedMargins.z, 0.0f, displayWidth - left - 1.0f);
  const float top =
      std::clamp(state.expandedMargins.y, 0.0f, displayHeight - 1.0f);
  const float bottom =
      std::clamp(state.expandedMargins.w, 0.0f, displayHeight - top - 1.0f);
  return ImVec4(left, top, displayWidth - right, displayHeight - bottom);
}

void OverlayPanel::clampEndpoints() noexcept {
  State &state = state_;
  const ImVec4 frame = expandedFrame();
  const float expandedWidth = std::max(frame.z - frame.x, 1.0f);
  const float expandedHeight = std::max(frame.w - frame.y, 1.0f);
  const float minimumWidth =
      std::min(state.minimumExpandedSize.x, expandedWidth);
  const float minimumHeight =
      std::min(state.minimumExpandedSize.y, expandedHeight);
  state.expandedSize.x =
      std::clamp(state.expandedSize.x, minimumWidth, expandedWidth);
  state.expandedSize.y =
      std::clamp(state.expandedSize.y, minimumHeight, expandedHeight);
  state.expandedPosition.x =
      std::clamp(state.expandedPosition.x, frame.x,
                 frame.z - state.expandedSize.x);
  state.expandedPosition.y =
      std::clamp(state.expandedPosition.y, frame.y,
                 frame.w - state.expandedSize.y);

  const float displayWidth = std::max(displaySize_.x - 1.0f, 1.0f);
  const float displayHeight = std::max(displaySize_.y - 1.0f, 1.0f);
  const float diameter = std::clamp(state.floatingDiameter, 1.0f,
                                    std::min(displayWidth, displayHeight));
  state.floatingDiameter = diameter;
  state.floatingPosition.x =
      std::clamp(state.floatingPosition.x, 0.0f, displayWidth - diameter);
  state.floatingPosition.y =
      std::clamp(state.floatingPosition.y, 0.0f, displayHeight - diameter);
}

}
