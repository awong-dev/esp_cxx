#include "esp_cxx/firebase/firebase_config.h"

#include "esp_cxx/httpd/config_endpoint.h"

namespace {
constexpr char kFirebasePrefix[] = "fb";
}  // namespace

namespace esp_cxx {

void FirebaseConfig::Load() {
  ConfigStore config_store;
  host_ = config_store.GetValue(kFirebasePrefix, "host").value_or(std::string());
  database_ = config_store.GetValue(kFirebasePrefix, "database").value_or(std::string());
  listen_path_ = config_store.GetValue(kFirebasePrefix, "path").value_or(std::string());
  auth_token_url_ = config_store.GetValue(kFirebasePrefix, "auth_url").value_or(std::string());
  device_id_ = config_store.GetValue(kFirebasePrefix, "device_id").value_or(std::string());
  secret_ = config_store.GetValue(kFirebasePrefix, "secret").value_or(std::string());
}

void FirebaseConfig::Save() {
  ConfigStore config_store;
  config_store.SetValue(kFirebasePrefix, "host", host_);
  config_store.SetValue(kFirebasePrefix, "database", database_);
  config_store.SetValue(kFirebasePrefix, "path", listen_path_);
  config_store.SetValue(kFirebasePrefix, "auth_url", auth_token_url_);
  config_store.SetValue(kFirebasePrefix, "device_id", device_id_);
  config_store.SetValue(kFirebasePrefix, "secret", secret_);
}

}  // namespace esp_cxx
