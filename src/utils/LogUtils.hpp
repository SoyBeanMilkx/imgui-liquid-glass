#pragma once

namespace glass_ui::log {

void info(const char *format, ...) __attribute__((format(printf, 1, 2)));
void warn(const char *format, ...) __attribute__((format(printf, 1, 2)));
void error(const char *format, ...) __attribute__((format(printf, 1, 2)));

}
