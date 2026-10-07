#include "LinearLayout.hpp"

#include <algorithm>

namespace glass_ui::widget {
namespace {

LayoutAlignment mainAlignment(const ContentGravity &gravity,
                              LayoutDirection direction) noexcept {
  return direction == LayoutDirection::Vertical ? gravity.vertical
                                                : gravity.horizontal;
}

LayoutAlignment crossAlignment(const ContentGravity &gravity,
                               LayoutDirection direction) noexcept {
  return direction == LayoutDirection::Vertical ? gravity.horizontal
                                                : gravity.vertical;
}

float alignmentOffset(LayoutAlignment alignment, float remaining) noexcept {
  if (alignment == LayoutAlignment::Center)
    return remaining * 0.5f;
  if (alignment == LayoutAlignment::End)
    return remaining;
  return 0.0f;
}

}

LinearLayout::LinearLayout(const LayoutRect &bounds, ImVec2 itemSize,
                           std::size_t itemCount, float density,
                           const LinearLayoutOptions &options) noexcept
    : contentBounds_(bounds.inset(options.padding, std::max(density, 0.0f))),
      direction_(options.direction), itemCount_(itemCount) {
  if (itemCount_ == 0)
    return;

  const float scale = std::max(density, 0.0f);
  const bool vertical = direction_ == LayoutDirection::Vertical;
  const float availableMain =
      vertical ? contentBounds_.height() : contentBounds_.width();
  const float availableCross =
      vertical ? contentBounds_.width() : contentBounds_.height();
  itemMainExtent_ =
      std::max(vertical ? itemSize.y : itemSize.x, 0.0f) * scale;
  itemCrossExtent_ = std::min(
      std::max(vertical ? itemSize.x : itemSize.y, 0.0f) * scale,
      availableCross);
  const float minimumGap = std::max(options.spacing, 0.0f) * scale;
  const float gapTotal =
      minimumGap * static_cast<float>(itemCount_ > 0 ? itemCount_ - 1 : 0);
  const LayoutAlignment main = mainAlignment(options.gravity, direction_);
  if (options.distribution == LayoutDistribution::Packed &&
      main == LayoutAlignment::Stretch) {
    itemMainExtent_ =
        std::max((availableMain - gapTotal) / static_cast<float>(itemCount_),
                 0.0f);
  }

  const float occupied =
      itemMainExtent_ * static_cast<float>(itemCount_) + gapTotal;
  const float remaining = std::max(availableMain - occupied, 0.0f);
  gap_ = minimumGap;
  if (options.distribution == LayoutDistribution::SpaceBetween &&
      itemCount_ > 1) {
    gap_ += remaining / static_cast<float>(itemCount_ - 1);
  } else if (options.distribution == LayoutDistribution::SpaceAround) {
    const float space = remaining / static_cast<float>(itemCount_);
    firstOffset_ = space * 0.5f;
    gap_ += space;
  } else if (options.distribution == LayoutDistribution::SpaceEvenly) {
    const float space = remaining / static_cast<float>(itemCount_ + 1);
    firstOffset_ = space;
    gap_ += space;
  } else if (options.distribution == LayoutDistribution::Packed) {
    firstOffset_ = alignmentOffset(main, remaining);
  }

  const LayoutAlignment cross = crossAlignment(options.gravity, direction_);
  if (cross == LayoutAlignment::Stretch)
    itemCrossExtent_ = availableCross;
  else
    crossOffset_ = alignmentOffset(cross, availableCross - itemCrossExtent_);
}

LayoutRect LinearLayout::slot(std::size_t index) const noexcept {
  if (index >= itemCount_)
    return {};
  const float mainOffset =
      firstOffset_ + static_cast<float>(index) * (itemMainExtent_ + gap_);
  if (direction_ == LayoutDirection::Vertical) {
    const ImVec2 minimum(contentBounds_.minimum.x + crossOffset_,
                         contentBounds_.minimum.y + mainOffset);
    return LayoutRect{minimum,
                      ImVec2(minimum.x + itemCrossExtent_,
                             minimum.y + itemMainExtent_)};
  }
  const ImVec2 minimum(contentBounds_.minimum.x + mainOffset,
                       contentBounds_.minimum.y + crossOffset_);
  return LayoutRect{minimum, ImVec2(minimum.x + itemMainExtent_,
                                    minimum.y + itemCrossExtent_)};
}

}
