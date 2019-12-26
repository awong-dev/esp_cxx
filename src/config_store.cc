#include "esp_cxx/config_store.h"

namespace {

constexpr size_t kMaxKeySize = 16;

}  // namespace

namespace esp_cxx {

const char ConfigStore::kNvsNamespace[] = "config";

void ConfigStore::SetValue(std::string_view prefix, std::string_view key, std::string_view value) {
  char buf[kMaxKeySize];
  snprintf(&buf[0], sizeof(buf), "%.*s:%.*s", prefix.size(), prefix.data(), key.size(), key.data());
  nvs_handle_.SetString(buf, value.data());
}

std::optional<std::string> ConfigStore::GetValue(std::string_view prefix, std::string_view key) {
  char buf[kMaxKeySize];
  snprintf(&buf[0], sizeof(buf), "%.*s:%.*s", prefix.size(), prefix.data(), key.size(), key.data());
  return nvs_handle_.GetString(buf);
}

unique_cJSON_ptr ConfigStore::GetAllValues() {
  unique_cJSON_ptr values(cJSON_CreateObject());
  nvs_iterator_t it = nvs_entry_find(NVS_DEFAULT_PART_NAME, kNvsNamespace,
                                     NVS_TYPE_STR);
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    it = nvs_entry_next(it);
    cJSON_AddStringToObject(values.get(), info.key,
                            nvs_handle_.GetString(info.key).value().c_str());
  };

  return values;
}

}  // namespace esp_cxx
