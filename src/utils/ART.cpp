#include "ART.hpp"

#include "core/hooks/ElfLoadMonitor.hpp"
#include "utils/LogUtils.hpp"

#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace glass_ui::art {
namespace {

using GetCreatedJavaVMsFn = jint (*)(JavaVM **, jsize, jsize *);

class RuntimeSource final {
public:
  void attach(const hooks::ElfLoadedEvent &event) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (getCreatedJavaVMs_)
      return;
    getCreatedJavaVMs_ = reinterpret_cast<GetCreatedJavaVMsFn>(
        hooks::resolveElfSymbol(event, "JNI_GetCreatedJavaVMs"));
    if (getCreatedJavaVMs_)
      log::info("JavaVM source attached: %s (%s)", event.name,
                event.handle ? "loader handle" : "mapped ELF");
  }

  JavaVM *get() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (vm_)
      return vm_;
    if (!getCreatedJavaVMs_)
      return nullptr;

    JavaVM *candidate = nullptr;
    jsize count = 0;
    if (getCreatedJavaVMs_(&candidate, 1, &count) != JNI_OK || count < 1 ||
        !candidate)
      return nullptr;
    vm_ = candidate;
    log::info("Android runtime acquired existing JavaVM");
    return vm_;
  }

private:
  std::mutex mutex_;
  GetCreatedJavaVMsFn getCreatedJavaVMs_ = nullptr;
  JavaVM *vm_ = nullptr;
};

RuntimeSource &runtimeSource() {
  static auto *source = new RuntimeSource;
  return *source;
}

void attachJavaVmSource(const hooks::ElfLoadedEvent &event,
                        void *context) noexcept {
  static_cast<RuntimeSource *>(context)->attach(event);
}

}
void initializeRuntime() noexcept {
  static std::once_flag once;
  std::call_once(once, [] {
    RuntimeSource &source = runtimeSource();
    const uint64_t art =
        hooks::onElfLoaded("libart.so", attachJavaVmSource, &source);
    const uint64_t nativeHelper =
        hooks::onElfLoaded("libnativehelper.so", attachJavaVmSource, &source);
    if (!art && !nativeHelper)
      log::warn("JavaVM source registration failed");
  });
}

JavaVM *javaVm() noexcept {
  initializeRuntime();
  return runtimeSource().get();
}

bool clearJavaException(JNIEnv *env) noexcept {
  if (!env || !env->ExceptionCheck())
    return false;
  env->ExceptionClear();
  return true;
}

jobject currentApplication(JNIEnv *env) noexcept {
  if (!env)
    return nullptr;
  jclass activityThread = env->FindClass("android/app/ActivityThread");
  if (activityThread && !clearJavaException(env)) {
    jmethodID method = env->GetStaticMethodID(
        activityThread, "currentApplication", "()Landroid/app/Application;");
    if (method && !clearJavaException(env)) {
      jobject application = env->CallStaticObjectMethod(activityThread, method);
      if (!clearJavaException(env) && application)
        return application;
    }
  }
  clearJavaException(env);

  jclass appGlobals = env->FindClass("android/app/AppGlobals");
  if (!appGlobals || clearJavaException(env))
    return nullptr;
  jmethodID method = env->GetStaticMethodID(appGlobals, "getInitialApplication",
                                            "()Landroid/app/Application;");
  if (!method || clearJavaException(env))
    return nullptr;
  jobject application = env->CallStaticObjectMethod(appGlobals, method);
  return clearJavaException(env) ? nullptr : application;
}

jclass loadDexClass(JNIEnv *env, const void *data, std::size_t size,
                    const char *className) noexcept {
  if (!env || !data || !size || !className)
    return nullptr;
  if (env->PushLocalFrame(24) != JNI_OK) {
    clearJavaException(env);
    return nullptr;
  }
  jclass result = nullptr;
  const auto load = [&]() -> jclass {
    jclass loaderClass = env->FindClass("java/lang/ClassLoader");
    if (clearJavaException(env) || !loaderClass)
      return nullptr;
    jmethodID system = env->GetStaticMethodID(
        loaderClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID loadClass = env->GetMethodID(
        loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (clearJavaException(env) || !system || !loadClass)
      return nullptr;
    jobject parent = env->CallStaticObjectMethod(loaderClass, system);
    if (clearJavaException(env))
      return nullptr;
    jobject loader = nullptr;
    jclass memoryClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    const bool memoryAvailable = !clearJavaException(env) && memoryClass;
    if (memoryAvailable) {
      jmethodID constructor =
          env->GetMethodID(memoryClass, "<init>",
                           "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
      if (clearJavaException(env) || !constructor)
        return nullptr;
      jobject bytes = env->NewDirectByteBuffer(const_cast<void *>(data), size);
      if (clearJavaException(env) || !bytes)
        return nullptr;
      loader = env->NewObject(memoryClass, constructor, bytes, parent);
    } else {
      jobject application = currentApplication(env);
      if (!application)
        return nullptr;
      jclass context = env->FindClass("android/content/Context");
      jclass file = env->FindClass("java/io/File");
      if (clearJavaException(env) || !context || !file)
        return nullptr;
      jmethodID cache =
          env->GetMethodID(context, "getCodeCacheDir", "()Ljava/io/File;");
      jmethodID path =
          env->GetMethodID(file, "getAbsolutePath", "()Ljava/lang/String;");
      if (clearJavaException(env) || !cache || !path)
        return nullptr;
      jobject directory = env->CallObjectMethod(application, cache);
      if (clearJavaException(env) || !directory)
        return nullptr;
      auto cachePath =
          static_cast<jstring>(env->CallObjectMethod(directory, path));
      if (clearJavaException(env) || !cachePath)
        return nullptr;
      const char *chars = env->GetStringUTFChars(cachePath, nullptr);
      if (clearJavaException(env) || !chars)
        return nullptr;
      std::string name = std::string(chars) + "/first-ime-XXXXXX.dex";
      env->ReleaseStringUTFChars(cachePath, chars);
      const int fd = mkstemps(name.data(), 4);
      if (fd < 0)
        return nullptr;
      bool written = fchmod(fd, S_IRUSR) == 0;
      const auto *bytes = static_cast<const unsigned char *>(data);
      std::size_t offset = 0;
      while (written && offset < size) {
        const ssize_t count = write(fd, bytes + offset, size - offset);
        if (count < 0 && errno == EINTR)
          continue;
        if (count <= 0)
          written = false;
        else
          offset += static_cast<std::size_t>(count);
      }
      close(fd);
      jclass diskClass = env->FindClass("dalvik/system/DexClassLoader");
      if (written && !clearJavaException(env) && diskClass) {
        jmethodID constructor =
            env->GetMethodID(diskClass, "<init>",
                             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
                             "String;Ljava/lang/ClassLoader;)V");
        if (!clearJavaException(env) && constructor) {
          jstring dexPath = env->NewStringUTF(name.c_str());
          if (!clearJavaException(env) && dexPath)
            loader = env->NewObject(diskClass, constructor, dexPath, cachePath,
                                    nullptr, parent);
        }
      }
      unlink(name.c_str());
    }
    if (clearJavaException(env) || !loader)
      return nullptr;
    jstring name = env->NewStringUTF(className);
    if (clearJavaException(env) || !name)
      return nullptr;
    auto clazz =
        static_cast<jclass>(env->CallObjectMethod(loader, loadClass, name));
    return clearJavaException(env) ? nullptr : clazz;
  };
  result = load();
  clearJavaException(env);
  return static_cast<jclass>(env->PopLocalFrame(result));
}

ScopedJniEnv::ScopedJniEnv(JavaVM *vm) noexcept : vm_(vm) {
  if (!vm_)
    return;
  const jint result =
      vm_->GetEnv(reinterpret_cast<void **>(&env_), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if (vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK)
      attached_ = true;
    else
      env_ = nullptr;
  } else if (result != JNI_OK) {
    env_ = nullptr;
  }
}

ScopedJniEnv::~ScopedJniEnv() {
  if (attached_)
    vm_->DetachCurrentThread();
}

}
