#include "LayoutViewport.hpp"
#include "ui/widget/runtime/Context.hpp"

#include "imgui.h"

#include <algorithm>

namespace glass_ui::widget {

bool BeginLayoutViewport(Context &context, const char *id,
                         const LayoutRect &bounds,
                         const LayoutViewportOptions &options) {
  const ImVec2 size(std::max(bounds.width(), 1.0f),
                    std::max(bounds.height(), 1.0f));
  ImGui::SetCursorScreenPos(bounds.minimum);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool visible = ImGui::BeginChild(
      id ? id : "##layout-viewport", size, options.childFlags,
      options.windowFlags);
  ImGui::PopStyleVar();
  context.drawTransforms().attach(ImGui::GetWindowDrawList());
  return visible;
}

void EndLayoutViewport() { ImGui::EndChild(); }

}
