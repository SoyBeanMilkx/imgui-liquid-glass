#include "ImGuiOpenGLRenderer.hpp"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "ui/renderer/DisplayInfo.hpp"
#include "utils/LogUtils.hpp"

namespace glass_ui::renderer::opengl {

void ImGuiOpenGLRenderer::initialize(InputBridge &input) noexcept {
  display::initializeDisplayInfo();
  runtime_.initialize(input);
}

bool ImGuiOpenGLRenderer::ensure(EGLContext context) {
  if (ready_ && context_ == context)
    return true;
  if (ready_)
    shutdown();
  if (context == EGL_NO_CONTEXT || !runtime_.ensureContext())
    return false;
  if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
    log::error("ImGui_ImplOpenGL3_Init failed");
    runtime_.shutdownContext();
    return false;
  }
  context_ = context;
  ready_ = true;
  log::info("imgui opengl-es backend ready");
  return true;
}

ImDrawData *ImGuiOpenGLRenderer::draw(const OpenGLSurfaceState &surface) {
  const display::DisplayInfo displayInfo = display::queryDisplayInfo();
  runtime_.prepareFrame(RenderSurfaceInfo{
      surface.width, surface.height, display::inputTransform(displayInfo)});
  ImGui_ImplOpenGL3_NewFrame();
  return runtime_.buildFrame();
}

void ImGuiOpenGLRenderer::render(ImDrawData *drawData) {
  if (drawData)
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

void ImGuiOpenGLRenderer::commitInput() noexcept { runtime_.commitInput(); }

void ImGuiOpenGLRenderer::suspendInput() noexcept { runtime_.suspendInput(); }

void ImGuiOpenGLRenderer::shutdown() {
  suspendInput();
  if (!ready_)
    return;
  ImGui_ImplOpenGL3_Shutdown();
  runtime_.shutdownContext();
  context_ = EGL_NO_CONTEXT;
  ready_ = false;
}

}
