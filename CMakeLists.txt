cmake_minimum_required(VERSION 3.15)

if(NOT ANDROID_STL)
    set(ANDROID_STL c++_static)
endif()

project(glass_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")
set(HRIDHI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/hridhi")
set(TOUCHER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/toucher")

function(glass_ui_require_file path description)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${description} was not found at: ${path}")
    endif()
endfunction()

function(glass_ui_import_static target archive)
    glass_ui_require_file("${archive}" "Static library for ${target}")
    add_library(${target} STATIC IMPORTED)
    set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION "${archive}"
    )
    if(ARGN)
        set_target_properties(${target} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ARGN}"
        )
    endif()
endfunction()

glass_ui_require_file("${IMGUI_DIR}/imgui.h" "Dear ImGui header")
glass_ui_require_file("${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
                   "Dear ImGui Vulkan backend header")
glass_ui_require_file("${IMGUI_DIR}/backends/imgui_impl_opengl3.h"
                   "Dear ImGui OpenGL backend header")
glass_ui_require_file("${TOUCHER_DIR}/toucher.h" "Toucher header")

glass_ui_import_static(imgui_core
    "${IMGUI_DIR}/bin/libimgui_core.a"
    "${IMGUI_DIR}")
glass_ui_import_static(imgui_vulkan
    "${IMGUI_DIR}/bin/libimgui_vulkan.a"
    "${IMGUI_DIR}")
glass_ui_import_static(imgui_opengl3
    "${IMGUI_DIR}/bin/libimgui_opengl3.a"
    "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
glass_ui_import_static(hridhi
    "${HRIDHI_DIR}/bin/libhridhi.a"
    "${HRIDHI_DIR}")
glass_ui_import_static(toucher
    "${TOUCHER_DIR}/bin/libtoucher.a"
    "${TOUCHER_DIR}")

add_subdirectory(src/ui/widget/effects/glass/shaders)
add_subdirectory(src/ui/widget/foundation/typography/fonts)
add_subdirectory(src/ui/input/java)

add_library(glass_ui SHARED
    src/OverlayRuntime.cpp
    src/core/hooks/ElfLoadMonitor.cpp
    src/core/graphics/GraphicsBootstrap.cpp
    src/core/graphics/opengl/EglHooks.cpp
    src/core/graphics/vulkan/VulkanHooks.cpp
    src/main.cpp
    src/ui/input/AndroidIme.cpp
    src/ui/input/OverlayInput.cpp
    src/ui/input/java/BridgeClass.cpp
    src/ui/input/touch/InputBridge.cpp
    src/ui/input/touch/TouchBridge.cpp
    src/ui/input/touch/TouchQueue.cpp
    src/ui/input/WindowFocusInfo.cpp
    src/ui/page/HomePage.cpp
    src/ui/page/MessagesPage.cpp
    src/ui/page/NotificationsPage.cpp
    src/ui/page/SearchPage.cpp
    src/ui/page/SettingsPage.cpp
    src/ui/renderer/DisplayInfo.cpp
    src/ui/renderer/OverlayFrameRuntime.cpp
    src/ui/renderer/opengl/ImGuiOpenGLRenderer.cpp
    src/ui/renderer/opengl/OpenGLRenderer.cpp
    src/ui/renderer/opengl/OpenGLStateGuard.cpp
    src/ui/renderer/opengl/OpenGLSurfaceState.cpp
    src/ui/renderer/vulkan/ImGuiVulkanRenderer.cpp
    src/ui/renderer/vulkan/SurfaceTransform.cpp
    src/ui/renderer/vulkan/SwapchainRenderer.cpp
    src/ui/renderer/vulkan/VulkanRenderer.cpp
    src/ui/widget/components/containers/GlassWindow.cpp
    src/ui/widget/components/containers/LayoutViewport.cpp
    src/ui/widget/components/containers/ScrollView.cpp
    src/ui/widget/components/containers/SwipePager.cpp
    src/ui/widget/components/containers/navigation/GlassNavigationLayout.cpp
    src/ui/widget/components/containers/navigation/NavigationIndicator.cpp
    src/ui/widget/components/containers/navigation/NavigationItem.cpp
    src/ui/widget/components/containers/navigation/NavigationRail.cpp
    src/ui/widget/components/controls/Button.cpp
    src/ui/widget/components/controls/Icon.cpp
    src/ui/widget/components/controls/ResizeHandle.cpp
    src/ui/widget/components/controls/Slider.cpp
    src/ui/widget/components/controls/switch/Switch.cpp
    src/ui/widget/components/controls/switch/SwitchRow.cpp
    src/ui/widget/components/controls/TabBar.cpp
    src/ui/widget/components/controls/Text.cpp
    src/ui/widget/components/controls/TextField.cpp
    src/ui/widget/effects/glass/backend/vulkan/VulkanGlassBackend.cpp
    src/ui/widget/effects/glass/backend/vulkan/VulkanGlassBlurPass.cpp
    src/ui/widget/effects/glass/backend/vulkan/VulkanGlassDrawCallbacks.cpp
    src/ui/widget/effects/glass/backend/vulkan/VulkanGlassGeometry.cpp
    src/ui/widget/effects/glass/backend/vulkan/VulkanGlassResources.cpp
    src/ui/widget/effects/glass/backend/opengl/OpenGLGlassBackend.cpp
    src/ui/widget/effects/glass/backend/opengl/OpenGLGlassBlurPass.cpp
    src/ui/widget/effects/glass/backend/opengl/OpenGLGlassDrawCallbacks.cpp
    src/ui/widget/effects/glass/backend/opengl/OpenGLGlassResources.cpp
    src/ui/widget/effects/glass/GlassBlurPolicy.cpp
    src/ui/widget/effects/glass/GlassRequestQueue.cpp
    src/ui/widget/foundation/animation/AnimationStore.cpp
    src/ui/widget/foundation/animation/FadeThroughTransition.cpp
    src/ui/widget/foundation/animation/FluidMotion.cpp
    src/ui/widget/foundation/animation/InteractiveScaleMotion.cpp
    src/ui/widget/foundation/animation/SpringMotion.cpp
    src/ui/widget/foundation/drawing/DrawTransform.cpp
    src/ui/widget/foundation/layout/BoxLayout.cpp
    src/ui/widget/foundation/layout/LinearLayout.cpp
    src/ui/widget/foundation/interaction/ItemBehavior.cpp
    src/ui/widget/foundation/interaction/GestureArena.cpp
    src/ui/widget/foundation/interaction/InputSpace.cpp
    src/ui/widget/foundation/interaction/ScrollStateStore.cpp
    src/ui/widget/foundation/interaction/TextInputSession.cpp
    src/ui/widget/foundation/typography/Font.cpp
    src/ui/widget/runtime/Context.cpp
    src/ui/OverlayUi.cpp
    src/ui/OverlayPanel.cpp
    src/utils/ART.cpp
    src/utils/LogUtils.cpp
)
add_dependencies(glass_ui glass_ui_glass_shaders)
target_include_directories(glass_ui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GLASS_UI_GLASS_SHADER_INCLUDE_DIR}"
    "${GLASS_UI_FONT_INCLUDE_DIR}"
    "${GLASS_UI_INPUT_JAVA_INCLUDE_DIR}"
)
target_compile_definitions(glass_ui PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
target_compile_options(glass_ui PRIVATE -Wall -Wextra -Werror)
target_link_options(glass_ui PRIVATE -Wl,--exclude-libs,ALL)
target_link_libraries(glass_ui PRIVATE
    imgui_vulkan
    imgui_opengl3
    imgui_core
    hridhi
    toucher
    vulkan
    EGL
    GLESv3
    dl
    log
)

add_custom_command(TARGET glass_ui POST_BUILD
    COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:glass_ui>"
)
