#include "HomePage.hpp"

#include "ui/widget/Widget.hpp"

namespace glass_ui::page {

void HomePage::draw(PageContext &context) {
  const char *tabs[] = {"Account", "Notifications", "Billing"};
  widget::TabBarOptions options;
  options.font = context.regularFont;
  options.fontSize = 12.0f;
  options.height = 30.0f;
  options.horizontalPadding = 1.0f;
  options.spacing = 18.0f;
  options.indicatorHeight = 1.5f;
  widget::TabBar(context.widgets, "##home-tabs", tabs, 3, &selectedTab_,
                 options);

  ImGui::Dummy(ImVec2(0.0f, 5.0f * context.widgets.frame().density));
  drawSelectedTab(context);
}

void HomePage::drawSelectedTab(PageContext &context) {
  const widget::SwipePagerResult pager = widget::BeginSwipePager(
      context.widgets, "##home-pages", 3, &selectedTab_);
  if (pager.visible) {
    constexpr const char *scrollIds[] = {
        "##account-scroll", "##notifications-scroll", "##billing-scroll"};
    const widget::ScrollViewResult scroll = widget::BeginScrollView(
        context.widgets, scrollIds[selectedTab_]);
    if (scroll.visible) {
      if (selectedTab_ == 0)
        widget::Text(context.widgets, "Account settings");
      else if (selectedTab_ == 1)
        drawNotifications(context);
      else
        widget::Text(context.widgets, "Billing settings");
    }
    widget::EndScrollView(context.widgets);
  }
  widget::EndSwipePager(context.widgets);
}

void HomePage::drawNotifications(PageContext &context) {
  widget::SwitchRowOptions row;
  row.titleFont = context.semiboldFont;
  row.descriptionFont = context.regularFont;
  row.switchOptions.size = ImVec2(28.0f, 17.0f);
  row.switchOptions.minimumTouchSize = 40.0f;
  widget::SwitchRow(
      context.widgets, "##mentions", "Mentions",
      "Receive notifications when you are mentioned by other event "
      "collaborators.",
      &context.state.notificationSettings[0], row);
  widget::SwitchRow(
      context.widgets, "##event-invites", "New event invites",
      "Receive notifications when someone invites you to a new event.",
      &context.state.notificationSettings[1], row);
  widget::SwitchRow(context.widgets, "##reminders", "Reminders",
                    "Receive notifications for tasks, events and incomplete "
                    "information.",
                    &context.state.notificationSettings[2], row);
  widget::SwitchRow(
      context.widgets, "##announcements", "Announcements",
      "Receive notifications for product related announcements and new "
      "features.",
      &context.state.notificationSettings[3], row);
}

}
