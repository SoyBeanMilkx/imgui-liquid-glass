#include "InputSpace.hpp"

namespace glass_ui::widget {

const InputSpace *InputSpaceStore::find(ImGuiID id) const noexcept {
  for (const InputSpace &space : spaces_) {
    if (space.id == id)
      return &space;
  }
  return nullptr;
}

const InputSpace *InputSpaceStore::hit(ImVec2 position) const noexcept {
  for (auto iterator = spaces_.rbegin(); iterator != spaces_.rend(); ++iterator) {
    const ImVec2 point = iterator->transform.inversePoint(position);
    const LayoutRect &bounds = iterator->bounds;
    if (point.x >= bounds.minimum.x && point.x <= bounds.maximum.x &&
        point.y >= bounds.minimum.y && point.y <= bounds.maximum.y)
      return &*iterator;
  }
  return nullptr;
}

ImVec2 InputSpaceStore::mapPoint(const InputSpace *space,
                               ImVec2 position) const noexcept {
  if (!ImGui::IsMousePosValid(&position))
    return position;
  if (space)
    return space->transform.inversePoint(position);
  return spaces_.empty() ? position : ImVec2(-FLT_MAX, -FLT_MAX);
}

void InputSpaceStore::beginFrame(PointerEventView events, InputSpaceView spaces,
                                ImGuiIO &io) {
  const bool mappedBefore = !spaces_.empty();
  if (spaces.count)
    spaces_.assign(spaces.spaces, spaces.spaces + spaces.count);
  else
    spaces_.clear();
  events_.clear();
  const InputSpace *mouseSpace = down_ ? find(captured_) : hit(pointer_);
  for (std::size_t index = 0; index < events.count; ++index) {
    PointerEvent event = events.events[index];
    pointer_ = event.position;
    if (event.phase == PointerPhase::Down) {
      mouseSpace = hit(pointer_);
      captured_ = mouseSpace ? mouseSpace->id : 0;
      down_ = true;
    } else {
      mouseSpace = down_ ? find(captured_) : hit(pointer_);
    }
    event.position = mapPoint(mouseSpace, pointer_);
    events_.push(event);
    if (event.phase == PointerPhase::Up) {
      captured_ = 0;
      down_ = false;
    } else if (event.phase == PointerPhase::Cancel) {
      captured_ = 0;
      down_ = false;
      pointer_ = ImVec2(-FLT_MAX, -FLT_MAX);
      mouseSpace = nullptr;
    }
  }
  if (!spaces_.empty() || mappedBefore) {
    const ImVec2 mouse = mapPoint(mouseSpace, pointer_);
    io.AddMousePosEvent(mouse.x, mouse.y);
  }
}

void InputSpaceStore::clear() noexcept {
  spaces_.clear();
  events_.clear();
  pointer_ = ImVec2(-FLT_MAX, -FLT_MAX);
  captured_ = 0;
  down_ = false;
}

}
