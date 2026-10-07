#include "DrawTransform.hpp"

#include <algorithm>

namespace glass_ui::widget {

DrawTransform DrawTransform::around(ImVec2 pivot, ImVec2 scale,
                                     ImVec2 offset) noexcept {
  scale.x = std::max(scale.x, 0.01f);
  scale.y = std::max(scale.y, 0.01f);
  return DrawTransform{
      scale, ImVec2(pivot.x * (1.0f - scale.x) + offset.x,
                     pivot.y * (1.0f - scale.y) + offset.y)};
}

ImVec2 DrawTransform::mapPoint(ImVec2 point) const noexcept {
  return ImVec2(point.x * scale.x + offset.x,
                 point.y * scale.y + offset.y);
}

ImVec2 DrawTransform::inversePoint(ImVec2 point) const noexcept {
  return ImVec2((point.x - offset.x) / scale.x,
                 (point.y - offset.y) / scale.y);
}

LayoutRect DrawTransform::mapBounds(const LayoutRect &bounds) const noexcept {
  return LayoutRect{mapPoint(bounds.minimum), mapPoint(bounds.maximum)};
}

ImVec4 DrawTransform::mapClipRect(ImVec4 clip) const noexcept {
  const LayoutRect bounds =
      mapBounds(LayoutRect{ImVec2(clip.x, clip.y), ImVec2(clip.z, clip.w)});
  return ImVec4(bounds.minimum.x, bounds.minimum.y,
                 bounds.maximum.x, bounds.maximum.y);
}

void DrawTransform::applyVertices(ImDrawList &drawList, int begin,
                                  int end) const noexcept {
  begin = std::clamp(begin, 0, drawList.VtxBuffer.Size);
  end = std::clamp(end, begin, drawList.VtxBuffer.Size);
  for (int index = begin; index < end; ++index)
    drawList.VtxBuffer[index].pos = mapPoint(drawList.VtxBuffer[index].pos);
}

void DrawTransformStore::beginFrame() {
  scopes_.clear();
  entries_.clear();
  foreground_ = nullptr;
  applied_ = false;
}

void DrawTransformStore::push(DrawTransform transform) {
  if (!scopes_.empty()) {
    const DrawTransform &parent = scopes_.back();
    transform.offset = parent.mapPoint(transform.offset);
    transform.scale.x *= parent.scale.x;
    transform.scale.y *= parent.scale.y;
  }
  scopes_.push_back(transform);
}

void DrawTransformStore::pop() {
  if (!scopes_.empty())
    scopes_.pop_back();
}

void DrawTransformStore::attach(ImDrawList *drawList) {
  if (!drawList || scopes_.empty())
    return;
  for (Entry &entry : entries_) {
    if (entry.drawList == drawList && entry.end < 0) {
      entry.transform = scopes_.back();
      return;
    }
  }
  entries_.push_back(Entry{drawList, scopes_.back()});
}

const DrawTransform *DrawTransformStore::find(
    const ImDrawList *drawList) const noexcept {
  for (const Entry &entry : entries_) {
    if (entry.drawList == drawList && entry.end < 0)
      return &entry.transform;
  }
  return nullptr;
}

ForegroundDraw DrawTransformStore::foreground() {
  return ForegroundDraw(*this);
}

void DrawTransformStore::apply(ImDrawData &drawData) {
  if (applied_)
    return;
  for (ImDrawList *drawList : drawData.CmdLists) {
    for (const Entry &entry : entries_) {
      if (entry.drawList != drawList)
        continue;
      entry.transform.applyVertices(
          *drawList, entry.begin,
          entry.end < 0 ? drawList->VtxBuffer.Size : entry.end);
      if (entry.end < 0) {
        for (ImDrawCmd &command : drawList->CmdBuffer)
          command.ClipRect = entry.transform.mapClipRect(command.ClipRect);
      }
    }
  }
  applied_ = true;
}

ForegroundDraw::ForegroundDraw(DrawTransformStore &store)
    : store_(store), drawList_(ImGui::GetForegroundDrawList()),
      previous_(store.foreground_), begin_(drawList_->VtxBuffer.Size) {
  if (const DrawTransform *transform = store.find(ImGui::GetWindowDrawList()))
    transform_ = *transform;
  if (previous_)
    previous_->flush();
  store_.foreground_ = this;
}

ForegroundDraw::~ForegroundDraw() {
  flush();
  store_.foreground_ = previous_;
  if (previous_)
    previous_->begin_ = previous_->drawList_->VtxBuffer.Size;
}

void ForegroundDraw::flush() {
  const int end = drawList_->VtxBuffer.Size;
  if (end > begin_)
    store_.entries_.push_back({drawList_, transform_, begin_, end});
  begin_ = end;
}

} // namespace glass_ui::widget
