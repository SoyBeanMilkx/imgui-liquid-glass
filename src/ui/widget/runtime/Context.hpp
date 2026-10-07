#pragma once

#include "FrameInfo.hpp"
#include "ui/widget/effects/EffectCapabilities.hpp"
#include "ui/widget/effects/glass/GlassRequestQueue.hpp"
#include "ui/widget/foundation/animation/AnimationStore.hpp"
#include "ui/widget/foundation/drawing/DrawTransform.hpp"
#include "ui/widget/foundation/interaction/GestureArena.hpp"
#include "ui/widget/foundation/interaction/InputSpace.hpp"
#include "ui/widget/foundation/interaction/ScrollStateStore.hpp"
#include "ui/widget/foundation/interaction/TextInputSession.hpp"
#include "ui/widget/foundation/style/Theme.hpp"

namespace glass_ui::widget {

class Context final {
public:
  void beginFrame(const FrameInfo &frame,
                 GlassBackdropStyle backdropStyle = {},
                 InputSpaceView inputSpaces = {});
  void endFrame();
  void reset() noexcept;

  const FrameInfo &frame() const noexcept { return frame_; }
  Theme &theme() noexcept { return theme_; }
  const Theme &theme() const noexcept { return theme_; }
  AnimationStore &animations() noexcept { return animations_; }
  GestureArena &gestures() noexcept { return gestures_; }
  ScrollStateStore &scrollStates() noexcept { return scrollStates_; }
  DrawTransformStore &drawTransforms() noexcept { return drawTransforms_; }
  ForegroundDraw foreground() { return drawTransforms_.foreground(); }
  void prepareDrawData(ImDrawData &drawData) { drawTransforms_.apply(drawData); }
  TextInputSession &textInput() noexcept { return textInput_; }
  void trackTextInput() noexcept;
  ImVec2 textInputPosition(ImVec2 position) const noexcept;
  GlassRequestQueue &glassRequests() noexcept { return glassRequests_; }
  const GlassRequestQueue &glassRequests() const noexcept {
    return glassRequests_;
  }

  void setEffectCapabilities(EffectCapabilities capabilities) noexcept {
    capabilities_ = capabilities;
  }
  const EffectCapabilities &effectCapabilities() const noexcept {
    return capabilities_;
  }

private:
  FrameInfo frame_{};
  Theme theme_{};
  AnimationStore animations_;
  GestureArena gestures_;
  ScrollStateStore scrollStates_;
  DrawTransformStore drawTransforms_;
  InputSpaceStore inputSpaces_;
  TextInputSession textInput_;
  ImDrawList *textInputDrawList_ = nullptr;
  GlassRequestQueue glassRequests_;
  EffectCapabilities capabilities_{};
  bool frameActive_ = false;
};

}
