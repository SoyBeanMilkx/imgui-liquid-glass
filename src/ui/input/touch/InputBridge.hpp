#pragma once

#include <array>
#include <chrono>
#include <cstdint>

#include "ui/input/InputRegion.hpp"
#include "ui/input/InputSurface.hpp"
#include "ui/input/PointerEvent.hpp"
#include "toucher.h"

struct ImGuiIO;

namespace glass_ui {

class InputBridge final {
public:
  InputBridge() = default;
  ~InputBridge();

  InputBridge(const InputBridge &) = delete;
  InputBridge &operator=(const InputBridge &) = delete;
  InputBridge(InputBridge &&) = delete;
  InputBridge &operator=(InputBridge &&) = delete;

  bool initialize() noexcept;
  void configureImGui(ImGuiIO &io) noexcept;
  void poll(ImGuiIO &io, const InputSurface &surface, PointerEventBatch &events,
            bool captureEnabled);
  void publishRegions(const InputRegionSet &regions) noexcept;
  bool setCaptureEnabled(bool enabled) noexcept;
  void suspend() noexcept;

private:
  bool connect() noexcept;
  bool acquire() noexcept;
  bool updateSurface(const InputSurface &surface) noexcept;
  void disconnect(int error) noexcept;
  void cancelPointer(ImGuiIO &io, PointerEventBatch &events) noexcept;
  bool applyEvent(ImGuiIO &io, const toucher_capture_event &event,
                  PointerEventBatch &events) noexcept;

  static constexpr auto RetryDelay = std::chrono::seconds(1);

  toucher_client *client_ = nullptr;
  InputSurface surface_{};
  std::array<toucher_region, InputRegionSet::Capacity> publishedRegions_{};
  uint32_t publishedRegionCount_ = 0;
  std::array<toucher_capture_event, PointerEventBatch::Capacity> pending_{};
  uint32_t pendingIndex_ = 0;
  uint32_t pendingCount_ = 0;
  uint64_t sequenceId_ = 0;
  std::chrono::steady_clock::time_point nextRetry_{};
  int lastError_ = 0;
  bool surfaceReady_ = false;
  bool wanted_ = false;
  bool owner_ = false;
  bool regionPublished_ = false;
  bool cancelPending_ = false;
  bool pointerDown_ = false;
  bool captureEnabled_ = true;
  ImVec2 lastPointerPosition_{};
  uint64_t lastPointerTimestampNs_ = 0;
};

}
