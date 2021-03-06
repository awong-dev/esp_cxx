#ifndef ESPCXX_FIREBASE_FIREBASE_CONFIG_H_
#define ESPCXX_FIREBASE_FIREBASE_CONFIG_H_

#include <string>
#include "esp_cxx/config_store.h"

namespace esp_cxx {

class FirebaseConfig {
 public:
  void Load(ConfigStore* store);
  void Save(ConfigStore* store);

  const std::string& host() const { return host_; }
  void set_host(const std::string& host) { host_ = host; }

  const std::string& database() const { return database_; }
  void set_database(const std::string& database) { database_ = database; }

  const std::string& listen_path() const { return listen_path_; }
  void set_listen_path(const std::string& listen_path) { listen_path_ = listen_path; }

  const std::string& auth_token_url() const { return auth_token_url_; }
  void set_auth_token_url(const std::string& auth_token_url) { auth_token_url_ = auth_token_url; }

  const std::string& device_id() const { return device_id_; }
  void set_device_id(const std::string& device_id) { device_id_ = device_id; }

  const std::string& secret() const { return secret_; }
  void set_secret(const std::string& secret) { secret_ = secret; }

 private:
  std::string host_;
  std::string database_;
  std::string listen_path_;

  std::string auth_token_url_;
  std::string device_id_;
  std::string secret_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_FIREBASE_FIREBASE_CONFIG_H_
