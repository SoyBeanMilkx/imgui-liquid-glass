#pragma once

#include <android/dlext.h>

#include <cstdint>

namespace glass_ui::hooks {

struct ElfLoadRequest {
  const char *path = nullptr;
  const char *name = nullptr;
  int flags = 0;
  const android_dlextinfo *extInfo = nullptr;
};

struct ElfLoadedEvent {
  const char *path = nullptr;
  const char *name = nullptr;
  void *handle = nullptr;
  uintptr_t baseAddress = 0;
  bool alreadyLoaded = false;
};

enum class ElfLoadAction {
  Continue,
  SkipOrigin,
};

using ElfLoadingCallback =
    ElfLoadAction (*)(const ElfLoadRequest &, void *context) noexcept;
using ElfLoadedCallback =
    void (*)(const ElfLoadedEvent &, void *context) noexcept;

// Owns the sole android_dlopen_ext hook; consumers register callbacks here.
bool initializeElfLoadMonitor() noexcept;

// Matches exact basenames and returns zero when registration fails.
uint64_t onElfLoading(const char *name, ElfLoadingCallback callback,
                      void *context = nullptr) noexcept;
uint64_t onElfLoaded(const char *name, ElfLoadedCallback callback,
                     void *context = nullptr) noexcept;
bool removeElfLoadCallback(uint64_t id) noexcept;

// Resolves through dlsym or the mapped ELF to bypass linker namespaces.
void *resolveElfSymbol(const ElfLoadedEvent &event,
                       const char *symbol) noexcept;

}
