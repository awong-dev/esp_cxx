#ifndef ESPCXX_HTTPD_CONFIG_ENDPOINT_H_
#define ESPCXX_HTTPD_CONFIG_ENDPOINT_H_

#include "esp_cxx/httpd/http_server.h"

#include "esp_cxx/config_store.h"

namespace esp_cxx {

class ConfigEndpoint : public HttpServer::Endpoint {
 public:
  void OnHttp(HttpRequest request, HttpResponse response) override;

 private:
  void OnGet(HttpRequest request, HttpResponse response);
  void OnPost(HttpRequest request, HttpResponse response);

  ConfigStore config_store_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_CONFIG_ENDPOINT_H_
