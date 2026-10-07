#pragma once

#include "ui/widget/components/containers/GlassWindow.hpp"

#include <string>

namespace glass_ui::widget {

enum class NavigationSide {
  Left,
  Right,
};

struct GlassGroupTransform {
  ImVec2 scale = ImVec2(1.0f, 1.0f);
  ImVec2 offset{};
};

struct GlassNavigationLayoutOptions {
  ImVec2 position{};
  ImVec2 size{};
  NavigationSide side = NavigationSide::Left;
  float railWidth = 54.0f;
  float gap = 10.0f;
  float railVisibility = 1.0f;
  GlassGroupTransform transform{};
  GlassWindowOptions content;
  GlassWindowOptions rail;
};

struct GlassNavigationLayoutBounds {
  ImVec2 contentMinimum{};
  ImVec2 contentMaximum{};
  ImVec2 railMinimum{};
  ImVec2 railMaximum{};
  bool railVisible = false;
  LayoutRect groupBounds() const noexcept;
};

struct GlassNavigationLayoutGeometry {
  GlassNavigationLayoutBounds layout;
  GlassNavigationLayoutBounds visual;
  DrawTransform transform;
  InputSpace inputSpace(ImGuiID id) const noexcept {
    return InputSpace{id, layout.groupBounds(), transform};
  }
};

class GlassNavigationLayout final {
public:
  GlassNavigationLayout(Context &context, const char *name,
                        const GlassNavigationLayoutOptions &options);

  static GlassNavigationLayoutGeometry resolveGeometry(
      const GlassNavigationLayoutOptions &options, float density) noexcept;

  bool beginRail();
  void endRail();
  bool beginContent(bool *open = nullptr);
  void endContent();

  const GlassNavigationLayoutBounds &bounds() const noexcept {
    return geometry_.visual;
  }
  const GlassNavigationLayoutBounds &layoutBounds() const noexcept {
    return geometry_.layout;
  }
  LayoutRect mapBounds(const LayoutRect &bounds) const noexcept;

private:
  Context &context_;
  GlassNavigationLayoutOptions options_;
  GlassNavigationLayoutGeometry geometry_;
  std::string contentName_;
  std::string railName_;
  float railVisibility_ = 0.0f;
  bool railBegun_ = false;
  bool contentBegun_ = false;
  void recordWindowBounds(bool rail) noexcept;
};

}
