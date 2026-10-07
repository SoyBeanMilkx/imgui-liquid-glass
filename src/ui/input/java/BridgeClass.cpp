#include "BridgeClass.hpp"

#include "generated/input_bridge.hpp"
#include "utils/ART.hpp"

#include <mutex>

namespace glass_ui::input::java {
namespace {

struct Bindings {
  std::mutex mutex;
  jobject loader = nullptr;
  jmethodID loadClass = nullptr;
};

Bindings &bindings() {
  static auto *value = new Bindings;
  return *value;
}

}

jclass loadBridgeClass(JNIEnv *env, const char *name) noexcept {
  auto &api = bindings();
  std::lock_guard<std::mutex> lock(api.mutex);
  if (!api.loader) {
    jclass anchor = art::loadDexClass(env, kInputBridgeDexData,
                                     sizeof(kInputBridgeDexData), name);
    if (!anchor)
      return nullptr;
    jclass clazz = env->FindClass("java/lang/Class");
    jclass loaderClass = env->FindClass("java/lang/ClassLoader");
    if (art::clearJavaException(env) || !clazz || !loaderClass)
      return nullptr;
    jmethodID getLoader = env->GetMethodID(
        clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID loadClass = env->GetMethodID(
        loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (art::clearJavaException(env) || !getLoader || !loadClass)
      return nullptr;
    jobject loader = env->CallObjectMethod(anchor, getLoader);
    if (art::clearJavaException(env) || !loader)
      return nullptr;
    jobject global = env->NewGlobalRef(loader);
    if (art::clearJavaException(env) || !global)
      return nullptr;
    api.loader = global;
    api.loadClass = loadClass;
  }
  jstring className = env->NewStringUTF(name);
  if (art::clearJavaException(env) || !className)
    return nullptr;
  auto clazz = static_cast<jclass>(
      env->CallObjectMethod(api.loader, api.loadClass, className));
  return art::clearJavaException(env) ? nullptr : clazz;
}

}
