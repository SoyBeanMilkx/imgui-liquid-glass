#include "TextInputSession.hpp"

#include <utility>

namespace glass_ui::widget {

void TextInputSession::beginFrame() noexcept { seen_ = false; }
void TextInputSession::endFrame() noexcept {
  if (!seen_)
    close();
}

void TextInputSession::focus(ImGuiID id, const char *text, bool password) {
  seen_ = true;
  if (request_.id == id && request_.token && request_.password == password)
    return;
  request_ = {};
  request_.id = id;
  request_.token = ++nextToken_;
  request_.revision = 1;
  request_.keyboardRequest = 1;
  request_.password = password;
  request_.edit.text = text ? text : "";
  request_.edit.cursor = static_cast<int>(request_.edit.text.size());
  request_.edit.selectionStart = request_.edit.cursor;
  request_.edit.selectionEnd = request_.edit.cursor;
  pending_.reset();
}

void TextInputSession::publish(ImGuiID id, TextEditState edit) {
  if (request_.id != id || !request_.token)
    return;
  const TextEditState &previous = request_.edit;
  if (previous.text == edit.text && previous.cursor == edit.cursor &&
      previous.selectionStart == edit.selectionStart &&
      previous.selectionEnd == edit.selectionEnd)
    return;
  request_.edit = std::move(edit);
  ++request_.revision;
}

bool TextInputSession::apply(TextInputUpdate update) {
  if (!request_.token || request_.token != update.token ||
      request_.revision != update.revision ||
      update.version <= request_.platformVersion)
    return false;
  request_.platformVersion = update.version;
  request_.edit = update.edit;
  pending_ = std::move(update.edit);
  return true;
}

bool TextInputSession::consume(ImGuiID id, TextEditState &edit) {
  if (request_.id != id || !pending_)
    return false;
  edit = std::move(*pending_);
  pending_.reset();
  return true;
}

void TextInputSession::requestKeyboard() noexcept {
  if (request_.token)
    ++request_.keyboardRequest;
}

void TextInputSession::close() noexcept {
  request_ = {};
  pending_.reset();
  seen_ = false;
}

}
