#include "Icon.hpp"

#include "imgui.h"

#include <algorithm>

namespace glass_ui::widget {
namespace {

void drawGrid(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
              float thickness) {
  const float cell = size * 0.26f;
  const float offset = size * 0.22f;
  for (int row = -1; row <= 1; row += 2) {
    for (int column = -1; column <= 1; column += 2) {
      const ImVec2 minimum(center.x + column * offset - cell * 0.5f,
                           center.y + row * offset - cell * 0.5f);
      drawList->AddRect(minimum, ImVec2(minimum.x + cell, minimum.y + cell),
                        color, cell * 0.18f, 0, thickness);
    }
  }
}

void drawSearch(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
                float thickness) {
  const float radius = size * 0.25f;
  const ImVec2 lens(center.x - size * 0.08f, center.y - size * 0.08f);
  drawList->AddCircle(lens, radius, color, 20, thickness);
  drawList->AddLine(ImVec2(lens.x + radius * 0.72f, lens.y + radius * 0.72f),
                    ImVec2(center.x + size * 0.36f, center.y + size * 0.36f),
                    color, thickness);
}

void drawBell(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
              float thickness) {
  const float half = size * 0.28f;
  drawList->PathLineTo(ImVec2(center.x - half, center.y + half * 0.55f));
  drawList->PathBezierQuadraticCurveTo(
      ImVec2(center.x - half * 0.8f, center.y - half),
      ImVec2(center.x, center.y - half));
  drawList->PathBezierQuadraticCurveTo(
      ImVec2(center.x + half * 0.8f, center.y - half),
      ImVec2(center.x + half, center.y + half * 0.55f));
  drawList->PathStroke(color, ImDrawFlags_None, thickness);
  drawList->AddLine(ImVec2(center.x - half * 1.1f, center.y + half * 0.55f),
                    ImVec2(center.x + half * 1.1f, center.y + half * 0.55f),
                    color, thickness);
  drawList->AddCircleFilled(ImVec2(center.x, center.y + half * 0.82f),
                            thickness * 0.72f, color, 12);
}

void drawChat(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
              float thickness) {
  const ImVec2 minimum(center.x - size * 0.32f, center.y - size * 0.24f);
  const ImVec2 maximum(center.x + size * 0.32f, center.y + size * 0.22f);
  drawList->AddRect(minimum, maximum, color, size * 0.10f, 0, thickness);
  drawList->AddLine(ImVec2(center.x - size * 0.12f, maximum.y),
                    ImVec2(center.x - size * 0.22f, center.y + size * 0.36f),
                    color, thickness);
  drawList->AddLine(ImVec2(center.x - size * 0.22f, center.y + size * 0.36f),
                    ImVec2(center.x + size * 0.02f, maximum.y), color,
                    thickness);
}

void drawSettings(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
                  float thickness) {
  constexpr float positions[3] = {-0.25f, 0.0f, 0.25f};
  constexpr float knobs[3] = {-0.12f, 0.16f, -0.02f};
  for (int index = 0; index < 3; ++index) {
    const float y = center.y + size * positions[index];
    drawList->AddLine(ImVec2(center.x - size * 0.34f, y),
                      ImVec2(center.x + size * 0.34f, y), color, thickness);
    drawList->AddCircleFilled(ImVec2(center.x + size * knobs[index], y),
                              thickness * 1.25f, color, 12);
  }
}

void drawSparkle(ImDrawList *drawList, ImVec2 center, float radius,
                 ImU32 color, float thickness) {
  const float inner = radius * 0.24f;
  drawList->PathLineTo(ImVec2(center.x, center.y - radius));
  drawList->PathLineTo(ImVec2(center.x + inner, center.y - inner));
  drawList->PathLineTo(ImVec2(center.x + radius, center.y));
  drawList->PathLineTo(ImVec2(center.x + inner, center.y + inner));
  drawList->PathLineTo(ImVec2(center.x, center.y + radius));
  drawList->PathLineTo(ImVec2(center.x - inner, center.y + inner));
  drawList->PathLineTo(ImVec2(center.x - radius, center.y));
  drawList->PathLineTo(ImVec2(center.x - inner, center.y - inner));
  drawList->PathLineTo(ImVec2(center.x, center.y - radius));
  drawList->PathStroke(color, ImDrawFlags_None, thickness);
}

void drawSparkles(ImDrawList *drawList, ImVec2 center, float size, ImU32 color,
                  float thickness) {
  drawSparkle(drawList, ImVec2(center.x - size * 0.08f,
                               center.y + size * 0.06f),
              size * 0.34f, color, thickness);
  drawSparkle(drawList, ImVec2(center.x + size * 0.28f,
                               center.y - size * 0.27f),
              size * 0.14f, color, std::max(thickness * 0.78f, 1.0f));
}

}

void DrawIcon(const Context &context, IconGlyph glyph, ImVec2 center,
              float size, ImVec4 color, float thickness) {
  const float density = context.frame().density;
  const float physicalSize = std::max(size * density, 1.0f);
  const float physicalThickness = std::max(thickness * density, 1.0f);
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const ImU32 packedColor = ImGui::GetColorU32(color);
  switch (glyph) {
  case IconGlyph::Grid:
    drawGrid(drawList, center, physicalSize, packedColor, physicalThickness);
    break;
  case IconGlyph::Search:
    drawSearch(drawList, center, physicalSize, packedColor,
               physicalThickness);
    break;
  case IconGlyph::Bell:
    drawBell(drawList, center, physicalSize, packedColor, physicalThickness);
    break;
  case IconGlyph::Chat:
    drawChat(drawList, center, physicalSize, packedColor, physicalThickness);
    break;
  case IconGlyph::Settings:
    drawSettings(drawList, center, physicalSize, packedColor,
                 physicalThickness);
    break;
  case IconGlyph::Sparkles:
    drawSparkles(drawList, center, physicalSize, packedColor,
                 physicalThickness);
    break;
  }
}

}
