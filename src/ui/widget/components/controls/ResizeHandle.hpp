#pragma once

#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

enum class ResizeHandleCorner {
  TopRight,
  BottomRight,
};

struct ResizeHandleOptions {
  float radius = 18.0f;
  float touchSize = 44.0f;
  float thickness = 2.5f;
  ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  bool enabled = false;
  bool active = false;
};

struct ResizeHandleResult {
  ImVec2 minimum{};
  ImVec2 maximum{};
  bool hovered = false;
  bool pressed = false;
};

ResizeHandleResult ResizeHandle(Context &context, const char *id,
                                ImVec2 corner,
                                ResizeHandleCorner cornerKind,
                                const ResizeHandleOptions &options = {});

}
