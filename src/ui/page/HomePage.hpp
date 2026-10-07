#pragma once

#include "PageContext.hpp"

namespace glass_ui::page {

class HomePage final {
public:
  void draw(PageContext &context);

private:
  void drawSelectedTab(PageContext &context);
  void drawNotifications(PageContext &context);

  int selectedTab_ = 1;
};

}
