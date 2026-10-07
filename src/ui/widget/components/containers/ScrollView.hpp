#pragma once

#include "ui/widget/runtime/Context.hpp"

namespace glass_ui::widget {

struct ScrollViewOptions {
  ImVec2 size{};
  float touchSlop = 8.0f;
  float directionRatio = 1.15f;
  float overscrollResistance = 0.36f;
  float deceleration = 7.5f;
  float minimumVelocity = 8.0f;
  float indicatorWidth = 2.5f;
  float indicatorMinimumLength = 24.0f;
  float indicatorInset = 3.0f;
  ImVec4 indicatorColor = ImVec4(1.0f, 1.0f, 1.0f, 0.58f);
  ImGuiChildFlags childFlags = ImGuiChildFlags_None;
  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
};

struct ScrollViewResult {
  bool visible = false;
  bool dragging = false;
  float offset = 0.0f;
  float maximum = 0.0f;
};

ScrollViewResult BeginScrollView(Context &context, const char *id,
                                 const ScrollViewOptions &options = {});
void EndScrollView(Context &context);

}
