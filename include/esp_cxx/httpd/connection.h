#ifndef ESPCXX_HTTPD_CONNECTION_H_
#define ESPCXX_HTTPD_CONNECTION_H_

#include <functional>
#include <string>

#include "esp_cxx/cxx17hack.h"

struct mg_connection;

namespace esp_cxx {

class MongooseEventManager;

class Connection {
 public:
  Connection() = default;
  explicit Connection(MongooseEventManager *event_manager,
                     std::function<void(std::string_view)> on_packet);
  ~Connection();

  // udp URL to connect to. Should be of form udp://[hostname]:{port}.
  // Examples:
  //   udp://1234  # port 1234 of localhost
  //   udp://123.4.5.1:1234  # port 1234 of 123.4.5.1
  bool Connect(const std::string& udp_url);
  void Send(std::string_view data);

 private:
  static void OnEventThunk(struct mg_connection *nc,
                           int event,
                           void *event_data,
                           void *user_data);

  void OnEvent(struct mg_connection *nc, int event, void *event_data);

  mg_connection* connection_ = nullptr;

  // Event manager used to initiate connections.
  MongooseEventManager* event_manager_ = nullptr;

  // Callback that is executed when data is received.
  std::function<void(std::string_view)> on_packet_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_CONNECTION_H_

