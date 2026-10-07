#include "MessagesPage.hpp"

#include "ui/widget/components/controls/Text.hpp"

namespace glass_ui::page {

void MessagesPage::draw(PageContext &context) {
  widget::Text(context.widgets, "Page 4");
}

}
