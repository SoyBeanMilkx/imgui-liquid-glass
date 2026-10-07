#include "GlassNavigationLayout.hpp"

#include <algorithm>

namespace glass_ui::widget {

LayoutRect GlassNavigationLayoutBounds::groupBounds() const noexcept {
  LayoutRect bounds{contentMinimum, contentMaximum};
  if (railVisible) {
    bounds.minimum.x = std::min(bounds.minimum.x, railMinimum.x);
    bounds.minimum.y = std::min(bounds.minimum.y, railMinimum.y);
    bounds.maximum.x = std::max(bounds.maximum.x, railMaximum.x);
    bounds.maximum.y = std::max(bounds.maximum.y, railMaximum.y);
  }
  return bounds;
}

GlassNavigationLayout::GlassNavigationLayout(
    Context &context, const char *name,
    const GlassNavigationLayoutOptions &options)
    : context_(context), options_(options),
      geometry_(resolveGeometry(options, context.frame().density)),
      contentName_(name ? name : "##glass-navigation-content"),
      railName_(contentName_ + "/rail") {
  railVisibility_ = std::clamp(options_.railVisibility, 0.0f, 1.0f);
}

GlassNavigationLayoutGeometry GlassNavigationLayout::resolveGeometry(
    const GlassNavigationLayoutOptions &options, float density) noexcept {
  GlassNavigationLayoutGeometry geometry;
  GlassNavigationLayoutBounds &bounds = geometry.layout;
  const float width = std::max(options.railWidth * density, 1.0f);
  const float gap = std::max(options.gap * density, 0.0f);
  const ImVec2 contentSize(std::max(options.size.x, 1.0f),
                           std::max(options.size.y, 1.0f));
  const float visibility = std::clamp(options.railVisibility, 0.0f, 1.0f);
  bounds.contentMinimum = options.position;
  bounds.contentMaximum = ImVec2(options.position.x + contentSize.x,
                                  options.position.y + contentSize.y);
  const float targetX = options.side == NavigationSide::Left
                            ? options.position.x - gap - width
                            : options.position.x + contentSize.x + gap;
  const float hiddenX = options.side == NavigationSide::Left
                            ? options.position.x
                            : options.position.x + contentSize.x - width;
  const float railX = hiddenX + (targetX - hiddenX) * visibility;
  bounds.railMinimum = ImVec2(railX, options.position.y);
  bounds.railMaximum = ImVec2(railX + width,
                              options.position.y + contentSize.y);
  bounds.railVisible = visibility > 0.001f;
  const LayoutRect group = bounds.groupBounds();
  geometry.transform = DrawTransform::around(
      ImVec2((group.minimum.x + group.maximum.x) * 0.5f,
              (group.minimum.y + group.maximum.y) * 0.5f),
      ImVec2(std::clamp(options.transform.scale.x, 0.60f, 1.40f),
              std::clamp(options.transform.scale.y, 0.60f, 1.40f)),
      options.transform.offset);
  geometry.visual = bounds;
  const LayoutRect content = geometry.transform.mapBounds(
      LayoutRect{bounds.contentMinimum, bounds.contentMaximum});
  const LayoutRect rail = geometry.transform.mapBounds(
      LayoutRect{bounds.railMinimum, bounds.railMaximum});
  geometry.visual.contentMinimum = content.minimum;
  geometry.visual.contentMaximum = content.maximum;
  geometry.visual.railMinimum = rail.minimum;
  geometry.visual.railMaximum = rail.maximum;
  return geometry;
}

LayoutRect GlassNavigationLayout::mapBounds(
    const LayoutRect &bounds) const noexcept {
  return geometry_.transform.mapBounds(bounds);
}

void GlassNavigationLayout::recordWindowBounds(bool rail) noexcept {
  const ImVec2 minimum = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const LayoutRect layout{minimum,
                          ImVec2(minimum.x + size.x, minimum.y + size.y)};
  const LayoutRect visual = mapBounds(layout);
  if (rail) {
    geometry_.layout.railMinimum = layout.minimum;
    geometry_.layout.railMaximum = layout.maximum;
    geometry_.visual.railMinimum = visual.minimum;
    geometry_.visual.railMaximum = visual.maximum;
  } else {
    geometry_.layout.contentMinimum = layout.minimum;
    geometry_.layout.contentMaximum = layout.maximum;
    geometry_.visual.contentMinimum = visual.minimum;
    geometry_.visual.contentMaximum = visual.maximum;
  }
}

bool GlassNavigationLayout::beginRail() {
  const GlassNavigationLayoutBounds &bounds = geometry_.layout;
  if (!bounds.railVisible || railBegun_)
    return false;
  ImGui::SetNextWindowPos(bounds.railMinimum, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      ImVec2(bounds.railMaximum.x - bounds.railMinimum.x,
             bounds.railMaximum.y - bounds.railMinimum.y),
      ImGuiCond_Always);
  ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
  GlassWindowOptions rail = options_.rail;
  rail.opacity *= railVisibility_;
  rail.flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, railVisibility_);
  railBegun_ = true;
  context_.drawTransforms().push(geometry_.transform);
  const bool visible =
      BeginGlassWindow(context_, railName_.c_str(), nullptr, rail);
  recordWindowBounds(true);
  return visible;
}

void GlassNavigationLayout::endRail() {
  if (!railBegun_)
    return;
  EndGlassWindow(context_);
  context_.drawTransforms().pop();
  ImGui::PopStyleVar();
  railBegun_ = false;
}

bool GlassNavigationLayout::beginContent(bool *open) {
  if (contentBegun_)
    return false;
  const GlassNavigationLayoutBounds &bounds = geometry_.layout;
  ImGui::SetNextWindowPos(bounds.contentMinimum, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      ImVec2(bounds.contentMaximum.x - bounds.contentMinimum.x,
             bounds.contentMaximum.y - bounds.contentMinimum.y),
      ImGuiCond_Always);
  ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
  GlassWindowOptions content = options_.content;
  content.flags |= ImGuiWindowFlags_NoSavedSettings;
  contentBegun_ = true;
  context_.drawTransforms().push(geometry_.transform);
  const bool visible =
      BeginGlassWindow(context_, contentName_.c_str(), open, content);
  recordWindowBounds(false);
  return visible;
}

void GlassNavigationLayout::endContent() {
  if (!contentBegun_)
    return;
  EndGlassWindow(context_);
  context_.drawTransforms().pop();
  contentBegun_ = false;
}

}
