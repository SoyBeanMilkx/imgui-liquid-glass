if(GLASS_UI_EMBED_SHADER_BINARY)
    if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
        message(FATAL_ERROR "Shader embedding requires INPUT, OUTPUT and SYMBOL")
    endif()

    file(READ "${INPUT}" binary HEX)
    string(REGEX REPLACE "(..)" "0x\\1, " binary "${binary}")
    file(WRITE "${OUTPUT}"
        "#pragma once\n\n#include <cstddef>\n\nalignas(4) inline constexpr unsigned char ${SYMBOL}[] = { ${binary} };\n")
    return()
endif()

if(NOT GLASS_UI_GLSLC AND WIDGET_GLSLC)
    set(GLASS_UI_GLSLC "${WIDGET_GLSLC}")
endif()
find_program(GLASS_UI_GLSLC
    NAMES glslc glslc.exe
    HINTS "${CMAKE_ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG}"
    NO_CMAKE_FIND_ROOT_PATH
    DOC "Host glslc executable used to build Vulkan widget effects")
if(NOT GLASS_UI_GLSLC OR NOT EXISTS "${GLASS_UI_GLSLC}")
    message(FATAL_ERROR
        "glslc is required to build Vulkan widget effects; set GLASS_UI_GLSLC "
        "(or WIDGET_GLSLC) to the host executable path")
endif()

set(GLASS_UI_GLASS_SHADER_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

function(glass_ui_embed_vulkan_shader name source symbol)
    set(spv "${CMAKE_CURRENT_BINARY_DIR}/${name}.spv")
    set(header "${GLASS_UI_GLASS_SHADER_GENERATED_DIR}/${name}_spv.hpp")
    add_custom_command(
        OUTPUT "${header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${GLASS_UI_GLASS_SHADER_GENERATED_DIR}"
        COMMAND "${GLASS_UI_GLSLC}" --target-env=vulkan1.0 -O
                -I "${CMAKE_CURRENT_SOURCE_DIR}"
                "${source}" -o "${spv}"
        COMMAND "${CMAKE_COMMAND}"
                -DGLASS_UI_EMBED_SHADER_BINARY=ON
                "-DINPUT=${spv}"
                "-DOUTPUT=${header}"
                "-DSYMBOL=${symbol}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        DEPENDS "${source}"
                "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_math.glslinc"
                "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_color.glslinc"
                "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_lighting.glslinc"
                "${CMAKE_CURRENT_SOURCE_DIR}/common/blur_kernel.glslinc"
                "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        VERBATIM
    )
    set(GLASS_UI_VULKAN_SHADER_HEADERS
        ${GLASS_UI_VULKAN_SHADER_HEADERS} "${header}" PARENT_SCOPE)
endfunction()

function(glass_ui_embed_opengl_shader name source symbol)
    set(header "${GLASS_UI_GLASS_SHADER_GENERATED_DIR}/${name}.hpp")
    add_custom_command(
        OUTPUT "${header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${GLASS_UI_GLASS_SHADER_GENERATED_DIR}"
        COMMAND "${CMAKE_COMMAND}"
                -DGLASS_UI_EMBED_SHADER_BINARY=ON
                "-DINPUT=${source}"
                "-DOUTPUT=${header}"
                "-DSYMBOL=${symbol}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        DEPENDS "${source}"
                "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        VERBATIM
    )
    set(GLASS_UI_OPENGL_SHADER_HEADERS
        ${GLASS_UI_OPENGL_SHADER_HEADERS} "${header}" PARENT_SCOPE)
endfunction()

glass_ui_embed_vulkan_shader(
    fullscreen_vert
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/fullscreen.vert"
    kFullscreenVertSpv)
glass_ui_embed_vulkan_shader(
    blur_frag
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/blur.frag"
    kBlurFragSpv)
glass_ui_embed_vulkan_shader(
    liquid_glass_vert
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/liquid_glass.vert"
    kLiquidGlassVertSpv)
glass_ui_embed_vulkan_shader(
    liquid_glass_frag
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/liquid_glass.frag"
    kLiquidGlassFragSpv)

glass_ui_embed_opengl_shader(
    opengl_fullscreen_vert
    "${CMAKE_CURRENT_SOURCE_DIR}/opengl/fullscreen.vert"
    kOpenGLFullscreenVert)
glass_ui_embed_opengl_shader(
    opengl_blur_frag
    "${CMAKE_CURRENT_SOURCE_DIR}/opengl/blur.frag"
    kOpenGLBlurFrag)
glass_ui_embed_opengl_shader(
    opengl_glass_vert
    "${CMAKE_CURRENT_SOURCE_DIR}/opengl/liquid_glass.vert"
    kOpenGLGlassVert)
glass_ui_embed_opengl_shader(
    opengl_glass_frag
    "${CMAKE_CURRENT_SOURCE_DIR}/opengl/liquid_glass.frag"
    kOpenGLGlassFrag)
glass_ui_embed_opengl_shader(
    opengl_blur_kernel
    "${CMAKE_CURRENT_SOURCE_DIR}/common/blur_kernel.glslinc"
    kOpenGLBlurKernel)
glass_ui_embed_opengl_shader(
    opengl_glass_math
    "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_math.glslinc"
    kOpenGLGlassMath)
glass_ui_embed_opengl_shader(
    opengl_glass_color
    "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_color.glslinc"
    kOpenGLGlassColor)
glass_ui_embed_opengl_shader(
    opengl_glass_lighting
    "${CMAKE_CURRENT_SOURCE_DIR}/common/glass_lighting.glslinc"
    kOpenGLGlassLighting)

set(GLASS_UI_GLASS_SHADER_HEADERS
    ${GLASS_UI_VULKAN_SHADER_HEADERS}
    ${GLASS_UI_OPENGL_SHADER_HEADERS})
add_custom_target(glass_ui_glass_shaders
    DEPENDS ${GLASS_UI_GLASS_SHADER_HEADERS})
set(GLASS_UI_GLASS_SHADER_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}"
    PARENT_SCOPE)
