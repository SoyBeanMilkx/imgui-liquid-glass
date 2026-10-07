#pragma once

#include "imgui.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace glass_ui {

enum class InputRegionShape : uint8_t {
  Rectangle,
  Ellipse,
};

struct InputRegion {
  ImVec2 minimum{};
  ImVec2 maximum{};
  InputRegionShape shape = InputRegionShape::Rectangle;
  bool visible = false;
};

class InputRegionSet final {
public:
  static constexpr std::size_t Capacity = 16;

  void clear() noexcept { count_ = 0; }

  bool add(const InputRegion &region) noexcept {
    if (!region.visible || count_ >= regions_.size())
      return false;
    regions_[count_++] = region;
    return true;
  }

  const InputRegion *data() const noexcept { return regions_.data(); }
  std::size_t size() const noexcept { return count_; }
  bool empty() const noexcept { return count_ == 0; }

private:
  std::array<InputRegion, Capacity> regions_{};
  std::size_t count_ = 0;
};

}
