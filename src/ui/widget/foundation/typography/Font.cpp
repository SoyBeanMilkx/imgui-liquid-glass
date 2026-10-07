#include "Font.hpp"

#include "generated/sf_pro_text_regular.hpp"
#include "generated/sf_pro_text_semibold.hpp"

#include <cstring>
#include <limits>

namespace glass_ui::widget {

ImFont *AddFontFromMemory(const void *data, std::size_t size,
                          const FontOptions &options) {
  if (!ImGui::GetCurrentContext() || !data || size == 0 ||
      size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return nullptr;

  void *fontData = const_cast<void *>(data);
  if (options.dataMode == FontDataMode::Copy) {
    fontData = ImGui::MemAlloc(size);
    if (!fontData)
      return nullptr;
    std::memcpy(fontData, data, size);
  }

  ImFontConfig config;
  config.FontDataOwnedByAtlas = options.dataMode == FontDataMode::Copy;
  config.MergeMode = options.merge;
  config.PixelSnapH = options.pixelSnap;
  config.FontNo = options.fontIndex;
  return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
      fontData, static_cast<int>(size), options.defaultSize, &config);
}

BundledFontFamily AddBundledDefaultFonts(float defaultSize) {
  FontOptions options;
  options.defaultSize = defaultSize;
  options.dataMode = FontDataMode::Borrowed;
  BundledFontFamily family;
  family.regular = AddFontFromMemory(kBundledRegularFontData,
                                     sizeof(kBundledRegularFontData), options);
  family.semibold = AddFontFromMemory(
      kBundledSemiboldFontData, sizeof(kBundledSemiboldFontData), options);
  return family;
}

}
