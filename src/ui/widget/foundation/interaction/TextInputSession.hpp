#pragma once

#include "imgui.h"

#include <cstdint>
#include <optional>
#include <string>

namespace glass_ui::widget {

// Positions are UTF-8 byte offsets, matching InputText callbacks.
struct TextEditState {
  std::string text;
  int cursor = 0;
  int selectionStart = 0;
  int selectionEnd = 0;
  int composingStart = -1;
  int composingEnd = -1;
};

struct TextInputRequest {
  uint64_t token = 0;
  uint64_t revision = 0;
  uint64_t platformVersion = 0;
  uint64_t keyboardRequest = 0;
  ImGuiID id = 0;
  TextEditState edit;
  bool password = false;
};

struct TextInputUpdate {
  uint64_t token = 0;
  uint64_t revision = 0;
  uint64_t version = 0;
  TextEditState edit;
  bool finished = false;
};

class TextInputSession final {
public:
  void beginFrame() noexcept;
  void endFrame() noexcept;
  void focus(ImGuiID id, const char *text, bool password);
  void publish(ImGuiID id, TextEditState edit);
  bool apply(TextInputUpdate update);
  bool consume(ImGuiID id, TextEditState &edit);
  void requestKeyboard() noexcept;
  void close() noexcept;
  const TextInputRequest &request() const noexcept { return request_; }

private:
  TextInputRequest request_;
  std::optional<TextEditState> pending_;
  uint64_t nextToken_ = 0;
  bool seen_ = false;
};

}
