#include "LogUtils.hpp"

#include <android/log.h>

#include <cstdarg>

#ifndef GLASS_UI_LOG_TAG
#define GLASS_UI_LOG_TAG "yuuki_glass_ui"
#endif

namespace glass_ui::log {
namespace {

enum class Level { Info, Warning, Error };

int androidPriority(Level level) {
  switch (level) {
  case Level::Warning:
    return ANDROID_LOG_WARN;
  case Level::Error:
    return ANDROID_LOG_ERROR;
  default:
    return ANDROID_LOG_INFO;
  }
}

void write(Level level, const char *format, va_list args) {
  __android_log_vprint(androidPriority(level), GLASS_UI_LOG_TAG, format, args);
}

}

void info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  write(Level::Info, format, args);
  va_end(args);
}

void warn(const char *format, ...) {
  va_list args;
  va_start(args, format);
  write(Level::Warning, format, args);
  va_end(args);
}

void error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  write(Level::Error, format, args);
  va_end(args);
}

}
