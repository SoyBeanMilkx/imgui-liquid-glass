#include "AndroidIme.hpp"

#include "java/BridgeClass.hpp"
#include "ui/input/WindowFocusInfo.hpp"
#include "ui/widget/foundation/interaction/TextInputSession.hpp"
#include "utils/ART.hpp"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace glass_ui {
namespace {

struct Mailbox {
  std::mutex mutex;
  std::optional<widget::TextInputUpdate> update;
  ImVec4 visibleArea{0.0f, 0.0f, 1.0f, 1.0f};
};

struct Registry {
  std::mutex mutex;
  uint64_t nextOwner = 0;
  std::unordered_map<uint64_t, std::weak_ptr<Mailbox>> owners;
};

Registry &registry() {
  static auto *value = new Registry;
  return *value;
}

std::shared_ptr<Mailbox> mailbox(jlong owner) {
  auto &entries = registry();
  std::lock_guard<std::mutex> lock(entries.mutex);
  const auto found = entries.owners.find(static_cast<uint64_t>(owner));
  return found != entries.owners.end() ? found->second.lock() : nullptr;
}

void JNICALL onEdit(JNIEnv *env, jclass, jlong owner, jlong token,
                    jlong revision, jlong version, jbyteArray bytes, jint start,
                    jint end, jint composingStart, jint composingEnd,
                    jboolean finished) {
  const auto target = mailbox(owner);
  if (!target || !bytes)
    return;
  widget::TextInputUpdate update;
  update.token = static_cast<uint64_t>(token);
  update.revision = static_cast<uint64_t>(revision);
  update.version = static_cast<uint64_t>(version);
  update.edit.text.resize(static_cast<std::size_t>(env->GetArrayLength(bytes)));
  env->GetByteArrayRegion(bytes, 0, update.edit.text.size(),
                          reinterpret_cast<jbyte *>(update.edit.text.data()));
  if (art::clearJavaException(env))
    return;
  update.edit.cursor = end;
  update.edit.selectionStart = start;
  update.edit.selectionEnd = end;
  update.edit.composingStart = composingStart;
  update.edit.composingEnd = composingEnd;
  update.finished = finished == JNI_TRUE;
  std::lock_guard<std::mutex> lock(target->mutex);
  target->update = std::move(update);
}

void JNICALL onVisibleArea(JNIEnv *, jclass, jlong owner, jfloat left,
                           jfloat top, jfloat right, jfloat bottom) {
  if (const auto target = mailbox(owner)) {
    std::lock_guard<std::mutex> lock(target->mutex);
    target->visibleArea = ImVec4(left, top, right, bottom);
  }
}

jobject JNICALL findActivity(JNIEnv *env, jclass) {
  return window::focusedActivity(env);
}

struct JavaApi {
  std::mutex mutex;
  jclass clazz = nullptr;
  jmethodID constructor = nullptr, update = nullptr, close = nullptr;

  bool bind(JNIEnv *env) {
    std::lock_guard<std::mutex> lock(mutex);
    if (clazz)
      return true;
    jclass candidate = input::java::loadBridgeClass(env, "glass_ui.overlay.ImeBridge");
    if (!candidate)
      return false;
    const JNINativeMethod methods[] = {
        {"onEdit", "(JJJJ[BIIIIZ)V", reinterpret_cast<void *>(onEdit)},
        {"onVisibleArea", "(JFFFF)V", reinterpret_cast<void *>(onVisibleArea)},
        {"findActivity", "()Landroid/app/Activity;",
         reinterpret_cast<void *>(findActivity)},
    };
    const int registered = env->RegisterNatives(candidate, methods, 3);
    if (art::clearJavaException(env) || registered != JNI_OK)
      return false;
    constructor = env->GetMethodID(candidate, "<init>", "(J)V");
    update = env->GetMethodID(candidate, "update", "(JJJJ[BIIZFF)V");
    close = env->GetMethodID(candidate, "close", "()V");
    if (art::clearJavaException(env) || !constructor || !update || !close)
      return false;
    clazz = static_cast<jclass>(env->NewGlobalRef(candidate));
    return !art::clearJavaException(env) && clazz;
  }
};

JavaApi &javaApi() {
  static auto *api = new JavaApi;
  return *api;
}

}

struct AndroidIme::State {
  std::shared_ptr<Mailbox> mailbox = std::make_shared<Mailbox>();
  jobject bridge = nullptr;
  uint64_t owner = 0, token = 0, revision = 0, version = 0;
  uint64_t keyboardRequest = 0;
  ImVec2 cursor{-1.0f, -1.0f};
  std::chrono::steady_clock::time_point nextAttempt{};

  State() {
    auto &entries = registry();
    std::lock_guard<std::mutex> lock(entries.mutex);
    owner = ++entries.nextOwner;
    entries.owners.emplace(owner, mailbox);
  }

  ~State() {
    {
      auto &entries = registry();
      std::lock_guard<std::mutex> lock(entries.mutex);
      entries.owners.erase(owner);
    }
    art::ScopedJniEnv scoped(art::javaVm());
    if (JNIEnv *env = scoped.get(); env && bridge) {
      env->CallVoidMethod(bridge, javaApi().close);
      art::clearJavaException(env);
      env->DeleteGlobalRef(bridge);
    }
  }

  bool ensure(JNIEnv *env) {
    if (bridge)
      return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < nextAttempt)
      return false;
    nextAttempt = now + std::chrono::seconds(1);
    JavaApi &api = javaApi();
    if (api.bind(env)) {
      jobject candidate =
          env->NewObject(api.clazz, api.constructor, static_cast<jlong>(owner));
      if (!art::clearJavaException(env) && candidate)
        bridge = env->NewGlobalRef(candidate);
    }
    if (art::clearJavaException(env) || !bridge) {
      log::warn("Android IME Java bridge unavailable");
      return false;
    }
    log::info("Android IME Java bridge ready");
    return true;
  }
};

AndroidIme::AndroidIme() : state_(std::make_unique<State>()) {}
AndroidIme::~AndroidIme() = default;

void AndroidIme::configureImGui() {
  auto &platform = ImGui::GetPlatformIO();
  platform.Platform_ImeUserData = this;
  platform.Platform_SetImeDataFn = [](ImGuiContext *, ImGuiViewport *,
                                      ImGuiPlatformImeData *data) {
    auto *ime =
        static_cast<AndroidIme *>(ImGui::GetPlatformIO().Platform_ImeUserData);
    if (ime)
      ime->cursor_ = data->InputPos;
  };
}

void AndroidIme::unconfigureImGui() noexcept {
  if (!ImGui::GetCurrentContext())
    return;
  auto &platform = ImGui::GetPlatformIO();
  if (platform.Platform_ImeUserData == this) {
    platform.Platform_ImeUserData = nullptr;
    platform.Platform_SetImeDataFn = nullptr;
  }
}

void AndroidIme::poll(widget::TextInputSession &session, ImGuiIO &io) {
  if (releaseEnter_) {
    io.AddKeyEvent(ImGuiKey_Enter, false);
    releaseEnter_ = false;
  }
  std::optional<widget::TextInputUpdate> update;
  {
    std::lock_guard<std::mutex> lock(state_->mailbox->mutex);
    update.swap(state_->mailbox->update);
  }
  if (update) {
    const bool finished = update->finished;
    if (session.apply(std::move(*update)) && finished) {
      io.AddKeyEvent(ImGuiKey_Enter, true);
      releaseEnter_ = true;
    }
  }
}

bool AndroidIme::commit(const widget::TextInputRequest &request,
                        ImVec2 displaySize, ImVec2 cursorPosition) {
  State &state = *state_;
  if (!request.token && !state.token)
    return true;
  const ImVec2 cursor(
      displaySize.x > 0.0f ? std::clamp(cursorPosition.x / displaySize.x, 0.0f, 1.0f)
                           : 0.0f,
      displaySize.y > 0.0f ? std::clamp(cursorPosition.y / displaySize.y, 0.0f, 1.0f)
                           : 0.0f);
  if (state.token == request.token && state.revision == request.revision &&
      state.version == request.platformVersion && state.cursor.x == cursor.x &&
      state.keyboardRequest == request.keyboardRequest &&
      state.cursor.y == cursor.y)
    return true;
  art::ScopedJniEnv scoped(art::javaVm());
  JNIEnv *env = scoped.get();
  if (!env)
    return false;
  if (env->PushLocalFrame(24) != JNI_OK) {
    art::clearJavaException(env);
    return false;
  }
  bool sent = false;
  if (state.ensure(env)) {
    jbyteArray bytes = env->NewByteArray(request.edit.text.size());
    if (!art::clearJavaException(env) && bytes) {
      env->SetByteArrayRegion(
          bytes, 0, request.edit.text.size(),
          reinterpret_cast<const jbyte *>(request.edit.text.data()));
      if (!art::clearJavaException(env)) {
        env->CallVoidMethod(
            state.bridge, javaApi().update, static_cast<jlong>(request.token),
            static_cast<jlong>(request.revision),
            static_cast<jlong>(request.platformVersion),
            static_cast<jlong>(request.keyboardRequest), bytes,
            request.edit.selectionStart, request.edit.selectionEnd,
            request.password ? JNI_TRUE : JNI_FALSE, cursor.x, cursor.y);
        if (!art::clearJavaException(env)) {
          state.token = request.token;
          state.revision = request.revision;
          state.version = request.platformVersion;
          state.keyboardRequest = request.keyboardRequest;
          state.cursor = cursor;
          sent = true;
        }
      }
    }
  }
  art::clearJavaException(env);
  env->PopLocalFrame(nullptr);
  return sent;
}

void AndroidIme::suspend() noexcept { commit({}, {}, {}); }

ImVec4 AndroidIme::visibleArea() const noexcept {
  std::lock_guard<std::mutex> lock(state_->mailbox->mutex);
  return state_->mailbox->visibleArea;
}

}
