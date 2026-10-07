#pragma once

#include "VulkanGlassBackend.hpp"

#include "imgui.h"

#include <vector>

namespace glass_ui::widget {

class VulkanGlassDrawCallbacks final {
public:
  VulkanGlassDrawCallbacks() = default;
  ~VulkanGlassDrawCallbacks();

  VulkanGlassDrawCallbacks(const VulkanGlassDrawCallbacks &) = delete;
  VulkanGlassDrawCallbacks &
  operator=(const VulkanGlassDrawCallbacks &) = delete;

  void inject(ImDrawData &drawData, const GlassRequestQueue &queue,
              const VulkanGlassFrameInfo &frame, VulkanGlassBackend &backend);
  void clear();

private:
  struct CallbackData {
    VulkanGlassBackend *backend = nullptr;
    VulkanGlassFrameInfo frame{};
    const GlassRequest *request = nullptr;
  };

  struct DrawListInjection {
    ImDrawList *drawList = nullptr;
    std::vector<int> commandOffsets;
  };

  static void render(const ImDrawList *, const ImDrawCmd *command);
  static bool belongsTo(const ImDrawData &drawData,
                        const ImDrawList *drawList) noexcept;

  std::vector<CallbackData> callbackData_;
  std::vector<DrawListInjection> injections_;
};

}
