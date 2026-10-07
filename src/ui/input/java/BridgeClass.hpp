#pragma once

#include <jni.h>

namespace glass_ui::input::java {

// Both platform adapters use one embedded DEX loader. Returns a local reference.
jclass loadBridgeClass(JNIEnv *env, const char *name) noexcept;

}
