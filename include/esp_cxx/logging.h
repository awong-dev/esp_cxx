#ifndef ESPCXX_LOGGING_H_
#define ESPCXX_LOGGING_H_

#include "esp_cxx/cpointer.h"
#include "esp_cxx/cxx17hack.h"

#include <functional>

namespace esp_cxx {
constexpr char kEspCxxTag[] = "espcxx";

static inline unique_C_ptr<char> PrintJson(cJSON* data) {
  return unique_C_ptr<char>(cJSON_PrintUnformatted(data));
}

void SetLogFilter(std::function<void(std::string_view)> on_log, std::string_view device_id);

}  // namespace espcxx

#ifndef FAKE_ESP_IDF

#include "esp_log.h"

#else  // FAKE_ESP_IDF

#include <cstdio>
#include <cstdint>
#include <time.h>

#define ESP_LOGD(tag, fmt, args...) fprintf(stderr, "%s:%d D: %s: " fmt "\n", __FILE__, __LINE__, tag, ##args)
#define ESP_LOGI(tag, fmt, args...) fprintf(stderr, "%s:%d I: %s: " fmt "\n", __FILE__, __LINE__, tag, ##args)
#define ESP_LOGW(tag, fmt, args...) fprintf(stderr, "%s:%d W: %s: " fmt "\n", __FILE__, __LINE__, tag, ##args)
#define ESP_LOGE(tag, fmt, args...) fprintf(stderr, "%s:%d E: %s: " fmt "\n", __FILE__, __LINE__, tag, ##args)
#define ESP_LOG_BUFFER_HEX_LEVEL(tag, bytes, size, level)

typedef enum {
    ESP_LOG_NONE,       /*!< No log output */
    ESP_LOG_ERROR,      /*!< Critical errors, software module can not recover on its own */
    ESP_LOG_WARN,       /*!< Error conditions from which recovery measures have been taken */
    ESP_LOG_INFO,       /*!< Information messages which describe normal flow of events */
    ESP_LOG_DEBUG,      /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
    ESP_LOG_VERBOSE     /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */
} esp_log_level_t;

static inline uint32_t esp_log_timestamp() { return time(NULL); }

#endif  // FAKE_ESP_IDF

#endif  // ESPCXX_LOGGING_H_
