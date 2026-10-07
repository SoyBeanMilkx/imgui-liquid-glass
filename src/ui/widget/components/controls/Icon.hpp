#pragma once

#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

enum class IconGlyph {
  Grid,
  Search,
  Bell,
  Chat,
  Settings,
  Sparkles,
};

void DrawIcon(const Context &context, IconGlyph glyph, ImVec2 center,
              float size, ImVec4 color, float thickness = 1.5f);

}
