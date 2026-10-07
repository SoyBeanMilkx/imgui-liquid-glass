#pragma once

#include "imgui.h"

namespace glass_ui::widget {

class Context;

struct ItemBehaviorState {
  ImGuiID id = 0;
  ImVec2 minimum{};
  ImVec2 maximum{};
  bool pressed = false;
  bool hovered = false;
  bool active = false;
};

ItemBehaviorState buttonBehavior(Context &context, const char *id, ImVec2 size,
                                 bool enabled);

}
