#pragma once

#include "GlassStyle.hpp"

#include "imgui.h"

#include <cstdint>

namespace glass_ui::widget {

enum class GlassBackdropSource : uint8_t {
  Frame,
  PreviousContent,
};

struct GlassRequest {
  ImGuiID id = 0;
  ImVec2 min{};
  ImVec2 max{};
  CornerRadii radii{};
  GlassShapeKind shape = GlassShapeKind::RoundedRect;
  GlassStyle style{};
  ImVec4 clipRect{};
  ImVec2 deformation{};
  ImDrawList *drawList = nullptr;
  int drawCommandOffset = 0;
  GlassBackdropSource backdropSource = GlassBackdropSource::Frame;
  float density = 1.0f;
  float opacity = 1.0f;
  float blurMix = 1.0f;
};

}
