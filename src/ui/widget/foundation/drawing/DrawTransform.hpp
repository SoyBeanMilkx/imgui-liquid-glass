#pragma once

#include "ui/widget/foundation/layout/LayoutTypes.hpp"

#include <vector>

namespace glass_ui::widget {

struct DrawTransform {
  ImVec2 scale = ImVec2(1.0f, 1.0f);
  ImVec2 offset{};

  static DrawTransform around(ImVec2 pivot, ImVec2 scale,
                              ImVec2 offset) noexcept;
  ImVec2 mapPoint(ImVec2 point) const noexcept;
  ImVec2 inversePoint(ImVec2 point) const noexcept;
  LayoutRect mapBounds(const LayoutRect &bounds) const noexcept;
  ImVec4 mapClipRect(ImVec4 clip) const noexcept;
  void applyVertices(ImDrawList &drawList, int begin, int end) const noexcept;
};

class ForegroundDraw;

class DrawTransformStore final {
public:
  void beginFrame();
  void push(DrawTransform transform);
  void pop();
  void attach(ImDrawList *drawList);
  ForegroundDraw foreground();
  const DrawTransform *find(const ImDrawList *drawList) const noexcept;
  void apply(ImDrawData &drawData);

private:
  friend class ForegroundDraw;
  struct Entry {
    ImDrawList *drawList;
    DrawTransform transform;
    int begin = 0;
    int end = -1;
  };
  std::vector<DrawTransform> scopes_;
  std::vector<Entry> entries_;
  ForegroundDraw *foreground_ = nullptr;
  bool applied_ = false;
};

class ForegroundDraw final {
public:
  ~ForegroundDraw();
  ForegroundDraw(const ForegroundDraw &) = delete;
  ForegroundDraw &operator=(const ForegroundDraw &) = delete;
  ImDrawList *drawList() const noexcept { return drawList_; }

private:
  friend class DrawTransformStore;
  explicit ForegroundDraw(DrawTransformStore &store);
  void flush();

  DrawTransformStore &store_;
  ImDrawList *drawList_;
  DrawTransform transform_;
  ForegroundDraw *previous_;
  int begin_;
};

} // namespace glass_ui::widget
