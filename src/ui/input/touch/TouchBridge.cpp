#include "TouchBridge.hpp"

#include "TouchQueue.hpp"
#include "ui/input/WindowFocusInfo.hpp"
#include "ui/input/java/BridgeClass.hpp"
#include "utils/ART.hpp"
#include "utils/LogUtils.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace glass_ui {
namespace {

struct Mailbox {
  std::mutex mutex;
  TouchQueue touches;
  uint64_t generation = 0;
  bool unavailable = false;
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

void JNICALL onPointer(JNIEnv *, jclass, jlong owner, jlong generation,
                       jint phase, jfloat x, jfloat y, jlong timestampNs) {
  if (phase < 0 || phase > static_cast<jint>(PointerPhase::Cancel))
    return;
  if (const auto target = mailbox(owner)) {
    std::lock_guard<std::mutex> lock(target->mutex);
    if (target->generation &&
        target->generation == static_cast<uint64_t>(generation))
      target->touches.push(PointerEvent{static_cast<PointerPhase>(phase),
                                       ImVec2(x, y),
                                       static_cast<uint64_t>(timestampNs)});
  }
}

void JNICALL onUnavailable(JNIEnv *, jclass, jlong owner, jlong generation) {
  if (const auto target = mailbox(owner)) {
    std::lock_guard<std::mutex> lock(target->mutex);
    if (target->generation &&
        target->generation == static_cast<uint64_t>(generation))
      target->unavailable = true;
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
    jclass candidate = input::java::loadBridgeClass(env, "glass_ui.overlay.TouchBridge");
    if (!candidate)
      return false;
    const JNINativeMethod methods[] = {
        {"onPointer", "(JJIFFJ)V", reinterpret_cast<void *>(onPointer)},
        {"onUnavailable", "(JJ)V", reinterpret_cast<void *>(onUnavailable)},
        {"findActivity", "()Landroid/app/Activity;",
         reinterpret_cast<void *>(findActivity)},
    };
    const int registered = env->RegisterNatives(candidate, methods, 3);
    if (art::clearJavaException(env) || registered != JNI_OK)
      return false;
    constructor = env->GetMethodID(candidate, "<init>", "(J)V");
    update = env->GetMethodID(candidate, "update", "(J[F)V");
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

struct TouchBridge::State {
  std::shared_ptr<Mailbox> mailbox = std::make_shared<Mailbox>();
  jobject bridge = nullptr;
  uint64_t owner = 0, generation = 0, nextGeneration = 0;
  std::array<jfloat, InputRegionSet::Capacity * 5> regions{};
  std::size_t regionSize = 0;
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
      log::warn("Android touch Java bridge unavailable");
      return false;
    }
    log::info("Android touch Java bridge ready");
    return true;
  }
};

TouchBridge::TouchBridge() : state_(std::make_unique<State>()) {}
TouchBridge::~TouchBridge() = default;

bool TouchBridge::poll(ImGuiIO &io, PointerEventBatch &events) noexcept {
  std::lock_guard<std::mutex> lock(state_->mailbox->mutex);
  if (state_->mailbox->unavailable) {
    state_->mailbox->touches.clear();
    io.ClearInputMouse();
    events.clear();
    events.push(PointerEvent{PointerPhase::Cancel, {}, 0});
    return false;
  }
  state_->mailbox->touches.poll(io, events);
  return true;
}

bool TouchBridge::commit(bool enabled, const InputRegionSet &regions,
                        ImVec2 displaySize) {
  State &state = *state_;
  if (!enabled && !state.generation)
    return true;
  std::array<jfloat, InputRegionSet::Capacity * 5> normalized{};
  std::size_t regionSize = 0;
  if (enabled && displaySize.x > 0.0f && displaySize.y > 0.0f) {
    for (std::size_t index = 0; index < regions.size(); ++index) {
      const InputRegion &region = regions.data()[index];
      normalized[regionSize++] = region.minimum.x / displaySize.x;
      normalized[regionSize++] = region.minimum.y / displaySize.y;
      normalized[regionSize++] = region.maximum.x / displaySize.x;
      normalized[regionSize++] = region.maximum.y / displaySize.y;
      normalized[regionSize++] = static_cast<jfloat>(region.shape);
    }
  }
  if (enabled == (state.generation != 0) && state.regionSize == regionSize &&
      std::equal(normalized.begin(), normalized.begin() + regionSize,
                 state.regions.begin()))
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
    jfloatArray values = env->NewFloatArray(regionSize);
    if (!art::clearJavaException(env) && values) {
      env->SetFloatArrayRegion(values, 0, regionSize, normalized.data());
      if (!art::clearJavaException(env)) {
        const uint64_t generation =
            enabled ? (state.generation ? state.generation : ++state.nextGeneration) : 0;
        {
          std::lock_guard<std::mutex> lock(state.mailbox->mutex);
          if (state.mailbox->generation != generation) {
            state.mailbox->touches.clear();
            state.mailbox->unavailable = false;
          }
          state.mailbox->generation = generation;
        }
        env->CallVoidMethod(state.bridge, javaApi().update,
                            static_cast<jlong>(generation), values);
        if (!art::clearJavaException(env)) {
          state.generation = generation;
          state.regions = normalized;
          state.regionSize = regionSize;
          sent = true;
        } else {
          std::lock_guard<std::mutex> lock(state.mailbox->mutex);
          state.mailbox->generation = state.generation;
        }
      }
    }
  }
  art::clearJavaException(env);
  env->PopLocalFrame(nullptr);
  return sent;
}

void TouchBridge::clear() noexcept {
  std::lock_guard<std::mutex> lock(state_->mailbox->mutex);
  state_->mailbox->touches.clear();
}

void TouchBridge::suspend() noexcept {
  commit(false, {}, {});
  std::lock_guard<std::mutex> lock(state_->mailbox->mutex);
  state_->mailbox->generation = 0;
  state_->mailbox->touches.clear();
  state_->mailbox->unavailable = false;
}

}
