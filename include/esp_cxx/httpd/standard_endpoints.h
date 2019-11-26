#ifndef ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_
#define ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

#include "esp_cxx/httpd/config_endpoint.h"
#include "esp_cxx/httpd/ota_endpoint.h"
#include "esp_cxx/httpd/log_stream_endpoint.h"

namespace esp_cxx {
template <const char content_type[]>
class StaticEndpoint : public HttpServer::Endpoint {
 public:
  explicit StaticEndpoint(std::string_view data)
    : data_(data) {
  }

  void OnHttp(HttpRequest request, HttpResponse response) override {
    response.Send(200, data_.size(), content_type, data_.data());
  }

 private:
  std::string_view data_;
};

using HtmlEndpoint = StaticEndpoint<HttpResponse::kContentTypeHtml>;
using JsonEndpoint = StaticEndpoint<HttpResponse::kContentTypeJson>;
using JsEndpoint = StaticEndpoint<HttpResponse::kContentTypeJs>;
using PlainEndpoint = StaticEndpoint<HttpResponse::kContentTypePlain>;

class StandardEndpoints {
 public:
  explicit StandardEndpoints(std::string_view index_html)
    : index_endpoint_(index_html) {
  }

  void RegisterEndpoints(HttpServer* server);

  ConfigEndpoint* config_endpoint() { return &config_endpoint_; }
  OtaEndpoint* ota_endpoint() { return &ota_endpoint_; }
  LogStreamEndpoint* log_stream_endpoint() { return &log_stream_endpoint_; }
  HtmlEndpoint* index_endpoint() { return &index_endpoint_; }

  // Stateless endpoints.
  static void ResetEndpoint(HttpRequest request, HttpResponse response);
  static void WifiConfigEndpoint(HttpRequest request, HttpResponse response);
  static void LedOnEndpoint(HttpRequest request, HttpResponse response);
  static void LedOffEndpoint(HttpRequest request, HttpResponse response);

 private:
  ConfigEndpoint config_endpoint_;
  OtaEndpoint ota_endpoint_;
  LogStreamEndpoint log_stream_endpoint_;
  HtmlEndpoint index_endpoint_;
};
}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

