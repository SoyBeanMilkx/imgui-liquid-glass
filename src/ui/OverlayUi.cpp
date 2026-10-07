#include "OverlayUi.hpp"

#include "imgui.h"
#include "ui/widget/Widget.hpp"

#include <algorithm>
#include <cmath>

namespace glass_ui {
namespace {

// Scale from the target's 471 dp short edge to survive host resolution changes.
constexpr float kReferenceShortEdge = 471.0f;
constexpr float kFloatingButtonDiameter = 58.0f;
constexpr float kWindowContentPadding = 12.0f;
constexpr float kWindowDragSlop = 8.0f;
constexpr float kNavigationRailWidth = 54.0f;
constexpr float kNavigationGap = 10.0f;
constexpr float kMinimumWindowWidth = 280.0f;
constexpr float kMinimumWindowHeight = 220.0f;
constexpr widget::NavigationSide kNavigationSide =
    widget::NavigationSide::Left;
constexpr ImGuiID kMainWindowAnimationId = 0x46574D31u;
constexpr uint32_t kWindowMorphChannel = 1u;
constexpr uint32_t kPageFadeChannel = 2u;
constexpr float kPageFadeDuration = 0.22f;
constexpr widget::SpringOptions kWindowMorphSpring{0.34f, 0.72f, 0.001f};

float smoothstep(float value) noexcept {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}

}

void OverlayUi::applyStyle() {
  ImGui::StyleColorsDark();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  io.MouseDoubleClickTime = 0.5f;
  io.MouseDoubleClickMaxDist = 40.0f;
  if (io.Fonts && io.Fonts->Fonts.empty()) {
    const widget::BundledFontFamily fonts = widget::AddBundledDefaultFonts();
    regularFont_ = fonts.regular;
    semiboldFont_ = fonts.semibold ? fonts.semibold : fonts.regular;
  }
  if (regularFont_) {
    io.FontDefault = regularFont_;
    widget::Theme &theme = widgetContext_.theme();
    theme.textFont = regularFont_;
    theme.buttonFont = semiboldFont_;
    theme.tabFont = regularFont_;
  } else if (io.Fonts && io.Fonts->Fonts.empty()) {
    io.FontDefault = io.Fonts->AddFontDefaultVector();
  }
  ImGuiStyle &style = ImGui::GetStyle();
  style.FontScaleDpi = 1.0f;
  appliedDensity_ = 1.0f;
}

void OverlayUi::prepareFrame(const OverlayUiFrame &frame) {
  const float shortEdge = std::min(frame.width, frame.height);
  const float density =
      std::clamp(shortEdge / kReferenceShortEdge, 0.75f, 4.0f);
  if (std::abs(density - appliedDensity_) > 0.001f) {
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(density / appliedDensity_);
    style.FontScaleDpi = density;
    appliedDensity_ = density;
  }
  configureLayout(frame);
  inputRegions_.clear();
  ImGuiIO &io = ImGui::GetIO();
  widget::FrameInfo widgetFrame{ImVec2(frame.width, frame.height),
                                 std::clamp(io.DeltaTime, 0.001f, 0.05f),
                                 appliedDensity_, ++frameNumber_, false,
                                 frame.pointerEvents};

  const float navigationExtent =
      (kNavigationRailWidth + kNavigationGap) * appliedDensity_;
  panel_.setExpandedMargins(
      kNavigationSide == widget::NavigationSide::Left
          ? ImVec4(navigationExtent, 0.0f, 0.0f, 0.0f)
          : ImVec4(0.0f, 0.0f, navigationExtent, 0.0f));
  panel_.setMinimumExpandedSize(
      ImVec2(kMinimumWindowWidth * appliedDensity_,
             kMinimumWindowHeight * appliedDensity_));
  panel_.beginFrame(widgetFrame.displaySize, frame.pointerEvents,
                    widgetFrame.deltaTime);
  floatingAmount_ = updatePanelTransition(widgetFrame);
  const widget::FluidMotionResult fluidMotion = updateFluidMotion(widgetFrame);
  const float floatingButtonScale = updateFloatingButtonScale(widgetFrame);
  prepareMainPanel(floatingAmount_, fluidMotion, floatingButtonScale);

  const widget::InputSpace inputSpace =
      widget::GlassNavigationLayout::resolveGeometry(mainPanelOptions_,
                                                      appliedDensity_)
          .inputSpace(kMainWindowAnimationId);
  widget::GlassBackdropStyle backdrop;
  backdrop.blurRadius = pageState_.glassBlurRadius;
  backdrop.quality = widget::GlassQuality::Auto;
  widgetContext_.beginFrame(widgetFrame, backdrop, {&inputSpace, 1});
}

void OverlayUi::draw() {
  drawMainPanel();
  widgetContext_.endFrame();
}

float OverlayUi::updatePanelTransition(const widget::FrameInfo &frame) {
  const float floatingTarget = panel_.isFloating() ? 1.0f : 0.0f;
  const widget::SpringTransition transition =
      widgetContext_.animations().spring(
          kMainWindowAnimationId, kWindowMorphChannel, floatingTarget,
          kWindowMorphSpring, frame.deltaTime, frame.frameNumber,
          frame.reduceMotion);
  panel_.setTransition(transition.value,
                       kFloatingButtonDiameter * appliedDensity_,
                       transition.active);
  return std::clamp(transition.value, 0.0f, 1.0f);
}

widget::FadeThroughResult OverlayUi::updatePageTransition() {
  const widget::FrameInfo &frame = widgetContext_.frame();
  return pageTransition_.update(
      widgetContext_.animations(), kMainWindowAnimationId, kPageFadeChannel,
      selectedNavigation_, kPageFadeDuration, frame.deltaTime,
      frame.frameNumber, frame.reduceMotion);
}

widget::FluidMotionResult OverlayUi::updateFluidMotion(
    const widget::FrameInfo &frame) {
  if (panel_.isFloating()) {
    fluidMotion_.reset();
    return {};
  }
  widget::FluidMotionOptions options;
  options.responseSeconds = 0.20f;
  options.dampingRatio = 0.52f;
  options.velocityForFullEffect = 850.0f;
  options.maximumStretch = 0.11f;
  options.maximumCompression = 0.035f;
  options.maximumOffset = 3.0f;
  return fluidMotion_.update(panel_.motionVelocity(), options, frame.density,
                             frame.deltaTime, frame.reduceMotion);
}

float OverlayUi::updateFloatingButtonScale(const widget::FrameInfo &frame) {
  widget::InteractiveScaleOptions options;
  options.pressedScale = 1.10f;
  options.entranceScale = 1.0f;
  const bool ballPresented =
      panel_.isFloating() && !panel_.transitionActive();
  const widget::InteractiveScaleResult motion = floatingButtonMotion_.update(
      ballPresented, panel_.floatingPressed(), options, frame.deltaTime,
      frame.reduceMotion);
  return motion.scale;
}

void OverlayUi::prepareMainPanel(
    float floatingAmount, const widget::FluidMotionResult &fluidMotion,
    float surfaceScale) {
  constexpr ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  widget::GlassWindowOptions glass;
  glass.flags = flags;
  glass.contentPadding = ImVec2(kWindowContentPadding, kWindowContentPadding);
  glass.radii = widget::CornerRadii::all(
      24.0f + (kFloatingButtonDiameter * 0.5f - 24.0f) * floatingAmount);
  glass.style.refractionAmount =
      -120.0f * (1.0f + 0.18f * fluidMotion.intensity);
  glass.style.depthEffect = 0.75f + 0.18f * fluidMotion.intensity;
  glass.style.chromaticAberration =
      0.5f + 0.16f * fluidMotion.intensity;
  glass.style.contrast = -0.10f;
  glass.style.tint =
      ImVec4(0.04f, 0.08f, 0.07f, 0.12f + 0.06f * floatingAmount);

  widget::GlassNavigationLayoutOptions navigation;
  // Keep the logical canvas on screen and independent of physical dragging.
  const float navigationExtent =
      (kNavigationRailWidth + kNavigationGap) * appliedDensity_;
  navigation.position = ImVec2(
      kNavigationSide == widget::NavigationSide::Left
          ? navigationExtent + 1.0f : 1.0f,
      1.0f);
  navigation.size = panel_.size();
  navigation.side = kNavigationSide;
  navigation.railWidth = kNavigationRailWidth;
  navigation.gap = kNavigationGap;
  navigation.railVisibility = 1.0f - floatingAmount;
  navigation.transform.scale =
      ImVec2(fluidMotion.scale.x * surfaceScale,
             fluidMotion.scale.y * surfaceScale);
  const ImVec2 physicalPosition = panel_.position();
  navigation.transform.offset = ImVec2(
      physicalPosition.x - navigation.position.x + fluidMotion.offset.x,
      physicalPosition.y - navigation.position.y + fluidMotion.offset.y);
  navigation.content = glass;
  navigation.rail = glass;
  navigation.rail.contentPadding = ImVec2(6.0f, 12.0f);
  navigation.rail.radii = widget::CornerRadii::all(24.0f);
  navigation.rail.flags |=
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  navigation.rail.style.tint = ImVec4(0.04f, 0.08f, 0.07f, 0.16f);
  mainPanelOptions_ = navigation;
}

void OverlayUi::drawMainPanel() {
  const float floatingAmount = floatingAmount_;
  widget::GlassNavigationLayout layout(widgetContext_, "##glass_ui-overlay",
                                       mainPanelOptions_);
  const bool railVisible = layout.beginRail();
  if (railVisible)
    drawNavigationRail();
  layout.endRail();

  const bool windowVisible = layout.beginContent();
  if (windowVisible) {
    const widget::GlassNavigationLayoutBounds &bounds = layout.bounds();
    InputRegion contentRegion;
    contentRegion.minimum = bounds.contentMinimum;
    contentRegion.maximum = bounds.contentMaximum;
    contentRegion.shape = panel_.isFloating() && floatingAmount >= 0.999f
                              ? InputRegionShape::Ellipse
                              : InputRegionShape::Rectangle;
    contentRegion.visible = bounds.contentMaximum.x > bounds.contentMinimum.x &&
                             bounds.contentMaximum.y > bounds.contentMinimum.y;
    inputRegions_.add(contentRegion);
  }
  if (railVisible) {
    const widget::GlassNavigationLayoutBounds &bounds = layout.bounds();
    inputRegions_.add(InputRegion{bounds.railMinimum, bounds.railMaximum,
                                  InputRegionShape::Rectangle,
                                  bounds.railVisible});
  }
  const float reveal = smoothstep((0.82f - floatingAmount) / 0.62f);
  const widget::FadeThroughResult page = updatePageTransition();
  const float contentOpacity = reveal * page.opacity;
  const bool contentInteractive = !panel_.isFloating() &&
                                  !panel_.transitionActive() &&
                                  floatingAmount <= 0.001f &&
                                  !page.active;
  if (windowVisible && contentOpacity > 0.001f) {
    const ImVec2 expandedPosition = panel_.expandedPosition();
    const ImVec2 expandedSize = panel_.expandedSize();
    const ImVec2 physicalPosition = panel_.position();
    const ImVec2 layoutPosition(
        mainPanelOptions_.position.x + (expandedPosition.x - physicalPosition.x),
        mainPanelOptions_.position.y + (expandedPosition.y - physicalPosition.y));
    const widget::LayoutRect expandedBounds{
        layoutPosition,
        ImVec2(layoutPosition.x + expandedSize.x,
               layoutPosition.y + expandedSize.y)};
    const widget::LayoutRect viewport =
        expandedBounds.inset(widget::LayoutInsets::all(kWindowContentPadding),
                              appliedDensity_);
    const bool viewportVisible =
        widget::BeginLayoutViewport(widgetContext_, "##main-content-viewport",
                                    viewport);
    if (viewportVisible)
      drawExpandedContent(page.visibleValue, contentOpacity,
                          contentInteractive, layout);
    widget::EndLayoutViewport();
  }
  if (windowVisible && floatingAmount > 0.001f)
    drawFloatingIcon(floatingAmount);
  if (windowVisible && !panel_.isFloating() &&
      !panel_.transitionActive() && floatingAmount <= 0.001f)
    drawResizeHandles(layout.layoutBounds());

  if (panel_.isFloating() && !panel_.transitionActive() &&
      floatingAmount >= 0.999f) {
    const widget::GlassNavigationLayoutBounds &bounds = layout.bounds();
    panel_.handleFloatingInput(bounds.contentMinimum, bounds.contentMaximum,
                               kWindowDragSlop * appliedDensity_);
  }
  layout.endContent();
}

void OverlayUi::drawFloatingIcon(float floatingAmount) {
  const float progress =
      std::clamp((floatingAmount - 0.28f) / 0.72f, 0.0f, 1.0f);
  const float visibility = progress * progress * (3.0f - 2.0f * progress);
  const ImVec2 position = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 center(position.x + size.x * 0.5f,
                      position.y + size.y * 0.5f);
  widget::DrawIcon(widgetContext_, widget::IconGlyph::Grid, center,
                   22.0f * (0.88f + 0.12f * visibility),
                   ImVec4(1.0f, 1.0f, 1.0f, visibility), 1.65f);
}

void OverlayUi::drawNavigationRail() {
  constexpr widget::NavigationRailItem items[] = {
      {"##home", widget::NavigationIcon::Grid, true},
      {"##search", widget::NavigationIcon::Search, true},
      {"##notifications", widget::NavigationIcon::Bell, true},
      {"##messages", widget::NavigationIcon::Chat, true},
      {"##settings", widget::NavigationIcon::Settings, true},
  };
  const widget::NavigationRailResult result = widget::NavigationRail(
      widgetContext_, "##main-navigation", items,
      sizeof(items) / sizeof(items[0]), &selectedNavigation_);
  if (result.pressed) {
    if (result.pressedIndex == 4)
      panel_.setResizeMode(!panel_.resizeMode());
    else
      panel_.setResizeMode(false);
  }
}

void OverlayUi::drawResizeHandles(
    const widget::GlassNavigationLayoutBounds &bounds) {
  widget::ResizeHandleOptions options;
  options.radius = 24.0f;
  options.enabled = panel_.resizeMode();
  options.active = panel_.isResizing(PanelResizeCorner::TopRight);
  const ImVec2 topRight(bounds.contentMaximum.x, bounds.contentMinimum.y);
  const widget::ResizeHandleResult top = widget::ResizeHandle(
      widgetContext_, "##resize-top-right", topRight,
      widget::ResizeHandleCorner::TopRight, options);
  panel_.handleResizeInput(PanelResizeCorner::TopRight, top.pressed);

  options.active = panel_.isResizing(PanelResizeCorner::BottomRight);
  const widget::ResizeHandleResult bottom = widget::ResizeHandle(
      widgetContext_, "##resize-bottom-right", bounds.contentMaximum,
      widget::ResizeHandleCorner::BottomRight, options);
  panel_.handleResizeInput(PanelResizeCorner::BottomRight, bottom.pressed);
}

void OverlayUi::drawExpandedContent(
    int pageIndex, float opacity, bool interactive,
    const widget::GlassNavigationLayout &layout) {
  constexpr const char *titles[] = {"Home", "Search", "Notifications",
                                    "Messages", "Settings"};
  const int page = std::clamp(pageIndex, 0, 4);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                      ImGui::GetStyle().Alpha *
                          std::clamp(opacity, 0.0f, 1.0f));
  if (!interactive) {
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
    ImGui::BeginDisabled();
  }
  widget::TextOptions heading;
  heading.font = semiboldFont_;
  heading.fontSize = 20.0f;
  heading.color = ImVec4(0.98f, 0.99f, 1.0f, 1.0f);
  widget::Text(widgetContext_, titles[page], heading);

  ImGui::Dummy(ImVec2(0.0f, 7.0f * widgetContext_.frame().density));
  if (interactive) {
    const widget::GlassNavigationLayoutBounds &bounds = layout.layoutBounds();
    const widget::LayoutRect titleBar = layout.mapBounds(widget::LayoutRect{
        bounds.contentMinimum,
        ImVec2(bounds.contentMaximum.x, ImGui::GetCursorScreenPos().y)});
    panel_.handleTitleBarInput(
        titleBar.minimum, titleBar.maximum,
        kWindowDragSlop * appliedDensity_);
  }
  drawSelectedPage(page);
  if (!interactive) {
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
  }
  ImGui::PopStyleVar();
}

void OverlayUi::drawSelectedPage(int pageIndex) {
  page::PageContext context{widgetContext_, regularFont_, semiboldFont_,
                            pageState_};
  switch (pageIndex) {
  case 0:
    homePage_.draw(context);
    break;
  case 1:
    searchPage_.draw(context);
    break;
  case 2:
    notificationsPage_.draw(context);
    break;
  case 3:
    messagesPage_.draw(context);
    break;
  case 4: {
    const page::SettingsPageResult result =
        settingsPage_.draw(context, panel_.resizeMode());
    if (result.resizeModeChanged)
      panel_.setResizeMode(result.resizeMode);
    break;
  }
  default:
    break;
  }
}

void OverlayUi::resetLayout() noexcept {
  inputRegions_.clear();
  layoutWidth_ = 0.0f;
  layoutHeight_ = 0.0f;
  panel_.reset();
  widgetContext_.reset();
  pageTransition_.reset(selectedNavigation_);
  fluidMotion_.reset();
  floatingButtonMotion_.reset();
  mainPanelOptions_ = {};
  floatingAmount_ = 0.0f;
}

void OverlayUi::configureLayout(const OverlayUiFrame &frame) {
  if (layoutWidth_ == frame.width && layoutHeight_ == frame.height)
    return;

  const bool landscape = frame.width > frame.height;
  const ImVec2 size(landscape ? frame.width * 0.34f : frame.width * 0.76f,
                    landscape ? frame.height * 0.64f : frame.height * 0.38f);
  const float navigationExtent =
      (kNavigationRailWidth + kNavigationGap) * appliedDensity_;
  const float navigationOffset =
      kNavigationSide == widget::NavigationSide::Left
          ? navigationExtent * 0.5f
          : -navigationExtent * 0.5f;
  panel_.configure(
      ImVec2((frame.width - size.x) * 0.5f + navigationOffset,
             (frame.height - size.y) * 0.34f),
      size);
  layoutWidth_ = frame.width;
  layoutHeight_ = frame.height;
}

}
