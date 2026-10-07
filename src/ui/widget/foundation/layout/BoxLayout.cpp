#include "BoxLayout.hpp"

#include <algorithm>

namespace glass_ui::widget {
namespace {

void resolveAxis(float minimum, float maximum, float desired,
                 float density, LayoutAlignment alignment, float &resultMinimum,
                 float &resultMaximum) noexcept {
  const float available = std::max(maximum - minimum, 0.0f);
  const float extent =
      std::min(resolveLayoutExtent(desired, available, density), available);
  float offset = 0.0f;
  if (alignment == LayoutAlignment::Center)
    offset = (available - extent) * 0.5f;
  else if (alignment == LayoutAlignment::End)
    offset = available - extent;
  resultMinimum = minimum + offset;
  resultMaximum = alignment == LayoutAlignment::Stretch
                      ? maximum
                      : resultMinimum + extent;
}

}

LayoutRect resolveBoxLayout(const LayoutRect &bounds, ImVec2 contentSize,
                            float density,
                            const BoxLayoutOptions &options) noexcept {
  const float scale = std::max(density, 0.0f);
  const LayoutRect content = bounds.inset(options.padding, scale);
  LayoutRect result;
  resolveAxis(content.minimum.x, content.maximum.x, contentSize.x, scale,
              options.gravity.horizontal, result.minimum.x, result.maximum.x);
  resolveAxis(content.minimum.y, content.maximum.y, contentSize.y, scale,
              options.gravity.vertical, result.minimum.y, result.maximum.y);
  return result;
}

}
