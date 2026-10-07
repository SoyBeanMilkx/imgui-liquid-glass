#include "DisplayInfo.hpp"

#include "toucher.h"
#include "utils/ART.hpp"
#include "utils/LogUtils.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>

namespace glass_ui::display {
namespace {

class JavaDisplaySource final {
public:
  bool query(DisplayInfo &info) noexcept;

private:
  bool ensureBindingsLocked(JNIEnv *env) noexcept;

  std::mutex mutex_;
  jobject display_ = nullptr;
  jclass pointClass_ = nullptr;
  jmethodID getRotation_ = nullptr;
  jmethodID getRealSize_ = nullptr;
  jmethodID pointConstructor_ = nullptr;
  jfieldID pointX_ = nullptr;
  jfieldID pointY_ = nullptr;
  bool bindingsLogged_ = false;
};

bool JavaDisplaySource::ensureBindingsLocked(JNIEnv *env) noexcept {
  if (display_ && pointClass_ && getRotation_ && getRealSize_ &&
      pointConstructor_ && pointX_ && pointY_)
    return true;

  jobject application = art::currentApplication(env);
  if (!application)
    return false;
  jclass contextClass = env->FindClass("android/content/Context");
  if (!contextClass || art::clearJavaException(env))
    return false;
  jmethodID getSystemService =
      env->GetMethodID(contextClass, "getSystemService",
                       "(Ljava/lang/String;)Ljava/lang/Object;");
  if (!getSystemService || art::clearJavaException(env))
    return false;
  jstring serviceName = env->NewStringUTF("window");
  if (!serviceName || art::clearJavaException(env))
    return false;
  jobject windowManager =
      env->CallObjectMethod(application, getSystemService, serviceName);
  if (!windowManager || art::clearJavaException(env))
    return false;
  jclass windowManagerClass = env->GetObjectClass(windowManager);
  if (!windowManagerClass || art::clearJavaException(env))
    return false;
  jmethodID getDefaultDisplay = env->GetMethodID(
      windowManagerClass, "getDefaultDisplay", "()Landroid/view/Display;");
  if (!getDefaultDisplay || art::clearJavaException(env))
    return false;
  jobject display = env->CallObjectMethod(windowManager, getDefaultDisplay);
  if (!display || art::clearJavaException(env))
    return false;
  jclass displayClass = env->GetObjectClass(display);
  if (!displayClass || art::clearJavaException(env))
    return false;
  jmethodID getRotation = env->GetMethodID(displayClass, "getRotation", "()I");
  jmethodID getRealSize = env->GetMethodID(displayClass, "getRealSize",
                                           "(Landroid/graphics/Point;)V");
  if (!getRotation || !getRealSize || art::clearJavaException(env))
    return false;

  jclass pointClass = env->FindClass("android/graphics/Point");
  if (!pointClass || art::clearJavaException(env))
    return false;
  jmethodID pointConstructor = env->GetMethodID(pointClass, "<init>", "()V");
  jfieldID pointX = env->GetFieldID(pointClass, "x", "I");
  jfieldID pointY = env->GetFieldID(pointClass, "y", "I");
  if (!pointConstructor || !pointX || !pointY || art::clearJavaException(env))
    return false;

  jobject displayGlobal = env->NewGlobalRef(display);
  jclass pointClassGlobal = static_cast<jclass>(env->NewGlobalRef(pointClass));
  if (!displayGlobal || !pointClassGlobal || art::clearJavaException(env)) {
    if (displayGlobal)
      env->DeleteGlobalRef(displayGlobal);
    if (pointClassGlobal)
      env->DeleteGlobalRef(pointClassGlobal);
    return false;
  }

  display_ = displayGlobal;
  pointClass_ = pointClassGlobal;
  getRotation_ = getRotation;
  getRealSize_ = getRealSize;
  pointConstructor_ = pointConstructor;
  pointX_ = pointX;
  pointY_ = pointY;
  if (!bindingsLogged_) {
    log::info("display source ready: Android Display API");
    bindingsLogged_ = true;
  }
  return true;
}

bool JavaDisplaySource::query(DisplayInfo &info) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  JavaVM *vm = art::javaVm();
  if (!vm)
    return false;
  art::ScopedJniEnv scopedEnv(vm);
  JNIEnv *env = scopedEnv.get();
  if (!env || env->PushLocalFrame(32) != JNI_OK) {
    if (env)
      art::clearJavaException(env);
    return false;
  }

  bool success = false;
  if (ensureBindingsLocked(env)) {
    const jint rotation = env->CallIntMethod(display_, getRotation_);
    if (!art::clearJavaException(env)) {
      jobject point = env->NewObject(pointClass_, pointConstructor_);
      if (point && !art::clearJavaException(env)) {
        env->CallVoidMethod(display_, getRealSize_, point);
        if (!art::clearJavaException(env)) {
          const jint width = env->GetIntField(point, pointX_);
          const jint height = env->GetIntField(point, pointY_);
          if (!art::clearJavaException(env) && width > 0 && height > 0 &&
              rotation >= 0 && rotation <= 3) {
            info.width = static_cast<uint32_t>(width);
            info.height = static_cast<uint32_t>(height);
            info.rotation = static_cast<Rotation>(rotation);
            info.detected = true;
            success = true;
          }
        }
      } else {
        art::clearJavaException(env);
      }
    }
  }
  env->PopLocalFrame(nullptr);
  return success;
}

struct DisplayCache {
  std::mutex mutex;
  DisplayInfo info{};
  std::chrono::steady_clock::time_point nextQuery{};
  bool warned = false;
};

JavaDisplaySource &displaySource() {
  static auto *source = new JavaDisplaySource;
  return *source;
}

DisplayCache &displayCache() {
  static auto *cache = new DisplayCache;
  return *cache;
}

}
uint32_t inputTransform(const DisplayInfo &info) noexcept {
  if (!info.detected)
    return TOUCHER_TRANSFORM_ROTATE_270;
  switch (info.rotation) {
  case Rotation::Rotate0:
    return TOUCHER_TRANSFORM_IDENTITY;
  case Rotation::Rotate90:
    return TOUCHER_TRANSFORM_ROTATE_270;
  case Rotation::Rotate180:
    return TOUCHER_TRANSFORM_ROTATE_180;
  case Rotation::Rotate270:
    return TOUCHER_TRANSFORM_ROTATE_90;
  }
  return TOUCHER_TRANSFORM_ROTATE_270;
}

DisplayInfo queryDisplayInfo() noexcept {
  initializeDisplayInfo();
  DisplayCache &cache = displayCache();
  std::lock_guard<std::mutex> lock(cache.mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now < cache.nextQuery)
    return cache.info;

  DisplayInfo detected;
  if (displaySource().query(detected)) {
    const bool changed = !cache.info.detected ||
                         cache.info.rotation != detected.rotation ||
                         cache.info.width != detected.width ||
                         cache.info.height != detected.height;
    cache.info = detected;
    cache.nextQuery = now + std::chrono::milliseconds(250);
    if (changed)
      log::info("display detected: %ux%u rotation=%u inputTransform=%u",
                detected.width, detected.height,
                static_cast<uint32_t>(detected.rotation),
                inputTransform(detected));
  } else {
    cache.nextQuery = now + std::chrono::seconds(2);
    if (!cache.info.detected && !cache.warned) {
      log::warn("display detection unavailable; input fallback=ROTATE_270");
      cache.warned = true;
    }
  }
  return cache.info;
}

void initializeDisplayInfo() noexcept { art::initializeRuntime(); }

}
