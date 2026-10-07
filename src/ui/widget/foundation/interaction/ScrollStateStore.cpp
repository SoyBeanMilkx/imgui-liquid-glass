#include "ScrollStateStore.hpp"

namespace glass_ui::widget {

void ScrollStateStore::beginFrame() noexcept { scopeDepth_ = 0; }

ScrollState &ScrollStateStore::acquire(ImGuiID id, uint64_t frameNumber) {
  ScrollState &state = states_[id];
  state.lastFrame = frameNumber;
  return state;
}

void ScrollStateStore::pushScope(const ScrollScope &scope) noexcept {
  if (scopeDepth_ < scopes_.size())
    scopes_[scopeDepth_++] = scope;
}

ScrollScope *ScrollStateStore::currentScope() noexcept {
  return scopeDepth_ > 0 ? &scopes_[scopeDepth_ - 1] : nullptr;
}

void ScrollStateStore::popScope() noexcept {
  if (scopeDepth_ > 0)
    --scopeDepth_;
}

void ScrollStateStore::endFrame(uint64_t frameNumber) {
  scopeDepth_ = 0;
  constexpr uint64_t collectionInterval = 120;
  constexpr uint64_t unusedFrameLifetime = 300;
  if (frameNumber < lastCollectionFrame_ + collectionInterval)
    return;
  for (auto iterator = states_.begin(); iterator != states_.end();) {
    if (frameNumber > iterator->second.lastFrame + unusedFrameLifetime)
      iterator = states_.erase(iterator);
    else
      ++iterator;
  }
  lastCollectionFrame_ = frameNumber;
}

void ScrollStateStore::clear() noexcept {
  states_.clear();
  scopes_ = {};
  scopeDepth_ = 0;
  lastCollectionFrame_ = 0;
}

}
