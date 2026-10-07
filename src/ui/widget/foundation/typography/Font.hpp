#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>

namespace glass_ui::widget {

enum class FontDataMode : uint8_t {
  Borrowed,
  Copy,
};

struct FontOptions {
  float defaultSize = 0.0f;
  uint32_t fontIndex = 0;
  bool merge = false;
  bool pixelSnap = false;
  FontDataMode dataMode = FontDataMode::Borrowed;
};

struct BundledFontFamily {
  ImFont *regular = nullptr;
  ImFont *semibold = nullptr;
};

// Registers memory-backed TTF/OTF; borrowed data must outlive the ImGui context.
ImFont *AddFontFromMemory(const void *data, std::size_t size,
                          const FontOptions &options = {});

// Registers bundled process-lifetime fonts in borrowed mode.
BundledFontFamily AddBundledDefaultFonts(float defaultSize = 14.0f);

}
