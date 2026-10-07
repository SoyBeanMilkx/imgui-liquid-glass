#include "SearchPage.hpp"

#include "ui/widget/components/controls/Text.hpp"
#include "ui/widget/components/controls/TextField.hpp"

namespace glass_ui::page {

void SearchPage::draw(PageContext &context) {
  auto &widgets = context.widgets;
  const float density = widgets.frame().density;
  widget::Text(widgets, "Find something");
  ImGui::Dummy(ImVec2(0.0f, 12.0f * density));
  widget::TextFieldOptions search;
  search.hint = "Search...";
  search.icon = widget::IconGlyph::Search;
  search.rounding = 24.0f;
  widget::TextField(widgets, "search", &query_, search);
  ImGui::Dummy(ImVec2(0.0f, 24.0f * density));
  widget::Text(widgets, "Display name");
  ImGui::Dummy(ImVec2(0.0f, 10.0f * density));
  widget::TextFieldOptions name;
  name.hint = "Enter a name";
  widget::TextField(widgets, "name", &name_, name);
}

}
