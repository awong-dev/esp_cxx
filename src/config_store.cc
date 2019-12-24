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

}  // namespace esp_cxx
