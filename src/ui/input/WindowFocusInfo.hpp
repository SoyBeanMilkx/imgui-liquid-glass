#pragma once

#include <jni.h>

namespace glass_ui::window {

enum class FocusState {
  Unknown,
  Focused,
  Unfocused,
};

// Unknown preserves input; only explicit focus loss pauses capture.
void initializeFocusInfo() noexcept;
FocusState queryFocusState() noexcept;
// Returns a local JNI reference owned by the caller.
jobject focusedActivity(JNIEnv *env) noexcept;

}
