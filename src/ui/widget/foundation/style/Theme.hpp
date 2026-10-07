#pragma once

#include "imgui.h"

namespace glass_ui::widget {

struct Theme {
  ImFont *textFont = nullptr;
  ImFont *buttonFont = nullptr;
  ImFont *tabFont = nullptr;
  float textFontSize = 0.0f;
  float buttonFontSize = 0.0f;
  float tabFontSize = 0.0f;
  ImVec4 textPrimary = ImVec4(0.96f, 0.97f, 1.0f, 1.0f);
  ImVec4 buttonFill = ImVec4(0.18f, 0.48f, 0.92f, 0.92f);
  ImVec4 buttonHovered = ImVec4(0.24f, 0.55f, 1.0f, 0.96f);
  ImVec4 buttonPressed = ImVec4(0.12f, 0.37f, 0.78f, 1.0f);
  ImVec4 buttonText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  // Neutral tracks inherit color from the blurred surface.
  ImVec4 switchTrackOff = ImVec4(0.08f, 0.10f, 0.11f, 0.52f);
  ImVec4 switchTrackOn = ImVec4(0.88f, 0.96f, 0.95f, 0.38f);
  ImVec4 switchThumb = ImVec4(0.98f, 0.99f, 1.0f, 1.0f);
  ImVec4 switchGlow = ImVec4(0.82f, 1.0f, 0.96f, 0.50f);
  ImVec4 sliderTrack = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
  ImVec4 sliderFill = ImVec4(0.04f, 0.52f, 1.0f, 0.95f);
  ImVec4 sliderThumb = ImVec4(0.98f, 0.99f, 1.0f, 1.0f);
  ImVec4 tabText = ImVec4(0.82f, 0.84f, 0.86f, 0.88f);
  ImVec4 tabTextSelected = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  ImVec4 tabIndicator = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
  ImVec4 divider = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
  ImVec4 scrollbarThumb = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
  ImVec4 scrollbarThumbHovered = ImVec4(1.0f, 1.0f, 1.0f, 0.28f);
  ImVec4 scrollbarThumbActive = ImVec4(1.0f, 1.0f, 1.0f, 0.38f);
  float buttonHeight = 44.0f;
  float buttonHorizontalPadding = 20.0f;
  float buttonRounding = 14.0f;
  float buttonAnimationResponse = 0.08f;
  float switchWidth = 51.0f;
  float switchHeight = 31.0f;
  float minimumTouchSize = 48.0f;
  float switchAnimationResponse = 0.09f;
  float switchGlowSpread = 12.0f;
  float switchGlowStrength = 0.36f;
  float sliderHeight = 38.0f;
  float sliderTrackHeight = 5.0f;
  float sliderThumbWidth = 36.0f;
  float sliderThumbHeight = 22.0f;
  float sliderActiveScale = 1.5f;
  float sliderAnimationResponse = 0.09f;
  float sliderMorphResponse = 0.24f;
  float sliderVelocityForFullEffect = 480.0f;
  float textFieldHeight = 48.0f;
  float textFieldRounding = 16.0f;
  float textFieldFocusSpread = 3.0f;
  float textFieldFocusResponse = 0.24f;
  float tabHeight = 36.0f;
  float tabHorizontalPadding = 4.0f;
  float tabSpacing = 18.0f;
  float tabIndicatorHeight = 2.0f;
  float tabAnimationResponse = 0.10f;
  float tabIndicatorAnimationDuration = 0.28f;
  float tabIndicatorStretch = 24.0f;
  float scrollbarWidth = 7.0f;
  float scrollbarRounding = 4.0f;
};

}
