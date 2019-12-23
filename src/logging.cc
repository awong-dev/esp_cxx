#include "esp_cxx/logging.h"

namespace esp_cxx {

namespace {

static std::function<void(std::string_view)> g_on_log_cb;
vprintf_like_t g_orig_vprintf;

int FilterLogs(const char *format, va_list args) {
  int retval = g_orig_vprintf(format, args);
  static char buf[512];
  vsnprintf(&buf[0], sizeof(buf), format, args);
  g_on_log_cb(buf);

  return retval;
}

}  // namespace

void SetLogFilter(std::function<void(std::string_view)> on_log) {
  g_on_log_cb = std::move(on_log);
  if (!g_orig_vprintf) {
    g_orig_vprintf = esp_log_set_vprintf(&FilterLogs);
  }
}

}  // namespace esp_cxx
