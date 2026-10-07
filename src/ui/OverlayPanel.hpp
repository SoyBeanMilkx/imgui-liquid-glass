#pragma once

#include "imgui.h"
#include "ui/input/PointerEvent.hpp"

#include <cfloat>

namespace glass_ui {

enum class PanelResizeCorner {
  TopRight,
  BottomRight,
};

class OverlayPanel final {
public:
  void configure(ImVec2 position, ImVec2 size);
  void setTransition(float floatingAmount, float floatingDiameter,
                     bool active);
  void setExpandedMargins(const ImVec4 &margins) noexcept;
  void setMinimumExpandedSize(ImVec2 size) noexcept;
  void setResizeMode(bool enabled) noexcept;
  void beginFrame(ImVec2 displaySize, PointerEventView events, float deltaTime);
  void handleTitleBarInput(const ImVec2 &minimum, const ImVec2 &maximum,
                           float dragSlop);
  void handleFloatingInput(const ImVec2 &minimum, const ImVec2 &maximum,
                           float dragSlop);
  void handleResizeInput(PanelResizeCorner corner, bool pressed);
  bool isFloating() const noexcept { return state_.floating; }
  bool floatingPressed() const noexcept {
    return state_.gesture == Gesture::MoveFloating;
  }
  bool resizeMode() const noexcept { return state_.resizeMode; }
  bool transitionActive() const noexcept { return state_.transitionActive; }
  bool isResizing(PanelResizeCorner corner) const noexcept;
  ImVec2 position() const noexcept { return state_.position; }
  ImVec2 size() const noexcept { return state_.size; }
  ImVec2 expandedPosition() const noexcept { return state_.expandedPosition; }
  ImVec2 expandedSize() const noexcept { return state_.expandedSize; }
  ImVec2 motionVelocity() const noexcept { return state_.motionVelocity; }
  ImVec2 pointerPosition() const noexcept { return pointerPosition_; }
  void cancelInput() noexcept;
  void reset() noexcept;

private:
  enum class Gesture {
    None,
    MoveExpanded,
    MoveFloating,
    ResizeTopRight,
    ResizeBottomRight,
  };

  struct State {
    ImVec2 expandedPosition{};
    ImVec2 expandedSize{};
    ImVec2 floatingPosition{};
    ImVec2 position{};
    ImVec2 size{};
    ImVec2 motionVelocity{};
    ImVec2 lastMouse{};
    ImVec2 gestureStartPointer{};
    ImVec2 gestureStartPosition{};
    ImVec2 gestureStartSize{};
    ImVec2 minimumExpandedSize = ImVec2(1.0f, 1.0f);
    ImVec4 expandedMargins{};
    float floatingDiameter = 1.0f;
    float floatingAmount = 0.0f;
    float visualFloatingAmount = 0.0f;
    float dragTravel = 0.0f;
    float dragSlop = 0.0f;
    Gesture gesture = Gesture::None;
    bool floating = false;
    bool resizeMode = false;
    bool transitionActive = false;
    bool configured = false;
  };

  void update();
  void updateGeometry() noexcept;
  void updateResize(ImVec2 pointer) noexcept;
  void setFloating(bool floating) noexcept;
  void beginMove(float dragSlop) noexcept;
  void beginResize(PanelResizeCorner corner) noexcept;
  ImVec4 expandedFrame() const noexcept;
  void clampEndpoints() noexcept;

  State state_{};
  ImVec2 displaySize_{};
  ImVec2 pointerPosition_ = ImVec2(-FLT_MAX, -FLT_MAX);
  float deltaTime_ = 1.0f / 60.0f;
  bool pointerDown_ = false;
  bool pointerReleased_ = false;
};

}
