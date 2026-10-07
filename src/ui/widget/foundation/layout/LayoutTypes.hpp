#pragma once

#include "imgui.h"

#include <algorithm>

namespace glass_ui::widget {

enum class LayoutDirection {
  Horizontal,
  Vertical,
};

enum class LayoutAlignment {
  Start,
  Center,
  End,
  Stretch,
};

enum class LayoutDistribution {
  Packed,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

struct ContentGravity {
  LayoutAlignment horizontal = LayoutAlignment::Start;
  LayoutAlignment vertical = LayoutAlignment::Start;
};

struct LayoutInsets {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  static constexpr LayoutInsets all(float value) noexcept {
    return LayoutInsets{value, value, value, value};
  }
};

inline float resolveLayoutExtent(float requested, float available,
                                 float density) noexcept {
  if (requested > 0.0f)
    return requested * density;
  if (requested < 0.0f)
    return std::max(available + requested * density, 1.0f);
  return std::max(available, 1.0f);
}

struct LayoutRect {
  ImVec2 minimum{};
  ImVec2 maximum{};

  float width() const noexcept {
    return std::max(maximum.x - minimum.x, 0.0f);
  }
  float height() const noexcept {
    return std::max(maximum.y - minimum.y, 0.0f);
  }
  ImVec2 size() const noexcept { return ImVec2(width(), height()); }

  LayoutRect inset(const LayoutInsets &insets, float density) const noexcept {
    float left = minimum.x + std::max(insets.left, 0.0f) * density;
    float top = minimum.y + std::max(insets.top, 0.0f) * density;
    float right =
        maximum.x - std::max(insets.right, 0.0f) * density;
    float bottom =
        maximum.y - std::max(insets.bottom, 0.0f) * density;
    if (left > right)
      left = right = (left + right) * 0.5f;
    if (top > bottom)
      top = bottom = (top + bottom) * 0.5f;
    return LayoutRect{ImVec2(left, top), ImVec2(right, bottom)};
  }
};

}
