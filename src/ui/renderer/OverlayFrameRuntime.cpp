#include "OverlayFrameRuntime.hpp"

#include "imgui.h"

#include <algorithm>

namespace glass_ui::renderer {

void OverlayFrameRuntime::initialize(InputBridge &input) noexcept {
  input_.initialize(input);
}

bool OverlayFrameRuntime::ensureContext() {
  if (ready_)
    return true;
  IMGUI_CHECKVERSION();
  if (!ImGui::CreateContext())
    return false;
  ImGuiIO &io = ImGui::GetIO();
  input_.configureImGui(io);
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
  ui_.applyStyle();
  lastFrame_ = std::chrono::steady_clock::now();
  ready_ = true;
  return true;
}

void OverlayFrameRuntime::prepareFrame(const RenderSurfaceInfo &surface) {
  surface_ = surface;
  const auto now = std::chrono::steady_clock::now();
  const float delta = std::chrono::duration<float>(now - lastFrame_).count();
  lastFrame_ = now;

  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(surface.width),
                          static_cast<float>(surface.height));
  io.DeltaTime = std::max(delta, 1.0f / 1000.0f);
  input_.poll(ui_.textInput(), io,
              InputSurface{surface.width, surface.height, surface.inputTransform});

  ui_.prepareFrame(OverlayUiFrame{static_cast<float>(surface.width),
                                  static_cast<float>(surface.height),
                                  input_.events()});
}

ImDrawData *OverlayFrameRuntime::buildFrame() {
  ImGui::NewFrame();
  ui_.draw();
  ImGui::Render();
  ImDrawData *drawData = ImGui::GetDrawData();
  if (drawData)
    ui_.prepareDrawData(*drawData);
  return drawData;
}

void OverlayFrameRuntime::commitInput() noexcept {
  input_.commit(ui_.textInput(), ImGui::GetIO().DisplaySize, ui_.inputRegions(),
                ui_.textInputPosition(input_.imeCursorPosition()));
}

void OverlayFrameRuntime::suspendInput() noexcept {
  input_.suspend(ui_.textInput());
}

void OverlayFrameRuntime::shutdownContext() {
  suspendInput();
  if (!ready_)
    return;
  if (ImGui::GetCurrentContext()) {
    input_.unconfigureImGui();
    ImGui::DestroyContext();
  }
  ready_ = false;
  ui_.resetLayout();
}

}
