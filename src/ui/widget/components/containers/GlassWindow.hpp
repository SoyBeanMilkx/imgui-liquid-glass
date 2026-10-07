#pragma once

#include "ui/widget/effects/glass/GlassStyle.hpp"
#include "ui/widget/foundation/layout/BoxLayout.hpp"
#include "ui/widget/runtime/Context.hpp"

#include "imgui.h"

namespace glass_ui::widget {

struct GlassWindowOptions {
  CornerRadii radii = CornerRadii::all(28.0f);
  GlassShapeKind shape = GlassShapeKind::RoundedRect;
  GlassStyle style{};
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  // Negative padding clears rounded corners; non-negative values are explicit.
  ImVec2 contentPadding = ImVec2(-1.0f, -1.0f);
  ImVec2 contentSize{};
  BoxLayoutOptions contentLayout{};
  GlassSurfaceEffect surfaceEffect{};
  float opacity = 1.0f;
};

bool BeginGlassWindow(Context &context, const char *name, bool *open,
                      const GlassWindowOptions &options = {});
void EndGlassWindow(Context &context);

}
