#pragma once

#include "ui/widget/foundation/layout/LayoutTypes.hpp"

namespace glass_ui::widget {

class Context;

struct LayoutViewportOptions {
  ImGuiChildFlags childFlags = ImGuiChildFlags_None;
  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;
};

bool BeginLayoutViewport(Context &context, const char *id,
                         const LayoutRect &bounds,
                         const LayoutViewportOptions &options = {});
void EndLayoutViewport();

}
