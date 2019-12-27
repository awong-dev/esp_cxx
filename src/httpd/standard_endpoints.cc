#include "esp_cxx/httpd/standard_endpoints.h"

#include <string>

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/wifi.h"

#ifndef FAKE_ESP_IDF
#include "driver/gpio.h"
#endif 

#include "jsmn.h"

namespace esp_cxx {

void StandardEndpoints::RegisterEndpoints(HttpServer* server) {
  server->RegisterEndpoint("/$", index_endpoint());
  server->RegisterEndpoint<&ResetEndpoint>("/api/reset$");
  server->RegisterEndpoint<&StatsEndpoint>("/api/stats$");

  server->RegisterEndpoint("/api/config$", config_endpoint());
  server->RegisterEndpoint("/api/ota$", ota_endpoint());

  server->EnableWebsockets();
  server->RegisterEndpoint("/api/logz$", log_stream_endpoint());
}

void StandardEndpoints::ResetEndpoint(HttpRequest request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet ||
      request.method() == HttpMethod::kPost) {
#ifndef FAKE_ESP_IDF
    esp_restart();
#endif
  }
}

void StandardEndpoints::StatsEndpoint(HttpRequest request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet) {
    unique_cJSON_ptr stats(cJSON_CreateObject());

    cJSON_AddNumberToObject(stats.get(), "free_heap_bytes", xPortGetFreeHeapSize());
    cJSON_AddNumberToObject(stats.get(), "uptime_us", esp_timer_get_time());

    auto result = PrintJson(stats.get());
    response.Send(200, strlen(result.get()), HttpResponse::kContentTypeJson, result.get());
  } else {
    response.SendError(400);
  }
}

}  // namespace esp_cxx

