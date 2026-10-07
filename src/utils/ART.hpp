#pragma once

#include <jni.h>

#include <cstddef>

namespace glass_ui::art {

// Reuses the host JavaVM without relying on JNI_OnLoad.
void initializeRuntime() noexcept;
JavaVM *javaVm() noexcept;

bool clearJavaException(JNIEnv *env) noexcept;
jobject currentApplication(JNIEnv *env) noexcept;
// Returns a local class reference; DEX data must remain alive while its classes are used.
jclass loadDexClass(JNIEnv *env, const void *data, std::size_t size,
                    const char *className) noexcept;

class ScopedJniEnv final {
public:
  explicit ScopedJniEnv(JavaVM *vm) noexcept;
  ~ScopedJniEnv();

  ScopedJniEnv(const ScopedJniEnv &) = delete;
  ScopedJniEnv &operator=(const ScopedJniEnv &) = delete;

  JNIEnv *get() const noexcept { return env_; }

private:
  JavaVM *vm_ = nullptr;
  JNIEnv *env_ = nullptr;
  bool attached_ = false;
};

}
