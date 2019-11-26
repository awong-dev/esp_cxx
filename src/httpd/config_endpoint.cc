#include "esp_cxx/httpd/config_endpoint.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/cpointer.h"

namespace esp_cxx {

const char ConfigEndpoint::kNvsNamespace[] = "config";

void ConfigEndpoint::OnHttp(HttpRequest request, HttpResponse response) {
  if (request.method() != HttpMethod::kGet &&
      request.method() != HttpMethod::kPost) {
    response.SendError(400, "Invalid Method");
    return;
  }
  unique_cJSON_ptr json(cJSON_Parse(request.body().data()));
  if (!cJSON_IsArray(json.get())) {
    response.SendError(400, "Expecting array of config values");
    return;
  }

  std::string result = "[";
  cJSON *entry;
  cJSON_ArrayForEach(entry, json.get()) {
    cJSON* key = cJSON_GetObjectItemCaseSensitive(entry, "k");
    cJSON* data = cJSON_GetObjectItemCaseSensitive(entry, "d");
    if (cJSON_IsString(key)) {
      result += "{\"k\":";
      result += key->valuestring;
      result += ",\"d\":";
      if (request.method() == HttpMethod::kPost) {
        esp_cxx::unique_C_ptr<char> data_str = PrintJson(data);
        nvs_handle_.SetString(key->valuestring, data_str.get());
        result += data_str.get();
      } else {
        result += nvs_handle_.GetString(key->valuestring).value_or(std::string("\"\""));
      }
      result += "},";
    }
  }

  if (result.back() == ',') {
    result.pop_back();
  }
  result += "]";

  response.Send(200, result.size(), nullptr, result);
}

}  // namespace esp_cxx
