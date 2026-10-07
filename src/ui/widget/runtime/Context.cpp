#include "Context.hpp"

#include <algorithm>

namespace glass_ui::widget {

void Context::beginFrame(const FrameInfo &frame,
                        GlassBackdropStyle backdropStyle,
                        InputSpaceView inputSpaces) {
  frame_ = frame;
  frame_.deltaTime = std::clamp(frame_.deltaTime, 1.0f / 1000.0f, 0.05f);
  frame_.density = std::max(frame_.density, 0.25f);
  inputSpaces_.beginFrame(frame.pointerEvents, inputSpaces, ImGui::GetIO());
  frame_.pointerEvents = inputSpaces_.events();
  for (std::size_t index = 0; index < frame_.pointerEvents.count; ++index) {
    if (frame_.pointerEvents.events[index].phase == PointerPhase::Cancel) {
      gestures_.clear();
      break;
    }
  }
  gestures_.beginFrame(frame_.pointerEvents, frame_.frameNumber);
  scrollStates_.beginFrame();
  textInput_.beginFrame();
  textInputDrawList_ = nullptr;
  drawTransforms_.beginFrame();
  glassRequests_.beginFrame(backdropStyle, frame_.density);
  frameActive_ = true;
}

void Context::endFrame() {
  if (!frameActive_)
    return;
  glassRequests_.applyTransforms(drawTransforms_);
  glassRequests_.seal();
  animations_.endFrame(frame_.frameNumber);
  gestures_.endFrame();
  scrollStates_.endFrame(frame_.frameNumber);
  textInput_.endFrame();
  frameActive_ = false;
}

void Context::reset() noexcept {
  frame_ = {};
  animations_.clear();
  gestures_.clear();
  scrollStates_.clear();
  inputSpaces_.clear();
  drawTransforms_.beginFrame();
  textInput_.close();
  textInputDrawList_ = nullptr;
  glassRequests_.beginFrame({}, 1.0f);
  frameActive_ = false;
}

void Context::trackTextInput() noexcept {
  textInputDrawList_ = ImGui::GetWindowDrawList();
}

ImVec2 Context::textInputPosition(ImVec2 position) const noexcept {
  const DrawTransform *transform = drawTransforms_.find(textInputDrawList_);
  return transform ? transform->mapPoint(position) : position;
}

}
