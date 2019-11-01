#include "esp_cxx/httpd/config_endpoint.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/cpointer.h"

namespace esp_cxx {

void ConfigEndpoint::OnHttp(HttpRequest request, HttpResponse response) {
  if (request.method() != HttpMethod::kGet &&
      request.method() != HttpMethod::kPost) {
    response.SendError(400, "Invalid Method");
    return;
  }
  unique_cJSON_ptr json(cJSON_Parse(request.body().data()));
  unique_cJSON_ptr key(
      cJSON_DetachItemFromObjectCaseSensitive(json.get(), "_key"));
  if (!cJSON_IsString(key.get())) {
    response.SendError(400, "Missing _key");
    return;
  }

  if (request.method() == HttpMethod::kPost) {
    auto value = PrintJson(json.get());
    nvs_handle_.SetString(key->valuestring, value.get());
    response.Send(200, strlen(value.get()), nullptr, value.get());
    return;
  }

  // Default treat things like a GET.
  std::string value = nvs_handle_.GetString(key->valuestring).value_or(std::string());
  response.Send(200, value.size(), nullptr, value);
}

}  // namespace esp_cxx
