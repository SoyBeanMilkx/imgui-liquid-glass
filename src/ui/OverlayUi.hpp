#pragma once

#include "OverlayPanel.hpp"
#include "UiModels.hpp"
#include "ui/input/InputRegion.hpp"
#include "ui/page/HomePage.hpp"
#include "ui/page/MessagesPage.hpp"
#include "ui/page/NotificationsPage.hpp"
#include "ui/page/SearchPage.hpp"
#include "ui/page/SettingsPage.hpp"
#include "ui/widget/foundation/animation/FadeThroughTransition.hpp"
#include "ui/widget/foundation/animation/FluidMotion.hpp"
#include "ui/widget/foundation/animation/InteractiveScaleMotion.hpp"
#include "ui/widget/components/containers/navigation/GlassNavigationLayout.hpp"
#include "widget/runtime/Context.hpp"

namespace glass_ui {

class OverlayUi final {
public:
  void applyStyle();
  void prepareFrame(const OverlayUiFrame &frame);
  void draw();
  void prepareDrawData(ImDrawData &drawData) {
    widgetContext_.prepareDrawData(drawData);
  }
  ImVec2 textInputPosition(ImVec2 position) const noexcept {
    return widgetContext_.textInputPosition(position);
  }
  void resetLayout() noexcept;
  const InputRegionSet &inputRegions() const noexcept { return inputRegions_; }
  widget::TextInputSession &textInput() noexcept {
    return widgetContext_.textInput();
  }
  void setEffectCapabilities(widget::EffectCapabilities capabilities) noexcept {
    widgetContext_.setEffectCapabilities(capabilities);
  }
  const widget::GlassRequestQueue &glassRequests() const noexcept {
    return widgetContext_.glassRequests();
  }

private:
  void configureLayout(const OverlayUiFrame &frame);
  float updatePanelTransition(const widget::FrameInfo &frame);
  widget::FadeThroughResult updatePageTransition();
  widget::FluidMotionResult updateFluidMotion(const widget::FrameInfo &frame);
  float updateFloatingButtonScale(const widget::FrameInfo &frame);
  void prepareMainPanel(float floatingAmount,
                        const widget::FluidMotionResult &fluidMotion,
                        float surfaceScale);
  void drawMainPanel();
  void drawNavigationRail();
  void drawFloatingIcon(float floatingAmount);
  void drawResizeHandles(
      const widget::GlassNavigationLayoutBounds &bounds);
  void drawExpandedContent(
      int pageIndex, float opacity, bool interactive,
      const widget::GlassNavigationLayout &layout);
  void drawSelectedPage(int pageIndex);

  float layoutWidth_ = 0.0f;
  float layoutHeight_ = 0.0f;
  float appliedDensity_ = 1.0f;
  OverlayPanel panel_;
  InputRegionSet inputRegions_;
  widget::Context widgetContext_;
  widget::GlassNavigationLayoutOptions mainPanelOptions_;
  float floatingAmount_ = 0.0f;
  ImFont *regularFont_ = nullptr;
  ImFont *semiboldFont_ = nullptr;
  uint64_t frameNumber_ = 0;
  int selectedNavigation_ = 0;
  widget::FadeThroughTransition pageTransition_;
  widget::FluidMotion fluidMotion_;
  widget::InteractiveScaleMotion floatingButtonMotion_;
  page::PageState pageState_;
  page::HomePage homePage_;
  page::SearchPage searchPage_;
  page::NotificationsPage notificationsPage_;
  page::MessagesPage messagesPage_;
  page::SettingsPage settingsPage_;
};

}
