#include "OpenGLGlassDrawCallbacks.hpp"

#include <algorithm>
#include <iterator>

namespace glass_ui::widget {

OpenGLGlassDrawCallbacks::~OpenGLGlassDrawCallbacks() { clear(); }

bool OpenGLGlassDrawCallbacks::belongsTo(const ImDrawData &drawData,
                                         const ImDrawList *drawList) noexcept {
  for (int index = 0; index < drawData.CmdListsCount; ++index) {
    if (drawData.CmdLists[index] == drawList)
      return true;
  }
  return false;
}

void OpenGLGlassDrawCallbacks::render(const ImDrawList *,
                                      const ImDrawCmd *command) {
  auto *data = static_cast<CallbackData *>(command->UserCallbackData);
  if (data && data->backend && data->request &&
      data->backend->prepareRequestBackdrop(
          data->request->backdropSource))
    data->backend->composite(*data->request);
}

void OpenGLGlassDrawCallbacks::inject(ImDrawData &drawData,
                                      const GlassRequestQueue &queue,
                                      OpenGLGlassBackend &backend) {
  clear();
  const std::vector<GlassRequest> &requests = queue.requests();
  const size_t requestCount = std::min<size_t>(
      requests.size(), backend.capabilities().maxGlassRegions);
  callbackData_.resize(requestCount);
  injections_.reserve(requestCount);

  for (size_t index = 0; index < requestCount; ++index) {
    const GlassRequest &request = requests[index];
    if (!request.drawList || !belongsTo(drawData, request.drawList))
      continue;
    callbackData_[index] = CallbackData{&backend, &request};
    auto injection = std::find_if(injections_.begin(), injections_.end(),
                                  [&request](const DrawListInjection &value) {
                                    return value.drawList == request.drawList;
                                  });
    if (injection == injections_.end()) {
      injections_.push_back(DrawListInjection{request.drawList, {}});
      injection = std::prev(injections_.end());
    }

    ImDrawCmd glassCommand;
    glassCommand.ClipRect = request.clipRect;
    glassCommand.UserCallback = render;
    glassCommand.UserCallbackData = &callbackData_[index];
    glassCommand.UserCallbackDataOffset = -1;
    const int offset = std::clamp(request.drawCommandOffset, 0,
                                  request.drawList->CmdBuffer.Size);
    const int preceding = static_cast<int>(std::count_if(
        injection->commandOffsets.begin(), injection->commandOffsets.end(),
        [offset](int value) { return value <= offset; }));
    const int insertion = offset + preceding * 2;
    request.drawList->CmdBuffer.insert(request.drawList->CmdBuffer.begin() +
                                           insertion,
                                       glassCommand);

    ImDrawCmd resetCommand;
    resetCommand.ClipRect = request.clipRect;
    resetCommand.UserCallback =
        ImGui::GetPlatformIO().DrawCallback_ResetRenderState;
    resetCommand.UserCallbackDataOffset = -1;
    request.drawList->CmdBuffer.insert(request.drawList->CmdBuffer.begin() +
                                           insertion + 1,
                                       resetCommand);
    injection->commandOffsets.push_back(offset);
  }
}

void OpenGLGlassDrawCallbacks::clear() {
  for (const DrawListInjection &injection : injections_) {
    if (!injection.drawList)
      continue;
    for (int index = injection.drawList->CmdBuffer.Size - 1; index >= 0;
         --index) {
      if (injection.drawList->CmdBuffer[index].UserCallback != render)
        continue;
      const int end = std::min(index + 2, injection.drawList->CmdBuffer.Size);
      injection.drawList->CmdBuffer.erase(
          injection.drawList->CmdBuffer.begin() + index,
          injection.drawList->CmdBuffer.begin() + end);
    }
  }
  injections_.clear();
  callbackData_.clear();
}

}
