#include "NotificationsPage.hpp"

#include "ui/widget/components/controls/Text.hpp"

namespace glass_ui::page {

void NotificationsPage::draw(PageContext &context) {
  widget::Text(context.widgets, "Page 3");
}

}
