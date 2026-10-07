#include "WindowFocusInfo.hpp"

#include "utils/ART.hpp"
#include "utils/LogUtils.hpp"

#include <chrono>
#include <mutex>

namespace glass_ui::window {
namespace {

class JavaFocusSource final {
public:
  bool query(FocusState &state) noexcept;
  jobject focusedActivity(JNIEnv *env) noexcept;

private:
  bool ensureBindings(JNIEnv *env) noexcept;
  jobject scanActivities(JNIEnv *env, FocusState &state) noexcept;

  std::mutex mutex_;
  jclass activityThreadClass_ = nullptr;
  jclass activityRecordClass_ = nullptr;
  jclass activityClass_ = nullptr;
  jmethodID currentActivityThread_ = nullptr;
  jfieldID activities_ = nullptr;
  jmethodID values_ = nullptr;
  jmethodID toArray_ = nullptr;
  jfieldID activity_ = nullptr;
  jmethodID hasWindowFocus_ = nullptr;
  bool readyLogged_ = false;
};

bool JavaFocusSource::ensureBindings(JNIEnv *env) noexcept {
  if (activityThreadClass_ && activityRecordClass_ && activityClass_ &&
      currentActivityThread_ && activities_ && values_ && toArray_ &&
      activity_ && hasWindowFocus_)
    return true;

  jclass activityThread = env->FindClass("android/app/ActivityThread");
  jclass activityRecord =
      env->FindClass("android/app/ActivityThread$ActivityClientRecord");
  jclass activityClass = env->FindClass("android/app/Activity");
  jclass arrayMapClass = env->FindClass("android/util/ArrayMap");
  jclass collectionClass = env->FindClass("java/util/Collection");
  if (art::clearJavaException(env) || !activityThread || !activityRecord ||
      !activityClass || !arrayMapClass || !collectionClass)
    return false;

  jmethodID currentActivityThread =
      env->GetStaticMethodID(activityThread, "currentActivityThread",
                             "()Landroid/app/ActivityThread;");
  jfieldID activities =
      env->GetFieldID(activityThread, "mActivities", "Landroid/util/ArrayMap;");
  jmethodID values =
      env->GetMethodID(arrayMapClass, "values", "()Ljava/util/Collection;");
  jmethodID toArray =
      env->GetMethodID(collectionClass, "toArray", "()[Ljava/lang/Object;");
  jfieldID activity =
      env->GetFieldID(activityRecord, "activity", "Landroid/app/Activity;");
  jmethodID hasWindowFocus =
      env->GetMethodID(activityClass, "hasWindowFocus", "()Z");
  if (art::clearJavaException(env) || !currentActivityThread || !activities ||
      !values || !toArray || !activity || !hasWindowFocus)
    return false;

  auto *activityThreadGlobal =
      static_cast<jclass>(env->NewGlobalRef(activityThread));
  auto *activityRecordGlobal =
      static_cast<jclass>(env->NewGlobalRef(activityRecord));
  auto *activityGlobal = static_cast<jclass>(env->NewGlobalRef(activityClass));
  if (art::clearJavaException(env) || !activityThreadGlobal ||
      !activityRecordGlobal || !activityGlobal) {
    if (activityThreadGlobal)
      env->DeleteGlobalRef(activityThreadGlobal);
    if (activityRecordGlobal)
      env->DeleteGlobalRef(activityRecordGlobal);
    if (activityGlobal)
      env->DeleteGlobalRef(activityGlobal);
    return false;
  }

  activityThreadClass_ = activityThreadGlobal;
  activityRecordClass_ = activityRecordGlobal;
  activityClass_ = activityGlobal;
  currentActivityThread_ = currentActivityThread;
  activities_ = activities;
  values_ = values;
  toArray_ = toArray;
  activity_ = activity;
  hasWindowFocus_ = hasWindowFocus;
  if (!readyLogged_) {
    log::info("window focus source ready: Activity.hasWindowFocus");
    readyLogged_ = true;
  }
  return true;
}

jobject JavaFocusSource::scanActivities(JNIEnv *env,
                                       FocusState &state) noexcept {
  if (ensureBindings(env)) {
    jobject activityThread = env->CallStaticObjectMethod(
        activityThreadClass_, currentActivityThread_);
    jobject activities = activityThread
                             ? env->GetObjectField(activityThread, activities_)
                             : nullptr;
    jobject values =
        activities ? env->CallObjectMethod(activities, values_) : nullptr;
    auto *records =
        values
            ? static_cast<jobjectArray>(env->CallObjectMethod(values, toArray_))
            : nullptr;
    if (!art::clearJavaException(env) && records) {
      bool sawActivity = false;
      const jsize count = env->GetArrayLength(records);
      for (jsize index = 0; index < count; ++index) {
        jobject record = env->GetObjectArrayElement(records, index);
        jobject activity =
            record ? env->GetObjectField(record, activity_) : nullptr;
        if (!activity)
          continue;
        sawActivity = true;
        const jboolean focused =
            env->CallBooleanMethod(activity, hasWindowFocus_);
        if (art::clearJavaException(env))
          break;
        if (focused == JNI_TRUE) {
          state = FocusState::Focused;
          return activity;
        }
      }
      if (!art::clearJavaException(env) && sawActivity) {
        state = FocusState::Unfocused;
      }
    } else {
      art::clearJavaException(env);
    }
  }
  return nullptr;
}

bool JavaFocusSource::query(FocusState &state) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  art::ScopedJniEnv scopedEnv(art::javaVm());
  JNIEnv *env = scopedEnv.get();
  if (!env || env->PushLocalFrame(32) != JNI_OK) {
    if (env)
      art::clearJavaException(env);
    return false;
  }
  state = FocusState::Unknown;
  scanActivities(env, state);
  env->PopLocalFrame(nullptr);
  return state != FocusState::Unknown;
}

jobject JavaFocusSource::focusedActivity(JNIEnv *env) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!env || env->PushLocalFrame(32) != JNI_OK) {
    if (env)
      art::clearJavaException(env);
    return nullptr;
  }
  FocusState state = FocusState::Unknown;
  jobject activity = scanActivities(env, state);
  return env->PopLocalFrame(activity);
}

struct FocusCache {
  std::mutex mutex;
  FocusState state = FocusState::Unknown;
  std::chrono::steady_clock::time_point nextQuery{};
};

JavaFocusSource &focusSource() {
  static auto *source = new JavaFocusSource;
  return *source;
}

FocusCache &focusCache() {
  static auto *cache = new FocusCache;
  return *cache;
}

}
void initializeFocusInfo() noexcept { art::initializeRuntime(); }

jobject focusedActivity(JNIEnv *env) noexcept {
  return focusSource().focusedActivity(env);
}

FocusState queryFocusState() noexcept {
  initializeFocusInfo();
  FocusCache &cache = focusCache();
  std::lock_guard<std::mutex> lock(cache.mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now < cache.nextQuery)
    return cache.state;

  FocusState detected = FocusState::Unknown;
  if (focusSource().query(detected)) {
    cache.state = detected;
    cache.nextQuery = now + std::chrono::milliseconds(50);
  } else {
    cache.state = FocusState::Unknown;
    cache.nextQuery = now + std::chrono::seconds(1);
  }
  return cache.state;
}

}
