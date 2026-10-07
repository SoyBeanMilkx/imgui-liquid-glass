#pragma once

#include "ui/input/PointerEvent.hpp"
#include "ui/widget/foundation/drawing/DrawTransform.hpp"

#include <cfloat>
#include <vector>

namespace glass_ui::widget {

struct InputSpace {
  ImGuiID id = 0;
  LayoutRect bounds;
  DrawTransform transform;
};

struct InputSpaceView {
  const InputSpace *spaces = nullptr;
  std::size_t count = 0;
};

class InputSpaceStore final {
public:
  void beginFrame(PointerEventView events, InputSpaceView spaces, ImGuiIO &io);
  PointerEventView events() const noexcept { return events_.view(); }
  void clear() noexcept;

private:
  const InputSpace *find(ImGuiID id) const noexcept;
  const InputSpace *hit(ImVec2 position) const noexcept;
  ImVec2 mapPoint(const InputSpace *space, ImVec2 position) const noexcept;

  std::vector<InputSpace> spaces_;
  PointerEventBatch events_;
  ImVec2 pointer_ = ImVec2(-FLT_MAX, -FLT_MAX);
  ImGuiID captured_ = 0;
  bool down_ = false;
};

}
