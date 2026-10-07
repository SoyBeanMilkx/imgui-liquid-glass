#include "InputBridge.hpp"

#include "abi.h"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstring>

namespace glass_ui {
namespace {

const char *errorText(int error) noexcept {
  return std::strerror(error < 0 ? -error : error);
}

float mapCoordinate(float value, uint32_t sourceSpan,
                    uint32_t targetSpan) noexcept {
  return static_cast<float>(static_cast<double>(value) / sourceSpan *
                            targetSpan);
}

bool sameRegion(const toucher_region &left,
                const toucher_region &right) noexcept {
  return left.type == right.type && left.left == right.left &&
         left.top == right.top && left.right == right.right &&
         left.bottom == right.bottom;
}

bool sameRegions(const toucher_region *left, const toucher_region *right,
                 uint32_t count) noexcept {
  for (uint32_t index = 0; index < count; ++index) {
    if (!sameRegion(left[index], right[index]))
      return false;
  }
  return true;
}

}

InputBridge::~InputBridge() { suspend(); }

bool InputBridge::initialize() noexcept { return connect(); }

void InputBridge::configureImGui(ImGuiIO &io) noexcept {
  io.ConfigInputTrickleEventQueue = false;
}

bool InputBridge::connect() noexcept {
  if (client_)
    return true;
  if (std::chrono::steady_clock::now() < nextRetry_)
    return false;

  const int result = toucher_create(nullptr, &client_);
  if (result) {
    disconnect(result);
    return false;
  }
  surfaceReady_ = false;
  return true;
}

bool InputBridge::acquire() noexcept {
  if (owner_)
    return true;

  std::array<toucher_device_info, TOUCHER_DEVICE_MAX> devices{};
  uint32_t count = 0;
  int result = toucher_refresh_devices(client_);
  if (!result)
    result =
        toucher_get_devices(client_, devices.data(), devices.size(), &count);
  if (result) {
    disconnect(result);
    return false;
  }

  const toucher_device_info *selected = nullptr;
  for (uint32_t i = 0; i < std::min(count, TOUCHER_DEVICE_MAX); ++i) {
    if (!(devices[i].flags & TOUCHER_DEVICE_CAPTURABLE))
      continue;
    if (!selected || devices[i].match_quality > selected->match_quality)
      selected = &devices[i];
  }
  if (!selected) {
    disconnect(-ENODEV);
    return false;
  }

  result = toucher_select_device(client_, selected->id, selected->generation);
  if (!result)
    result = toucher_capture_acquire(client_, 0);
  if (result) {
    disconnect(result);
    return false;
  }
  owner_ = true;
  regionPublished_ = false;
  lastError_ = 0;
  nextRetry_ = {};
  log::info("Toucher capture acquired: device=%u generation=%u", selected->id,
            selected->generation);
  return true;
}

bool InputBridge::updateSurface(const InputSurface &surface) noexcept {
  if (surfaceReady_ && surface.width == surface_.width &&
      surface.height == surface_.height &&
      surface.transform == surface_.transform)
    return true;

  if (surface.width < 2 || surface.height < 2 ||
      surface.transform > TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_270) {
    disconnect(-EOPNOTSUPP);
    return false;
  }

  toucher_surface_info info{};
  info.display_width = surface.width;
  info.display_height = surface.height;
  info.width = surface.width;
  info.height = surface.height;
  info.transform = surface.transform;
  const int result = toucher_set_surface(client_, &info);
  if (result) {
    disconnect(result);
    return false;
  }
  cancelPending_ = cancelPending_ || surfaceReady_;
  pendingIndex_ = pendingCount_ = 0;
  regionPublished_ = false;
  surface_ = surface;
  surfaceReady_ = true;
  return true;
}

void InputBridge::suspend() noexcept {
  cancelPending_ = cancelPending_ || owner_ || pointerDown_ || pendingCount_;
  if (client_) {
    toucher_destroy(client_);
    client_ = nullptr;
  }
  owner_ = false;
  wanted_ = false;
  surfaceReady_ = false;
  regionPublished_ = false;
  pendingIndex_ = pendingCount_ = 0;
  nextRetry_ = {};
}

void InputBridge::disconnect(int error) noexcept {
  suspend();
  nextRetry_ = std::chrono::steady_clock::now() + RetryDelay;
  if (error != lastError_)
    log::warn("Toucher capture unavailable: %d (%s)", error, errorText(error));
  lastError_ = error;
}

void InputBridge::cancelPointer(ImGuiIO &io,
                                PointerEventBatch &events) noexcept {
  io.ClearEventsQueue();
  io.ClearInputMouse();
  events.clear();
  events.push(PointerEvent{PointerPhase::Cancel, lastPointerPosition_,
                           lastPointerTimestampNs_});
  pointerDown_ = false;
  sequenceId_ = 0;
  cancelPending_ = false;
}

bool InputBridge::applyEvent(ImGuiIO &io, const toucher_capture_event &event,
                             PointerEventBatch &events) noexcept {
  if (event.type == TOUCHER_POINTER_CANCEL ||
      event.type == TOUCHER_POINTER_RESET) {
    lastPointerTimestampNs_ = event.timestamp_ns;
    cancelPointer(io, events);
    return true;
  }
  if (!std::isfinite(event.x) || !std::isfinite(event.y) ||
      (event.type == TOUCHER_POINTER_DOWN && pointerDown_) ||
      (event.type != TOUCHER_POINTER_DOWN &&
       (!pointerDown_ || sequenceId_ != event.sequence_id))) {
    disconnect(-EPROTO);
    cancelPointer(io, events);
    return true;
  }

  lastPointerPosition_ =
      ImVec2(mapCoordinate(event.x, surface_.width - 1, surface_.width),
             mapCoordinate(event.y, surface_.height - 1, surface_.height));
  lastPointerTimestampNs_ = event.timestamp_ns;
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMousePosEvent(lastPointerPosition_.x, lastPointerPosition_.y);
  switch (event.type) {
  case TOUCHER_POINTER_DOWN:
    sequenceId_ = event.sequence_id;
    pointerDown_ = true;
    events.push(PointerEvent{PointerPhase::Down, lastPointerPosition_,
                             lastPointerTimestampNs_});
    io.AddMouseButtonEvent(0, true);
    return true;
  case TOUCHER_POINTER_UP:
    pointerDown_ = false;
    sequenceId_ = 0;
    events.push(PointerEvent{PointerPhase::Up, lastPointerPosition_,
                             lastPointerTimestampNs_});
    io.AddMouseButtonEvent(0, false);
    return true;
  case TOUCHER_POINTER_MOVE:
    events.push(PointerEvent{PointerPhase::Move, lastPointerPosition_,
                             lastPointerTimestampNs_});
    return false;
  default:
    disconnect(-EPROTO);
    cancelPointer(io, events);
    return true;
  }
}

bool InputBridge::setCaptureEnabled(bool enabled) noexcept {
  if (captureEnabled_ == enabled)
    return true;
  captureEnabled_ = enabled;
  regionPublished_ = false;
  pendingIndex_ = pendingCount_ = 0;

  if (!enabled) {
    cancelPending_ = cancelPending_ || pointerDown_;
    if (owner_) {
      const int result = toucher_capture_set_regions(client_, nullptr, 0);
      if (result) {
        disconnect(result);
        return false;
      }
    }
    log::info("Toucher regions paused");
  } else {
    log::info("Toucher regions resumed");
  }
  return true;
}

void InputBridge::poll(ImGuiIO &io, const InputSurface &surface,
                       PointerEventBatch &events, bool captureEnabled) {
  events.clear();
  if (!setCaptureEnabled(captureEnabled))
    return;
  if (cancelPending_) {
    cancelPointer(io, events);
    return;
  }
  if (!captureEnabled_) {
    if (owner_) {
      const int result = toucher_capture_keepalive(client_);
      if (result)
        disconnect(result);
    }
    return;
  }
  if (!wanted_)
    return;
  if (!pointerDown_)
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  if (!connect())
    return;
  if (!updateSurface(surface) || !acquire()) {
    if (cancelPending_ || pointerDown_)
      cancelPointer(io, events);
    return;
  }
  if (cancelPending_) {
    cancelPointer(io, events);
    return;
  }

  int result;
  if (pendingIndex_ < pendingCount_) {
    result = toucher_capture_keepalive(client_);
  } else {
    pendingIndex_ = pendingCount_ = 0;
    result = toucher_capture_poll(client_, pending_.data(), pending_.size(),
                                  &pendingCount_);
  }
  if (result) {
    pendingIndex_ = pendingCount_ = 0;
    if (result != -EOVERFLOW)
      disconnect(result);
    else
      log::warn("Toucher capture overflow, cancelling interaction");
    cancelPointer(io, events);
    return;
  }

  while (pendingIndex_ < pendingCount_) {
    const toucher_capture_event event = pending_[pendingIndex_++];
    if (applyEvent(io, event, events))
      break;
  }
}

void InputBridge::publishRegions(const InputRegionSet &regions) noexcept {
  if (regions.empty()) {
    suspend();
    return;
  }
  wanted_ = true;
  if (!captureEnabled_)
    return;
  if (!owner_ || !surfaceReady_ || cancelPending_)
    return;

  std::array<toucher_region, InputRegionSet::Capacity> next{};
  uint32_t count = 0;
  for (std::size_t index = 0; index < regions.size(); ++index) {
    const InputRegion &region = regions.data()[index];
    if (region.maximum.x <= region.minimum.x ||
        region.maximum.y <= region.minimum.y)
      continue;
    toucher_region &output = next[count++];
    output.type = region.shape == InputRegionShape::Ellipse
                      ? TOUCHER_REGION_ELLIPSE
                      : TOUCHER_REGION_RECT;
    output.left =
        mapCoordinate(region.minimum.x, surface_.width, surface_.width - 1);
    output.top =
        mapCoordinate(region.minimum.y, surface_.height, surface_.height - 1);
    output.right =
        mapCoordinate(region.maximum.x, surface_.width, surface_.width - 1);
    output.bottom =
        mapCoordinate(region.maximum.y, surface_.height, surface_.height - 1);
  }
  if (!count) {
    suspend();
    return;
  }
  if (regionPublished_ && count == publishedRegionCount_ &&
      sameRegions(next.data(), publishedRegions_.data(), count))
    return;

  const int result = toucher_capture_set_regions(client_, next.data(), count);
  if (result) {
    disconnect(result);
    return;
  }
  publishedRegions_ = next;
  publishedRegionCount_ = count;
  regionPublished_ = true;
}

}
