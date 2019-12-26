#include "esp_cxx/httpd/config_endpoint.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/cpointer.h"

namespace esp_cxx {

void ConfigEndpoint::OnHttp(HttpRequest request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet) {
    OnGet(request, response);
  } else if (request.method() == HttpMethod::kPost) {
    OnPost(request, response);
  } else {
    response.SendError(400, "Invalid Method");
  }
}

void ConfigEndpoint::OnGet(HttpRequest request, HttpResponse response) {
  auto result_str = PrintJson(config_store_.GetAllValues().get());
  response.Send(200, strlen(result_str.get()), HttpResponse::kContentTypeJson,
                result_str.get());
}

void ConfigEndpoint::OnPost(HttpRequest request, HttpResponse response) {
  ESP_LOGI(kEspCxxTag, "Config got %.*s\n", request.body().size(), request.body().data());
  unique_cJSON_ptr body(cJSON_Parse(request.body().data()));
  unique_cJSON_ptr prefix(cJSON_DetachItemFromObjectCaseSensitive(
          body.get(), "prefix"));
  if (!prefix || !cJSON_IsString(prefix.get())) {
    response.SendError(400, "Need prefix string");
    return;
  }

  ESP_LOGI(kEspCxxTag, "prefix found: %s", prefix->valuestring);
  unique_cJSON_ptr config_data(cJSON_DetachItemFromObjectCaseSensitive(
          body.get(), "config_data"));
  if (!config_data) {
    response.SendError(400, "Missing data");
    return;
  }

  ESP_LOGI(kEspCxxTag, "Data found");
  if (!cJSON_IsObject(config_data.get())) {
    response.SendError(400, "Expecting object of values");
    return;
  }

  int prefix_len = strlen(prefix->valuestring) + 1;
  unique_cJSON_ptr result(cJSON_CreateObject());
  cJSON *entry;
  cJSON_ArrayForEach(entry, config_data.get()) {
    // NvsFlash can only handle 15 bytes including null.
    if ((strlen(entry->string) + prefix_len) < 15) {
      char full_key[15];
      snprintf(&full_key[0], sizeof(full_key), "%s:%s", prefix->valuestring, entry->string);

      if (cJSON_IsString(entry)) {
        ESP_LOGI(kEspCxxTag, "Set %s = %s", full_key, entry->valuestring);
        cJSON_AddStringToObject(result.get(), full_key, entry->valuestring);
        config_store_.SetValue(prefix->valuestring, entry->string, entry->valuestring);
      }
    }
  }

  auto result_str = PrintJson(result.get());
  response.Send(200, strlen(result_str.get()), HttpResponse::kContentTypeJson,
                result_str.get());
}

}  // namespace esp_cxx
