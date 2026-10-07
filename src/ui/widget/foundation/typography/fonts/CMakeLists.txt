set(GLASS_UI_FONT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

function(glass_ui_embed_widget_font name source symbol)
    set(header "${GLASS_UI_FONT_GENERATED_DIR}/${name}.hpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" font_data HEX)
    string(REGEX REPLACE "(..)" "0x\\1, " font_data "${font_data}")
    file(MAKE_DIRECTORY "${GLASS_UI_FONT_GENERATED_DIR}")
    file(WRITE "${header}"
        "#pragma once\n\n#include <cstddef>\n\nalignas(4) inline constexpr unsigned char ${symbol}[] = { ${font_data} };\n")
endfunction()

glass_ui_embed_widget_font(
    sf_pro_text_regular
    "${CMAKE_CURRENT_SOURCE_DIR}/SF-Pro-Text-Regular-Latin.otf"
    kBundledRegularFontData)
glass_ui_embed_widget_font(
    sf_pro_text_semibold
    "${CMAKE_CURRENT_SOURCE_DIR}/SF-Pro-Text-Semibold-Latin.otf"
    kBundledSemiboldFontData)

set(GLASS_UI_FONT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
