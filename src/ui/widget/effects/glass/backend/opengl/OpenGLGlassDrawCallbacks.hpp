#pragma once

#include "OpenGLGlassBackend.hpp"

#include "imgui.h"

#include <vector>

namespace glass_ui::widget {

class OpenGLGlassDrawCallbacks final {
public:
  ~OpenGLGlassDrawCallbacks();

  OpenGLGlassDrawCallbacks(const OpenGLGlassDrawCallbacks &) = delete;
  OpenGLGlassDrawCallbacks &
  operator=(const OpenGLGlassDrawCallbacks &) = delete;
  OpenGLGlassDrawCallbacks() = default;

  void inject(ImDrawData &drawData, const GlassRequestQueue &queue,
              OpenGLGlassBackend &backend);
  void clear();

private:
  struct CallbackData {
    OpenGLGlassBackend *backend = nullptr;
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
