#include "esp_cxx/wifi.h"

#include <assert.h>
#include <string.h>

#include "esp_cxx/logging.h"
#include "esp_cxx/nvs_handle.h"

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#endif

namespace esp_cxx {

#ifndef FAKE_ESP_IDF
#define fldsiz(name, field) (sizeof(((name *)0)->field))
static_assert(Wifi::kSsidBytes == fldsiz(wifi_config_t, sta.ssid),
              "Ssid field size changed");
static_assert(Wifi::kPasswordBytes == fldsiz(wifi_config_t, sta.password),
              "Password field size changed");
#undef fldsize
#endif  // FAKE_ESP_IDF

namespace {

constexpr char kSsidNvsKey[] = "ssid";
constexpr char kPasswordNvsKey[] = "password";

}  // namespace

Wifi::Wifi() = default;

Wifi::~Wifi() {
  Disconnect();
}

Wifi* Wifi::GetInstance() {
  static Wifi wifi;
  return &wifi;
}

std::optional<std::string> Wifi::GetSsid() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadOnly);
  return nvs_wifi_config.GetString(kSsidNvsKey);
}

std::optional<std::string> Wifi::GetPassword() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadOnly);
  return nvs_wifi_config.GetString(kPasswordNvsKey);
}

void Wifi::SetSsid(const std::string& ssid) {
  assert(ssid.size() <= kSsidBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadWrite);

  ESP_LOGD(kEspCxxTag, "Writing ssid: %s", ssid.c_str());
  nvs_wifi_config.SetString(kSsidNvsKey, ssid);
}

void Wifi::SetPassword(const std::string& password) {
  assert(password.size() < kPasswordBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadWrite);

  ESP_LOGD(kEspCxxTag, "Writing password: %s", password.c_str());
  nvs_wifi_config.SetString(kPasswordNvsKey, password);
}

void Wifi::SetApEventHandlers(
    std::function<void(ip_event_got_ip_t*)> on_ap_connect,
    std::function<void(uint8_t)> on_ap_disconnect) {
  on_ap_connect_ = std::move(on_ap_connect);
  on_ap_disconnect_ = std::move(on_ap_disconnect);
}
bool Wifi::ConnectToAP() {
#ifndef FAKE_ESP_IDF
  wifi_config_t wifi_config = {};

  std::string ssid = GetSsid().value_or(std::string());
  std::string password = GetPassword().value_or(std::string());
  if (ssid.empty() || password.empty() ||
      ssid.size() > sizeof(wifi_config.sta.ssid) ||
      password.size() > sizeof(wifi_config.sta.password)) {
    ESP_LOGE(kEspCxxTag,
             "Stored Ssid or Password has invalid length. Resetting to "
             "empty which may end up here again.");
    SetSsid("");
    SetPassword("");
    return false;
  }

  ESP_LOGW(kEspCxxTag, "Got config ssid: %s password: %s",
           ssid.c_str(), password.c_str());
  strcpy((char*)&wifi_config.sta.ssid[0], ssid.c_str());
  strcpy((char*)&wifi_config.sta.password[0], password.c_str());

  WifiConnect(wifi_config, true);
#endif
  return true;
}

void Wifi::ReconnectToAP() {
#ifndef FAKE_ESP_IDF
  esp_wifi_connect();
#endif
}

bool Wifi::CreateSetupNetwork(const std::string& setup_ssid,
                              const std::string& setup_password) {
#ifndef FAKE_ESP_IDF
  wifi_config_t wifi_config = {};

  if (setup_ssid.empty() || // Allow empty password.
      setup_ssid.size() > sizeof(wifi_config.ap.ssid) ||
      setup_password.size() > sizeof(wifi_config.ap.password)) {
    ESP_LOGE(kEspCxxTag, "Setup Ssid or Password has invalid length");
    return false;
  }

  // TODO(awong): Don't forget to set country.
  strcpy((char*)&wifi_config.ap.ssid[0], setup_ssid.c_str());
  strcpy((char*)&wifi_config.ap.password[0], setup_password.c_str());

  // Assume null termination always since c_str() is used.
  wifi_config.ap.ssid_len = 0;

  if (!setup_password.empty()) {
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.beacon_interval = 100;

  ESP_LOGW(kEspCxxTag, "Creating setup network at ssid: %s password: %s",
           setup_ssid.c_str(), setup_password.c_str());
  WifiConnect(wifi_config, false);
#endif  // FAKE_ESP_IDF
  return true;
}

void Wifi::Disconnect() {
#ifndef FAKE_ESP_IDF
  esp_wifi_stop();
#endif  // FAKE_ESP_IDF
}

#ifndef FAKE_ESP_IDF

void Wifi::EventHandlerThunk(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
  Wifi* wifi = reinterpret_cast<Wifi*>(arg);
  wifi->OnWifiEvent(event_base, event_id, event_data);
}

void Wifi::OnWifiEvent(esp_event_base_t event_base, int32_t event_id,
                       void* event_data) {
  if (event_base == WIFI_EVENT) {
    switch(event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(kEspCxxTag, "STA_START.");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t* disconnected_info =
            reinterpret_cast<wifi_event_sta_disconnected_t*>(event_data);
        ESP_LOGI(kEspCxxTag, "STA_DISCONNECTED. %d", disconnected_info->reason);
        if (on_ap_disconnect_) {
          on_ap_disconnect_(disconnected_info->reason);
        }
        break;
      }

      case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t *ap_connected =
            reinterpret_cast<wifi_event_ap_staconnected_t*>(event_data);
        ESP_LOGI(kEspCxxTag, "station:" MACSTR " join, AID=%d",
                 MAC2STR(ap_connected->mac), ap_connected->aid);
        break;
      }
      case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t *ap_disconnected =
            reinterpret_cast<wifi_event_ap_stadisconnected_t*>(event_data);
        ESP_LOGI(kEspCxxTag, "station:" MACSTR "leave, AID=%d",
                 MAC2STR(ap_disconnected->mac), ap_disconnected->aid);
        break;
      }
      default:
        break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *got_ip =
        reinterpret_cast<ip_event_got_ip_t*>(event_data);
    ESP_LOGI(kEspCxxTag, "got ip:%s", ip4addr_ntoa(&got_ip->ip_info.ip));
    if (on_ap_connect_) {
      on_ap_connect_(got_ip);
    }
  }
}

void Wifi::WifiConnect(const wifi_config_t& wifi_config, bool is_station) {
  tcpip_adapter_init();

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &EventHandlerThunk, this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &EventHandlerThunk, this));

  if (is_station) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  } else {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  }
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(kEspCxxTag, "wifi_init finished.");
  ESP_LOGI(kEspCxxTag, "%s SSID:%s password:%s",
           is_station ? "connect to ap" : "created network",
           wifi_config.sta.ssid, wifi_config.sta.password);
}

#endif  // FAKE_ESP_IDF

}  // namespace esp_cxx
