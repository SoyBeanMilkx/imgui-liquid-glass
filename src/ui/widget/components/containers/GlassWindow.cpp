#include "GlassWindow.hpp"

#include <algorithm>

namespace glass_ui::widget {
namespace {

constexpr float kRoundedCornerSafeInset = 0.2928932188f;

float effectiveRadius(const GlassWindowOptions &options, const ImVec2 &size,
                      float density = 1.0f) noexcept {
  const float limit = std::max(0.0f, std::min(size.x, size.y) * 0.5f);
  if (options.shape == GlassShapeKind::Circle ||
      options.shape == GlassShapeKind::Capsule)
    return limit;
  return std::clamp(options.radii.topLeft * density, 0.0f, limit);
}

bool canUseBackdropEffect(const Context &context) noexcept {
  const EffectCapabilities &capabilities = context.effectCapabilities();
  return capabilities.backdropCapture &&
         context.glassRequests().requests().size() <
             capabilities.maxGlassRegions;
}

float maximumCornerRadius(const GlassWindowOptions &options) noexcept {
  return std::max(
      {options.radii.topLeft, options.radii.topRight,
       options.radii.bottomRight, options.radii.bottomLeft, 0.0f});
}

ImVec2 contentPadding(const Context &context,
                      const GlassWindowOptions &options) noexcept {
  const float density = context.frame().density;
  const ImVec2 defaultPadding = ImGui::GetStyle().WindowPadding;
  // Keep antialiased content clear of transparent rounded-corner pixels.
  const float safeInset =
      (maximumCornerRadius(options) * kRoundedCornerSafeInset + 2.0f) *
      density;
  return ImVec2(options.contentPadding.x >= 0.0f
                    ? options.contentPadding.x * density
                    : std::max(defaultPadding.x, safeInset),
                options.contentPadding.y >= 0.0f
                    ? options.contentPadding.y * density
                    : std::max(defaultPadding.y, safeInset));
}

// Keep empty window draw lists available for backend effect injection.
void keepDrawListAlive(const ImDrawList *, const ImDrawCmd *) {}

LayoutRect contentBounds(const Context &context,
                         const GlassWindowOptions &options) noexcept {
  const ImVec2 minimum = ImGui::GetCursorScreenPos();
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const LayoutRect bounds{
      minimum,
      ImVec2(minimum.x + std::max(available.x, 0.0f),
             minimum.y + std::max(available.y, 0.0f)),
  };
  return resolveBoxLayout(bounds, options.contentSize, context.frame().density,
                          options.contentLayout);
}

}
bool BeginGlassWindow(Context &context, const char *name, bool *open,
                      const GlassWindowOptions &options) {
  const bool useBackdropEffect = canUseBackdropEffect(context);
  const float opacity = std::clamp(options.opacity, 0.0f, 1.0f);
  const ImVec4 tint = options.style.tint;
  const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
  const ImVec4 fallbackFill(tint.x, tint.y, tint.z,
                            std::max(0.52f, tint.w) * opacity);
  const ImVec4 fallbackBorder(1.0f, 1.0f, 1.0f,
                              (52.0f / 255.0f) * opacity);
  const float rounding = effectiveRadius(options, ImVec2(10000.0f, 10000.0f),
                                         context.frame().density);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,
                      useBackdropEffect ? 0.0f : 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      contentPadding(context, options));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,
                      context.theme().scrollbarWidth *
                          context.frame().density);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,
                      context.theme().scrollbarRounding *
                          context.frame().density);
  const ImVec4 windowColor = useBackdropEffect ? transparent : fallbackFill;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, windowColor);
  ImGui::PushStyleColor(ImGuiCol_TitleBg, windowColor);
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, windowColor);
  ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, windowColor);
  ImGui::PushStyleColor(ImGuiCol_Border,
                        useBackdropEffect ? transparent : fallbackBorder);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, transparent);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
                        context.theme().scrollbarThumb);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,
                        context.theme().scrollbarThumbHovered);
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,
                        context.theme().scrollbarThumbActive);

  ImGuiWindowFlags flags = options.flags;
  if (useBackdropEffect)
    flags |= ImGuiWindowFlags_NoBackground;
  else
    flags &= ~ImGuiWindowFlags_NoBackground;
  const bool visible = ImGui::Begin(name, open, flags);
  context.drawTransforms().attach(ImGui::GetWindowDrawList());

  ImGui::PopStyleColor(9);
  ImGui::PopStyleVar(5);

  const ImVec2 position = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  if (size.x > 0.0f && size.y > 0.0f) {
    if (useBackdropEffect)
      ImGui::GetWindowDrawList()->AddCallback(keepDrawListAlive, nullptr);
    GlassRequest request;
    const GlassSurfaceEffect &effect = options.surfaceEffect;
    const DrawTransform surface = effect.enabled
        ? DrawTransform::around(
              effect.pivot,
              ImVec2(std::clamp(effect.scale.x, 0.60f, 1.40f),
                      std::clamp(effect.scale.y, 0.60f, 1.40f)),
              effect.offset)
        : DrawTransform{};
    const float radiusScale = std::min(surface.scale.x, surface.scale.y);
    request.id = ImGui::GetID(name);
    request.min = surface.mapPoint(position);
    request.max = surface.mapPoint(
        ImVec2(position.x + size.x, position.y + size.y));
    request.radii = CornerRadii{
        options.radii.topLeft * radiusScale, options.radii.topRight * radiusScale,
        options.radii.bottomRight * radiusScale,
        options.radii.bottomLeft * radiusScale};
    request.shape = options.shape;
    request.style = options.style;
    request.deformation = effect.enabled ? effect.deformation : ImVec2{};
    request.clipRect = ImVec4(0.0f, 0.0f, context.frame().displaySize.x,
                              context.frame().displaySize.y);
    request.drawList = ImGui::GetWindowDrawList();
    request.density = context.frame().density;
    request.opacity = opacity;
    context.glassRequests().submit(request);
  }

  const LayoutRect content = contentBounds(context, options);
  ImGui::SetCursorScreenPos(content.minimum);
  ImGui::PushClipRect(content.minimum, content.maximum, true);

  return visible;
}

void EndGlassWindow(Context &) {
  ImGui::PopClipRect();
  ImGui::End();
}

}
