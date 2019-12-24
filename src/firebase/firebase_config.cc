#include "esp_cxx/firebase/firebase_config.h"

#include "esp_cxx/httpd/config_endpoint.h"

namespace {
constexpr char kFirebasePrefix[] = "fb";
}  // namespace

namespace esp_cxx {

void FirebaseConfig::Load(ConfigStore* store) {
  host_ = store->GetValue(kFirebasePrefix, "host").value_or(std::string());
  database_ = store->GetValue(kFirebasePrefix, "database").value_or(std::string());
  listen_path_ = store->GetValue(kFirebasePrefix, "path").value_or(std::string());
  auth_token_url_ = store->GetValue(kFirebasePrefix, "auth_url").value_or(std::string());
  device_id_ = store->GetValue(kFirebasePrefix, "device_id").value_or(std::string());
  secret_ = store->GetValue(kFirebasePrefix, "secret").value_or(std::string());
}

void FirebaseConfig::Save(ConfigStore* store) {
  store->SetValue(kFirebasePrefix, "host", host_);
  store->SetValue(kFirebasePrefix, "database", database_);
  store->SetValue(kFirebasePrefix, "path", listen_path_);
  store->SetValue(kFirebasePrefix, "auth_url", auth_token_url_);
  store->SetValue(kFirebasePrefix, "device_id", device_id_);
  store->SetValue(kFirebasePrefix, "secret", secret_);
}

}  // namespace esp_cxx
