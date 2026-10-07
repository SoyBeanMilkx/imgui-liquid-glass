#pragma once

#include "imgui.h"

#include <memory>

namespace glass_ui::widget {
class TextInputSession;
struct TextInputRequest;
}

namespace glass_ui {

class AndroidIme final {
public:
  AndroidIme();
  ~AndroidIme();
  AndroidIme(const AndroidIme &) = delete;
  AndroidIme &operator=(const AndroidIme &) = delete;

  void configureImGui();
  void unconfigureImGui() noexcept;
  void poll(widget::TextInputSession &session, ImGuiIO &io);
  bool commit(const widget::TextInputRequest &request, ImVec2 displaySize,
              ImVec2 cursorPosition);
  ImVec2 cursorPosition() const noexcept { return cursor_; }
  void suspend() noexcept;
  ImVec4 visibleArea() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
  ImVec2 cursor_{};
  bool releaseEnter_ = false;
};

}
