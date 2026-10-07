#pragma once

#include "ui/widget/foundation/layout/BoxLayout.hpp"
#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

struct SwipePagerOptions {
  ImVec2 size{};
  ImVec2 contentSize{};
  BoxLayoutOptions contentLayout{};
  float touchSlop = 10.0f;
  float directionRatio = 1.2f;
  float distanceThreshold = 0.15f;
  float minimumDistance = 48.0f;
  float flingVelocity = 700.0f;
  bool wrap = false;
  ImGuiChildFlags childFlags = ImGuiChildFlags_None;
  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
};

struct SwipePagerResult {
  bool visible = false;
  bool selectionChanged = false;
  bool dragging = false;
  float dragProgress = 0.0f;
};

SwipePagerResult BeginSwipePager(Context &context, const char *id,
                                 int pageCount, int *selectedIndex,
                                 const SwipePagerOptions &options = {});
void EndSwipePager(Context &context);

}
