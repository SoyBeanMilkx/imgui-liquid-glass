#pragma once

#include "imgui.h"

#include <cstdint>

namespace glass_ui::widget {

enum class GlassQuality : uint8_t {
  Auto,
  Full,
  Balanced,
  Minimal,
};

enum class GlassShapeKind : uint8_t {
  RoundedRect,
  Capsule,
  Circle,
};

struct CornerRadii {
  float topLeft = 28.0f;
  float topRight = 28.0f;
  float bottomRight = 28.0f;
  float bottomLeft = 28.0f;

  static constexpr CornerRadii all(float radius) noexcept {
    return CornerRadii{radius, radius, radius, radius};
  }
};

struct GlassLighting {
  float innerShadow = 0.12f;
  float rimGlow = 0.20f;
  float specular = 0.35f;
  float outerShadow = 0.14f;
  float shadowExtent = 5.0f;
  float specularPower = 28.0f;
  ImVec2 lightDirection = ImVec2(-0.7071f, -0.7071f);
};

struct GlassStyle {
  float refractionHeight = 20.0f;
  float refractionAmount = -70.0f;
  float depthEffect = 0.3f;
  float chromaticAberration = 0.5f;
  float contrast = 0.0f;
  float whitePoint = 0.0f;
  float chromaMultiplier = 1.0f;
  ImVec4 tint = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
  GlassLighting lighting{};
};

struct GlassSurfaceEffect {
  ImVec2 pivot{};
  ImVec2 scale = ImVec2(1.0f, 1.0f);
  ImVec2 offset{};
  ImVec2 deformation{};
  bool enabled = false;
};

// All glass surfaces share one frame-scoped backdrop blur.
struct GlassBackdropStyle {
  float blurRadius = 0.01f;
  GlassQuality quality = GlassQuality::Auto;
};

} // namespace glass_ui::widget
