#pragma once

#include "GlassRequest.hpp"

#include <vector>

namespace glass_ui::widget {

class DrawTransformStore;

class GlassRequestQueue final {
public:
  void beginFrame(GlassBackdropStyle backdropStyle, float density);
  void submit(GlassRequest request);
  void applyTransforms(const DrawTransformStore &transforms);
  void seal();

  const std::vector<GlassRequest> &requests() const noexcept {
    return requests_;
  }
  const GlassBackdropStyle &backdropStyle() const noexcept {
    return backdropStyle_;
  }
  float density() const noexcept { return density_; }

private:
  std::vector<GlassRequest> requests_;
  GlassBackdropStyle backdropStyle_{};
  float density_ = 1.0f;
  bool sealed_ = false;
};

}
