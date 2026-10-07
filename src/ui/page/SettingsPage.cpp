#include "SettingsPage.hpp"

#include "ui/widget/Widget.hpp"

#include <cstdio>

namespace glass_ui::page {

SettingsPageResult SettingsPage::draw(PageContext &context,
                                      bool resizeMode) {
  SettingsPageResult result{resizeMode, false};
  const widget::ScrollViewResult scroll =
      widget::BeginScrollView(context.widgets, "##settings-scroll");
  if (!scroll.visible) {
    widget::EndScrollView(context.widgets);
    return result;
  }

  widget::TextOptions section;
  section.font = context.semiboldFont;
  section.fontSize = 12.0f;
  section.color = ImVec4(0.98f, 0.99f, 1.0f, 0.72f);
  widget::Text(context.widgets, "Window", section);

  widget::SwitchRowOptions row;
  row.titleFont = context.semiboldFont;
  row.descriptionFont = context.regularFont;
  row.switchOptions.size = ImVec2(28.0f, 17.0f);
  row.switchOptions.minimumTouchSize = 40.0f;
  result.resizeModeChanged = widget::SwitchRow(
      context.widgets, "##settings-resize", "Resize window",
      "Drag the highlighted right corners to resize.", &result.resizeMode,
      row);

  ImGui::Dummy(ImVec2(0.0f, 5.0f * context.widgets.frame().density));
  widget::Text(context.widgets, "Appearance", section);
  char blurLabel[32];
  std::snprintf(blurLabel, sizeof(blurLabel), "Glass blur  %.1f",
                context.state.glassBlurRadius);
  widget::TextOptions label;
  label.font = context.semiboldFont;
  label.fontSize = 14.0f;
  label.color = ImVec4(0.98f, 0.99f, 1.0f, 0.92f);
  widget::Text(context.widgets, blurLabel, label);
  widget::SliderOptions slider;
  slider.size = ImVec2(-8.0f, 0.0f);
  widget::Slider(context.widgets, "##settings-glass-blur",
                 &context.state.glassBlurRadius, 4.0f, 24.0f, slider);

  ImGui::Dummy(ImVec2(0.0f, 5.0f * context.widgets.frame().density));
  widget::Text(context.widgets, "Notifications", section);
  widget::SwitchRow(context.widgets, "##settings-mentions", "Mentions",
                    "Notify me when someone mentions me.",
                    &context.state.notificationSettings[0], row);
  widget::SwitchRow(context.widgets, "##settings-invites", "Event invites",
                    "Notify me about new event invitations.",
                    &context.state.notificationSettings[1], row);
  widget::SwitchRow(context.widgets, "##settings-reminders", "Reminders",
                    "Show reminders for upcoming tasks and events.",
                    &context.state.notificationSettings[2], row);
  widget::EndScrollView(context.widgets);
  return result;
}

}
