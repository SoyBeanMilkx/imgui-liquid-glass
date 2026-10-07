#pragma once

#include "LayoutTypes.hpp"

#include <cstddef>

namespace glass_ui::widget {

struct LinearLayoutOptions {
  LayoutDirection direction = LayoutDirection::Vertical;
  LayoutDistribution distribution = LayoutDistribution::Packed;
  ContentGravity gravity{};
  LayoutInsets padding{};
  float spacing = 0.0f;
};

class LinearLayout final {
public:
  LinearLayout(const LayoutRect &bounds, ImVec2 itemSize,
               std::size_t itemCount, float density,
               const LinearLayoutOptions &options = {}) noexcept;

  LayoutRect slot(std::size_t index) const noexcept;
  std::size_t size() const noexcept { return itemCount_; }

private:
  LayoutRect contentBounds_{};
  LayoutDirection direction_ = LayoutDirection::Vertical;
  std::size_t itemCount_ = 0;
  float itemMainExtent_ = 0.0f;
  float itemCrossExtent_ = 0.0f;
  float firstOffset_ = 0.0f;
  float gap_ = 0.0f;
  float crossOffset_ = 0.0f;
};

}
