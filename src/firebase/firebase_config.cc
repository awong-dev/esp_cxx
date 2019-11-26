#include "esp_cxx/firebase/firebase_config.h"

#include "esp_cxx/httpd/config_endpoint.h"

namespace esp_cxx {

void FirebaseConfig::Load() {
  NvsHandle handle(ConfigEndpoint::kNvsNamespace, NvsHandle::Mode::kReadOnly);
  host_ = handle.GetString("fb:host").value_or(std::string());
  database_ = handle.GetString("fb:database").value_or(std::string());
  listen_path_ = handle.GetString("fb:listen_path").value_or(std::string());
  auth_token_url_ = handle.GetString("fb:auth_token_url").value_or(std::string());
  device_id_ = handle.GetString("fb:device_id").value_or(std::string());
  password_ = handle.GetString("fb:password").value_or(std::string());
}

void FirebaseConfig::Save() {
  NvsHandle handle(ConfigEndpoint::kNvsNamespace, NvsHandle::Mode::kReadWrite);
  handle.SetString("fb:host", host_);
  handle.SetString("fb:database", database_);
  handle.SetString("fb:listen_path", listen_path_);
  handle.SetString("fb:auth_token_url", auth_token_url_);
  handle.SetString("fb:device_id", device_id_);
  handle.SetString("fb:password", password_);
}

}  // namespace esp_cxx
