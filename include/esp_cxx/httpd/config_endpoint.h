#ifndef ESPCXX_HTTPD_CONFIG_ENDPOINT_H_
#define ESPCXX_HTTPD_CONFIG_ENDPOINT_H_

#include "esp_cxx/httpd/http_server.h"

#include "esp_cxx/nvs_handle.h"

namespace esp_cxx {

class ConfigEndpoint : public HttpServer::Endpoint {
 public:
  virtual void OnHttp(HttpRequest request, HttpResponse response);
  static const char kNvsNamespace[];

 private:
  NvsHandle nvs_handle_{kNvsNamespace, NvsHandle::Mode::kReadWrite};
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_CONFIG_ENDPOINT_H_
