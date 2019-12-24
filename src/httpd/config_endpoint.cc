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
  unique_cJSON_ptr body(cJSON_Parse(request.body().data()));
  unique_cJSON_ptr prefix(cJSON_DetachItemFromObjectCaseSensitive(
          body.get(), "prefix"));
  if (!prefix || !cJSON_IsString(prefix.get())) {
    response.SendError(400, "Need prefix string");
    return;
  }

  unique_cJSON_ptr data(cJSON_DetachItemFromObjectCaseSensitive(
          body.get(), "data"));
  if (!data) {
    response.SendError(400, "Missing data");
    return;
  }

  ESP_LOGI(kEspCxxTag, "Got %.*s\n", request.body().size(), request.body().data());
  if (!cJSON_IsArray(data.get())) {
    response.SendError(400, "Expecting array of config values\0");
    return;
  }

  std::string result = "{";
  cJSON *entry;
  cJSON_ArrayForEach(entry, data.get()) {
    // NvsFlash can only handle 15 bytes including null.
    if (cJSON_IsString(entry) && strlen(entry->string) < 15) {
      result += "{\"";
      result += entry->string;
      result + "\":\"";
      if (request.method() == HttpMethod::kPost) {
        config_store_.SetValue(prefix->valuestring, entry->string, entry->valuestring);
        result += entry->valuestring;
      } else {
        result += config_store_.GetValue(prefix->valuestring, entry->string).value_or(std::string("\"\""));
      }
      result += "\"},";
    }
    
    /*
     */
    cJSON* key = cJSON_GetObjectItemCaseSensitive(entry, "k");
    cJSON* data = cJSON_GetObjectItemCaseSensitive(entry, "d");
    if (cJSON_IsString(key) && strlen(key->valuestring) < 15) {
      result += "{\"k\":\"";
      result += key->valuestring;
      result += "\",\"d\":\"";
      if (request.method() == HttpMethod::kPost) {
        if (cJSON_IsString(data)) {
          config_store_.SetValue(prefix->valuestring, key->valuestring, data->valuestring);
          result += data->valuestring;
        } else {
          esp_cxx::unique_C_ptr<char> data_str = PrintJson(data);
          config_store_.SetValue(prefix->valuestring, key->valuestring, data_str.get());
          result += data_str.get();
        }
      } else {
        result += config_store_.GetValue(prefix->valuestring, key->valuestring).value_or(std::string("\"\""));
      }
      result += "\"},";
    }
  }

  if (result.back() == ',') {
    result.pop_back();
  }
  result += "}";

  // TODO(awong): Set the content type.
  response.Send(200, result.size(), nullptr, result);
}

}  // namespace esp_cxx
