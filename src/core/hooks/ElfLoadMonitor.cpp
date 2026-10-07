#include "ElfLoadMonitor.hpp"

#include "hridhi/hridhi.h"
#include "utils/LogUtils.hpp"

#include <dlfcn.h>
#include <elf.h>
#include <link.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace glass_ui::hooks {
namespace {

using AndroidDlopenExtFn =
    void *(*)(const char *, int, const android_dlextinfo *);

struct LoadingRegistration {
  uint64_t id = 0;
  std::string name;
  ElfLoadingCallback callback = nullptr;
  void *context = nullptr;
};

struct LoadedRegistration {
  uint64_t id = 0;
  std::string name;
  ElfLoadedCallback callback = nullptr;
  void *context = nullptr;
};

struct Registry {
  std::mutex mutex;
  std::vector<LoadingRegistration> loading;
  std::vector<LoadedRegistration> loaded;
  uint64_t nextId = 1;
};

std::atomic<AndroidDlopenExtFn> originalAndroidDlopenExt{nullptr};
hridhi_t androidDlopenExtHook = nullptr;
thread_local bool insideLoaderHook = false;

Registry &registry() {
  static auto *instance = new Registry;
  return *instance;
}

const char *baseName(const char *path) noexcept {
  if (!path)
    return "";
  const char *slash = std::strrchr(path, '/');
  return slash ? slash + 1 : path;
}

std::string normalizedName(const char *name) {
  return name ? baseName(name) : "";
}

std::vector<LoadingRegistration> loadingCallbacks(const char *name) {
  std::vector<LoadingRegistration> result;
  Registry &state = registry();
  std::lock_guard<std::mutex> lock(state.mutex);
  for (const LoadingRegistration &entry : state.loading) {
    if (entry.name == name)
      result.push_back(entry);
  }
  return result;
}

std::vector<LoadedRegistration> loadedCallbacks(const char *name) {
  std::vector<LoadedRegistration> result;
  Registry &state = registry();
  std::lock_guard<std::mutex> lock(state.mutex);
  for (const LoadedRegistration &entry : state.loaded) {
    if (entry.name == name)
      result.push_back(entry);
  }
  return result;
}

uintptr_t findLoadedBase(const char *name) noexcept {
  struct Search {
    const char *name;
    uintptr_t base = 0;
  } search{name};
  dl_iterate_phdr(
      [](dl_phdr_info *info, size_t, void *data) {
        auto &current = *static_cast<Search *>(data);
        if (std::strcmp(baseName(info->dlpi_name), current.name) != 0)
          return 0;
        current.base = static_cast<uintptr_t>(info->dlpi_addr);
        return 1;
      },
      &search);
  return search.base;
}

void notifyLoaded(const char *path, void *handle, bool alreadyLoaded,
                  ElfLoadedCallback singleCallback = nullptr,
                  void *singleContext = nullptr) noexcept {
  const char *name = baseName(path);
  const ElfLoadedEvent event{path, name, handle, findLoadedBase(name),
                             alreadyLoaded};
  if (singleCallback) {
    singleCallback(event, singleContext);
    return;
  }
  for (const LoadedRegistration &entry : loadedCallbacks(name))
    entry.callback(event, entry.context);
}

bool replayLoaded(const LoadedRegistration &entry) noexcept {
  struct Search {
    const LoadedRegistration *entry;
    std::string path;
    uintptr_t base = 0;
    bool found = false;
  } search{&entry, {}, 0, false};
  dl_iterate_phdr(
      [](dl_phdr_info *info, size_t, void *data) {
        auto &current = *static_cast<Search *>(data);
        if (std::strcmp(baseName(info->dlpi_name), current.entry->name.c_str()) !=
            0)
          return 0;
        current.path = info->dlpi_name;
        current.base = static_cast<uintptr_t>(info->dlpi_addr);
        current.found = true;
        return 1;
      },
      &search);
  if (search.found) {
    const ElfLoadedEvent event{search.path.c_str(), baseName(search.path.c_str()),
                               nullptr, search.base, true};
    entry.callback(event, entry.context);
  }
  return search.found;
}

void *simulateLoadFailure(AndroidDlopenExtFn original) noexcept {
  // Use an empty path without mutating caller memory; clear extInfo to block fd loads.
  return original("", RTLD_NOW | RTLD_LOCAL, nullptr);
}

void *hookedAndroidDlopenExt(const char *path, int flags,
                             const android_dlextinfo *extInfo) {
  const AndroidDlopenExtFn original =
      originalAndroidDlopenExt.load(std::memory_order_acquire);
  if (!original)
    return nullptr;
  if (insideLoaderHook)
    return original(path, flags, extInfo);

  insideLoaderHook = true;
  const char *name = baseName(path);
  const ElfLoadRequest request{path, name, flags, extInfo};
  bool skipped = false;
  for (const LoadingRegistration &entry : loadingCallbacks(name)) {
    if (entry.callback(request, entry.context) == ElfLoadAction::SkipOrigin) {
      skipped = true;
      break;
    }
  }

  if (skipped) {
    log::info("android_dlopen_ext skipped: %s", path ? path : "<null>");
    void *result = simulateLoadFailure(original);
    insideLoaderHook = false;
    return result;
  }

  void *handle = original(path, flags, extInfo);
  if (handle && path)
    notifyLoaded(path, handle, false);
  insideLoaderHook = false;
  return handle;
}

uint32_t elfHash(const char *name) noexcept {
  uint32_t hash = 0;
  while (*name) {
    hash = (hash << 4) + static_cast<uint8_t>(*name++);
    const uint32_t high = hash & 0xf0000000u;
    if (high)
      hash ^= high >> 24;
    hash &= ~high;
  }
  return hash;
}

uint32_t gnuHash(const char *name) noexcept {
  uint32_t hash = 5381;
  while (*name)
    hash = hash * 33u + static_cast<uint8_t>(*name++);
  return hash;
}

struct DynamicImage {
  uintptr_t base = 0;
  uintptr_t loadMinimum = 0;
  uintptr_t loadMaximum = 0;
  const ElfW(Sym) *symbols = nullptr;
  const char *strings = nullptr;
  const uint32_t *systemVHash = nullptr;
  const uint32_t *gnuHash = nullptr;

  uintptr_t address(ElfW(Addr) value) const noexcept {
    const uintptr_t raw = static_cast<uintptr_t>(value);
    return raw >= loadMinimum && raw < loadMaximum ? raw : base + raw;
  }
};

bool readDynamicImage(uintptr_t base, DynamicImage &image) noexcept {
  if (!base)
    return false;
  const auto *header = reinterpret_cast<const ElfW(Ehdr) *>(base);
  if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
      header->e_ident[EI_CLASS] != ELFCLASS64)
    return false;
  const auto *programHeaders = reinterpret_cast<const ElfW(Phdr) *>(
      base + static_cast<uintptr_t>(header->e_phoff));
  const ElfW(Dyn) *dynamic = nullptr;
  uintptr_t minimum = UINTPTR_MAX;
  uintptr_t maximum = 0;
  for (ElfW(Half) index = 0; index < header->e_phnum; ++index) {
    const ElfW(Phdr) &program = programHeaders[index];
    if (program.p_type == PT_LOAD) {
      minimum = std::min(minimum,
                         base + static_cast<uintptr_t>(program.p_vaddr));
      maximum = std::max(
          maximum, base + static_cast<uintptr_t>(program.p_vaddr) +
                       static_cast<uintptr_t>(program.p_memsz));
    } else if (program.p_type == PT_DYNAMIC) {
      dynamic = reinterpret_cast<const ElfW(Dyn) *>(
          base + static_cast<uintptr_t>(program.p_vaddr));
    }
  }
  if (!dynamic || minimum == UINTPTR_MAX || maximum <= minimum)
    return false;

  image.base = base;
  image.loadMinimum = minimum;
  image.loadMaximum = maximum;
  for (const ElfW(Dyn) *entry = dynamic; entry->d_tag != DT_NULL; ++entry) {
    switch (entry->d_tag) {
    case DT_SYMTAB:
      image.symbols = reinterpret_cast<const ElfW(Sym) *>(
          image.address(entry->d_un.d_ptr));
      break;
    case DT_STRTAB:
      image.strings = reinterpret_cast<const char *>(
          image.address(entry->d_un.d_ptr));
      break;
    case DT_HASH:
      image.systemVHash = reinterpret_cast<const uint32_t *>(
          image.address(entry->d_un.d_ptr));
      break;
    case DT_GNU_HASH:
      image.gnuHash = reinterpret_cast<const uint32_t *>(
          image.address(entry->d_un.d_ptr));
      break;
    default:
      break;
    }
  }
  return image.symbols && image.strings &&
         (image.systemVHash || image.gnuHash);
}

void *symbolAddress(const DynamicImage &image,
                    const ElfW(Sym) &symbol) noexcept {
  if (symbol.st_shndx == SHN_UNDEF || symbol.st_value == 0)
    return nullptr;
  const uintptr_t value = symbol.st_shndx == SHN_ABS
                              ? static_cast<uintptr_t>(symbol.st_value)
                              : image.base +
                                    static_cast<uintptr_t>(symbol.st_value);
  return reinterpret_cast<void *>(value);
}

void *findSystemVSymbol(const DynamicImage &image,
                        const char *name) noexcept {
  if (!image.systemVHash)
    return nullptr;
  const uint32_t bucketCount = image.systemVHash[0];
  const uint32_t chainCount = image.systemVHash[1];
  if (!bucketCount || !chainCount)
    return nullptr;
  const uint32_t *buckets = image.systemVHash + 2;
  const uint32_t *chains = buckets + bucketCount;
  for (uint32_t index = buckets[elfHash(name) % bucketCount];
       index != STN_UNDEF && index < chainCount; index = chains[index]) {
    const ElfW(Sym) &symbol = image.symbols[index];
    if (std::strcmp(image.strings + symbol.st_name, name) == 0)
      return symbolAddress(image, symbol);
  }
  return nullptr;
}

void *findGnuSymbol(const DynamicImage &image, const char *name) noexcept {
  if (!image.gnuHash)
    return nullptr;
  const uint32_t bucketCount = image.gnuHash[0];
  const uint32_t symbolOffset = image.gnuHash[1];
  const uint32_t bloomCount = image.gnuHash[2];
  const uint32_t bloomShift = image.gnuHash[3];
  if (!bucketCount || !bloomCount)
    return nullptr;

  const auto *bloom =
      reinterpret_cast<const ElfW(Addr) *>(image.gnuHash + 4);
  const auto *buckets =
      reinterpret_cast<const uint32_t *>(bloom + bloomCount);
  const uint32_t *chains = buckets + bucketCount;
  const uint32_t hash = gnuHash(name);
  constexpr uint32_t BloomBits = sizeof(ElfW(Addr)) * 8u;
  const ElfW(Addr) word = bloom[(hash / BloomBits) % bloomCount];
  const ElfW(Addr) mask =
      (ElfW(Addr){1} << (hash % BloomBits)) |
      (ElfW(Addr){1} << ((hash >> bloomShift) % BloomBits));
  if ((word & mask) != mask)
    return nullptr;

  uint32_t index = buckets[hash % bucketCount];
  if (index < symbolOffset)
    return nullptr;
  for (;;) {
    const uint32_t chainHash = chains[index - symbolOffset];
    const ElfW(Sym) &symbol = image.symbols[index];
    if ((chainHash | 1u) == (hash | 1u) &&
        std::strcmp(image.strings + symbol.st_name, name) == 0)
      return symbolAddress(image, symbol);
    if (chainHash & 1u)
      return nullptr;
    ++index;
  }
}

bool installHook() noexcept {
  static std::once_flag once;
  static bool installed = false;
  std::call_once(once, [] {
    if (!hridhi_available()) {
      log::error("ELF load monitor: hridhi unavailable");
      return;
    }
    void *library = dlopen("libdl.so", RTLD_NOW | RTLD_NOLOAD);
    if (!library)
      library = dlopen("libdl.so", RTLD_NOW);
    if (!library) {
      log::error("ELF load monitor: dlopen(libdl.so) failed: %s", dlerror());
      return;
    }
    void *target = dlsym(library, "android_dlopen_ext");
    if (!target) {
      log::error("ELF load monitor: android_dlopen_ext not found");
      return;
    }
    void *gateway = nullptr;
    androidDlopenExtHook = hridhi_hook_install(
        target, reinterpret_cast<void *>(hookedAndroidDlopenExt), &gateway);
    if (!androidDlopenExtHook) {
      log::error("ELF load monitor hook failed @ %p: %s", target,
                 hridhi_strerror(errno));
      return;
    }
    originalAndroidDlopenExt.store(reinterpret_cast<AndroidDlopenExtFn>(gateway),
                                   std::memory_order_release);
    installed = true;
    log::info("ELF load monitor ready");
  });
  return installed;
}

}
bool initializeElfLoadMonitor() noexcept { return installHook(); }

uint64_t onElfLoading(const char *name, ElfLoadingCallback callback,
                      void *context) noexcept {
  if (!name || !*name || !callback)
    return 0;
  LoadingRegistration entry;
  entry.name = normalizedName(name);
  entry.callback = callback;
  entry.context = context;
  Registry &state = registry();
  std::lock_guard<std::mutex> lock(state.mutex);
  entry.id = state.nextId++;
  state.loading.push_back(entry);
  return entry.id;
}

uint64_t onElfLoaded(const char *name, ElfLoadedCallback callback,
                     void *context) noexcept {
  if (!name || !*name || !callback)
    return 0;
  LoadedRegistration entry;
  entry.name = normalizedName(name);
  entry.callback = callback;
  entry.context = context;
  Registry &state = registry();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    entry.id = state.nextId++;
    state.loaded.push_back(entry);
  }
  replayLoaded(entry);
  return entry.id;
}

bool removeElfLoadCallback(uint64_t id) noexcept {
  if (!id)
    return false;
  Registry &state = registry();
  std::lock_guard<std::mutex> lock(state.mutex);
  const auto loadingEnd =
      std::remove_if(state.loading.begin(), state.loading.end(),
                     [id](const LoadingRegistration &entry) {
                       return entry.id == id;
                     });
  const bool removedLoading = loadingEnd != state.loading.end();
  state.loading.erase(loadingEnd, state.loading.end());
  const auto loadedEnd =
      std::remove_if(state.loaded.begin(), state.loaded.end(),
                     [id](const LoadedRegistration &entry) {
                       return entry.id == id;
                     });
  const bool removedLoaded = loadedEnd != state.loaded.end();
  state.loaded.erase(loadedEnd, state.loaded.end());
  return removedLoading || removedLoaded;
}

void *resolveElfSymbol(const ElfLoadedEvent &event,
                       const char *symbol) noexcept {
  if (!symbol || !*symbol)
    return nullptr;
  if (event.handle) {
    if (void *address = dlsym(event.handle, symbol))
      return address;
  }
  DynamicImage image;
  if (!readDynamicImage(event.baseAddress, image))
    return nullptr;
  if (void *address = findGnuSymbol(image, symbol))
    return address;
  return findSystemVSymbol(image, symbol);
}

}
