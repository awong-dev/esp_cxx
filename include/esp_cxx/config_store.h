#ifndef CONFIG_STORE_H_
#define CONFIG_STORE_H_

#include "esp_cxx/cpointer.h"
#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/nvs_handle.h"

namespace esp_cxx {

class ConfigStore {
 public:
  void SetValue(std::string_view prefix, std::string_view key, std::string_view value);
  std::optional<std::string> GetValue(std::string_view prefix, std::string_view key);
  unique_cJSON_ptr GetAllValues();

 private:
  static const char kNvsNamespace[];
  NvsHandle nvs_handle_{kNvsNamespace, NvsHandle::Mode::kReadWrite};
};

}  // namespace esp_cxx

#endif  // CONFIG_STORE_H_
