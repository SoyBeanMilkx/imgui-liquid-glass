#include "TextField.hpp"

#include "ui/widget/foundation/layout/LayoutTypes.hpp"

#include <algorithm>
#include <utility>

namespace glass_ui::widget {
namespace {

constexpr uint32_t kFocusChannel = 0x54455854u;
constexpr float kMaximumFocus = 1.25f;
constexpr float kAntialiasMargin = 1.0f;

struct EditCallback {
  TextInputSession &session;
  ImGuiID id;
  std::string &value;
  bool password;
};

int editCallback(ImGuiInputTextCallbackData *data) {
  auto &callback = *static_cast<EditCallback *>(data->UserData);
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    callback.value.resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = callback.value.data();
    return 0;
  }
  callback.session.focus(callback.id, data->Buf, callback.password);
  TextEditState external;
  if (callback.session.consume(callback.id, external)) {
    if (external.text != data->Buf) {
      data->DeleteChars(0, data->BufTextLen);
      data->InsertChars(0, external.text.c_str());
    }
    data->CursorPos = std::clamp(external.cursor, 0, data->BufTextLen);
    data->SelectionStart =
        std::clamp(external.selectionStart, 0, data->BufTextLen);
    data->SelectionEnd = std::clamp(external.selectionEnd, 0, data->BufTextLen);
  }
  TextEditState edit;
  edit.text.assign(data->Buf, static_cast<std::size_t>(data->BufTextLen));
  edit.cursor = data->CursorPos;
  edit.selectionStart = data->SelectionStart;
  edit.selectionEnd = data->SelectionEnd;
  callback.session.publish(callback.id, std::move(edit));
  return 0;
}

void glassAnchor(const ImDrawList *, const ImDrawCmd *) {}

void drawSurface(Context &context, ImGuiID id, const ImVec2 &minimum,
                 const ImVec2 &maximum, float rounding, float focus,
                 const TextFieldOptions &options) {
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const EffectCapabilities &capabilities = context.effectCapabilities();
  GlassStyle style = options.glassStyle;
  style.lighting.rimGlow += std::max(focus, 0.0f) * 0.20f;
  style.lighting.specular += std::max(focus, 0.0f) * 0.12f;
  if (capabilities.backdropCapture &&
      context.glassRequests().requests().size() <
          capabilities.maxGlassRegions) {
    drawList->AddCallback(glassAnchor, nullptr);
    GlassRequest request;
    request.id = id;
    request.min = minimum;
    request.max = maximum;
    request.radii = CornerRadii::all(rounding);
    request.style = style;
    request.clipRect = drawList->_CmdHeader.ClipRect;
    request.drawList = drawList;
    request.drawCommandOffset = drawList->CmdBuffer.Size - 2;
    request.backdropSource = GlassBackdropSource::PreviousContent;
    request.density = context.frame().density;
    request.blurMix = std::clamp(options.glassBlurMix, 0.0f, 1.0f);
    request.opacity =
        ImGui::GetStyle().Alpha * (options.enabled ? 1.0f : 0.55f);
    context.glassRequests().submit(request);
  } else {
    ImVec4 fill = style.tint;
    fill.w = std::max(fill.w, 0.16f);
    drawList->AddRectFilled(minimum, maximum, ImGui::GetColorU32(fill),
                            rounding * context.frame().density);
    drawList->AddRect(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f + focus * 0.12f)),
        rounding * context.frame().density);
  }
}

}

TextFieldResult TextField(Context &context, const char *id, std::string *value,
                          const TextFieldOptions &options) {
  if (!id || !value)
    return {};
  const Theme &theme = context.theme();
  const FrameInfo &frame = context.frame();
  const float density = frame.density;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float width =
      std::max(resolveLayoutExtent(options.size.x, available.x, density), 1.0f);
  // PushFont takes an unscaled base size; measure after ImGui applies DPI.
  ImGui::PushFont(theme.textFont,
                  theme.textFontSize > 0.0f ? theme.textFontSize : 0.0f);
  const float fontSize = ImGui::GetFontSize();
  const float focusSpread = options.focusAnimation
                                ? std::max(theme.textFieldFocusSpread, 0.0f) *
                                      density
                                : 0.0f;
  // The fixed layout includes the spring's maximum outset and edge antialiasing.
  const float visualPadding = focusSpread * kMaximumFocus + kAntialiasMargin;
  const float height = std::max(
      (options.size.y > 0.0f ? options.size.y : theme.textFieldHeight) *
          density,
      fontSize + 8.0f * density + 2.0f * visualPadding);
  const ImVec2 layoutMinimum = ImGui::GetCursorScreenPos();
  const ImVec2 layoutMaximum(layoutMinimum.x + width, layoutMinimum.y + height);
  const ImVec2 padding(std::min(visualPadding, (width - 1.0f) * 0.5f),
                       std::min(visualPadding, (height - 1.0f) * 0.5f));
  const ImVec2 minimum(layoutMinimum.x + padding.x,
                        layoutMinimum.y + padding.y);
  const ImVec2 maximum(layoutMaximum.x - padding.x,
                        layoutMaximum.y - padding.y);
  const ImVec2 editorSize(maximum.x - minimum.x, maximum.y - minimum.y);
  const float iconWidth =
      options.icon ? std::min(40.0f * density, editorSize.x * 0.25f) : 0.0f;
  const float rounding =
      options.rounding >= 0.0f ? options.rounding : theme.textFieldRounding;
  const std::string previous = *value;
  ImGui::PushID(id);
  const ImGuiID inputId = ImGui::GetID("##input");
  if (options.enabled)
    context.gestures().horizontalDrag(
        inputId, layoutMinimum, layoutMaximum,
        HorizontalDragParameters{10.0f * density, 1.2f, 2});
  const bool interactive =
      options.enabled && !context.gestures().tapsSuppressed();
  const bool previouslyFocused = context.textInput().request().id == inputId;
  const SpringTransition motion = context.animations().spring(
      inputId, kFocusChannel, previouslyFocused && interactive ? 1.0f : 0.0f,
      SpringOptions{theme.textFieldFocusResponse, 0.48f, 0.001f},
      frame.deltaTime, frame.frameNumber,
      frame.reduceMotion || !options.focusAnimation);
  const float focus = std::clamp(motion.value, -0.12f, kMaximumFocus);
  const float spread = focusSpread * focus;
  const ImVec2 outset(
      std::clamp(spread, -std::max(editorSize.x - 1.0f, 0.0f) * 0.5f,
                  std::max(padding.x - kAntialiasMargin, 0.0f)),
      std::clamp(spread, -std::max(editorSize.y - 1.0f, 0.0f) * 0.5f,
                  std::max(padding.y - kAntialiasMargin, 0.0f)));
  drawSurface(context, inputId,
              ImVec2(minimum.x - outset.x, minimum.y - outset.y),
              ImVec2(maximum.x + outset.x, maximum.y + outset.y), rounding,
              std::max(focus, 0.0f), options);

  if (options.icon) {
    ImVec4 color = theme.textPrimary;
    color.w *= options.enabled ? 0.75f : 0.4f;
    DrawIcon(context, *options.icon,
             ImVec2(minimum.x + iconWidth * 0.55f,
                     minimum.y + editorSize.y * 0.5f),
             19.0f, color, 1.6f);
  }
  ImGui::SetCursorScreenPos(ImVec2(minimum.x + iconWidth, minimum.y));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(14.0f * density,
                              (editorSize.y - fontSize) * 0.5f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4());
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4());
  ImGui::PushStyleColor(ImGuiCol_Text, theme.textPrimary);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, theme.tabText);
  ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4());
  ImGui::SetNextItemWidth(std::max(editorSize.x - iconWidth, 1.0f));
  const bool clicked = interactive && ImGui::GetIO().MouseClicked[0] &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
      ImGui::IsMouseHoveringRect(layoutMinimum, layoutMaximum);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  if (clicked && (mouse.x < minimum.x + iconWidth || mouse.x >= maximum.x ||
                  mouse.y < minimum.y || mouse.y >= maximum.y))
    ImGui::SetKeyboardFocusHere();
  ImGui::BeginDisabled(!interactive);
  EditCallback callback{context.textInput(), inputId, *value, options.password};
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize |
                              ImGuiInputTextFlags_CallbackAlways |
                              ImGuiInputTextFlags_EnterReturnsTrue;
  if (options.password)
    flags |= ImGuiInputTextFlags_Password;
  const bool submitted = ImGui::InputTextWithHint(
      "##input", options.hint ? options.hint : "", value->data(),
      value->capacity() + 1, flags, editCallback, &callback);
  const bool focused = interactive && ImGui::IsItemActive();
  if (focused) {
    context.trackTextInput();
    if (clicked)
      context.textInput().requestKeyboard();
  }
  ImGui::EndDisabled();
  ImGui::PopStyleColor(6);
  ImGui::PopStyleVar(2);
  ImGui::PopFont();
  ImGui::PopID();
  if (!options.enabled && context.textInput().request().id == inputId)
    context.textInput().close();

  // Reserve fixed layout bounds; the glass spring never moves the editor.
  ImGui::SetCursorScreenPos(layoutMinimum);
  ImGui::Dummy(ImVec2(width, height));
  return TextFieldResult{*value != previous, submitted, focused};
}

}
