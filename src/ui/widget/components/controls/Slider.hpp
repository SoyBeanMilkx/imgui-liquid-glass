#pragma once

#include "ui/widget/runtime/Context.hpp"

#include <optional>

namespace glass_ui::widget {

struct SliderOptions {
  ImVec2 size{};
  ImVec2 thumbSize{};
  float step = 0.0f;
  float minimumTouchSize = -1.0f;
  float trackHeight = -1.0f;
  float activeScale = -1.0f;
  float touchSlop = 10.0f;
  float directionRatio = 1.2f;
  std::optional<ImVec4> trackColor;
  std::optional<ImVec4> fillColor;
  std::optional<ImVec4> thumbColor;
  GlassStyle glassStyle =
      GlassStyle{11.0f,
                 -26.0f,
                 0.36f,
                 0.22f,
                 0.0f,
                 0.0f,
                 1.0f,
                 ImVec4(1.0f, 1.0f, 1.0f, 0.08f),
                 GlassLighting{0.18f, 0.32f, 0.60f, 0.18f, 6.0f, 18.0f,
                               ImVec2(-0.7071f, -0.7071f)}};
  float glassBlurMix = 1.0f;
  bool glassOnInteraction = true;
  bool enabled = true;
};

bool Slider(Context &context, const char *id, float *value, float minimum,
            float maximum, const SliderOptions &options = {});

} // namespace glass_ui::widget
