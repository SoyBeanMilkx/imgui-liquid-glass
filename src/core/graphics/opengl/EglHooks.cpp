#include "EglHooks.hpp"

#include "hridhi/hridhi.h"
#include "ui/renderer/opengl/OpenGLRenderer.hpp"
#include "utils/LogUtils.hpp"

#include <EGL/egl.h>

#include <dlfcn.h>

#include <atomic>
#include <cerrno>
#include <mutex>

namespace {

using SwapBuffersFn = EGLBoolean(EGLAPIENTRYP)(EGLDisplay, EGLSurface);
using SwapBuffersWithDamageFn = EGLBoolean(EGLAPIENTRYP)(EGLDisplay, EGLSurface,
                                                         const EGLint *, EGLint);
using DestroyContextFn = EGLBoolean(EGLAPIENTRYP)(EGLDisplay, EGLContext);
using DestroySurfaceFn = EGLBoolean(EGLAPIENTRYP)(EGLDisplay, EGLSurface);

std::atomic<SwapBuffersFn> originalSwapBuffers{nullptr};
std::atomic<SwapBuffersWithDamageFn> originalSwapBuffersWithDamage{nullptr};
std::atomic<DestroyContextFn> originalDestroyContext{nullptr};
std::atomic<DestroySurfaceFn> originalDestroySurface{nullptr};
hridhi_t swapBuffersHook = nullptr;
hridhi_t swapBuffersWithDamageHook = nullptr;
hridhi_t destroyContextHook = nullptr;
hridhi_t destroySurfaceHook = nullptr;
thread_local bool inSwap = false;

EGLBoolean EGLAPIENTRY hookedSwapBuffers(EGLDisplay display,
                                         EGLSurface surface) {
  const auto original = originalSwapBuffers.load(std::memory_order_acquire);
  if (!original)
    return EGL_FALSE;
  if (inSwap)
    return original(display, surface);
  inSwap = true;
  glass_ui::renderer::opengl::beforeSwap(display, surface);
  const EGLBoolean result = original(display, surface);
  inSwap = false;
  return result;
}

EGLBoolean EGLAPIENTRY hookedSwapBuffersWithDamage(EGLDisplay display,
                                                   EGLSurface surface,
                                                   const EGLint *rects,
                                                   EGLint count) {
  const auto original =
      originalSwapBuffersWithDamage.load(std::memory_order_acquire);
  if (!original)
    return EGL_FALSE;
  if (inSwap)
    return original(display, surface, rects, count);
  inSwap = true;
  glass_ui::renderer::opengl::beforeSwap(display, surface);
  const EGLBoolean result = original(display, surface, rects, count);
  inSwap = false;
  return result;
}

EGLBoolean EGLAPIENTRY hookedDestroyContext(EGLDisplay display,
                                             EGLContext context) {
  glass_ui::renderer::opengl::destroyContext(context);
  const auto original = originalDestroyContext.load(std::memory_order_acquire);
  return original ? original(display, context) : EGL_FALSE;
}

EGLBoolean EGLAPIENTRY hookedDestroySurface(EGLDisplay display,
                                             EGLSurface surface) {
  glass_ui::renderer::opengl::destroySurface(surface);
  const auto original = originalDestroySurface.load(std::memory_order_acquire);
  return original ? original(display, surface) : EGL_FALSE;
}

template <typename Function>
bool install(void *library, const char *name, void *replacement,
             std::atomic<Function> &original, hridhi_t &handle,
             bool required) {
  void *target = dlsym(library, name);
  if (!target) {
    if (required)
      glass_ui::log::error("dlsym(%s) failed", name);
    return !required;
  }
  void *gateway = nullptr;
  handle = hridhi_hook_install(target, replacement, &gateway);
  if (!handle) {
    if (required)
      glass_ui::log::error("hridhi_hook_install(%s @ %p): %s", name, target,
                        hridhi_strerror(errno));
    return !required;
  }
  original.store(reinterpret_cast<Function>(gateway),
                 std::memory_order_release);
  return true;
}

bool installHooks() {
  static std::once_flag once;
  static bool installed = false;
  std::call_once(once, [] {
    if (!hridhi_available()) {
      glass_ui::log::error("hridhi_available() = 0");
      return;
    }
    void *library = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!library)
      library = dlopen("libEGL.so", RTLD_NOW);
    if (!library) {
      glass_ui::log::error("dlopen(libEGL.so) failed: %s", dlerror());
      return;
    }
    const bool swap = install(library, "eglSwapBuffers",
                              reinterpret_cast<void *>(hookedSwapBuffers),
                              originalSwapBuffers, swapBuffersHook, true);
    install(library, "eglSwapBuffersWithDamageKHR",
            reinterpret_cast<void *>(hookedSwapBuffersWithDamage),
            originalSwapBuffersWithDamage, swapBuffersWithDamageHook, false);
    const bool context = install(
        library, "eglDestroyContext",
        reinterpret_cast<void *>(hookedDestroyContext), originalDestroyContext,
        destroyContextHook, true);
    const bool surface = install(
        library, "eglDestroySurface",
        reinterpret_cast<void *>(hookedDestroySurface), originalDestroySurface,
        destroySurfaceHook, true);
    installed = swap && context && surface;
    glass_ui::log::info("EGL hook install complete: %s",
                     installed ? "ready" : "failed");
  });
  return installed;
}

}
namespace glass_ui::hooks::opengl {

bool initialize() { return installHooks(); }

}
