#pragma once

#include "PageContext.hpp"

#include <string>

namespace glass_ui::page {

class SearchPage final {
public:
  void draw(PageContext &context);

private:
  std::string query_;
  std::string name_;
};

}
