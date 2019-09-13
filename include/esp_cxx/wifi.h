#ifndef ESPCXX_WIFI_H_
#define ESPCXX_WIFI_H_

#include <string>
#include <functional>

#include "esp_cxx/cxx17hack.h"

#ifndef FAKE_ESP_IDF
#include "esp_event.h"
#endif

namespace esp_cxx {

class Wifi {
 public:
  static constexpr size_t kSsidBytes = 32;
  static constexpr size_t kPasswordBytes = 64;
  
  static Wifi* GetInstance();

  // Set and get the ssid/password from Nvs.
  static std::optional<std::string> GetSsid();
  static std::optional<std::string> GetPassword();

  // The passed in string_views MUST be null terminated and smaller than
  // kSsidBytes and kPasswordBytes respecitvely.
  static void SetSsid(const std::string& ssid);
  static void SetPassword(const std::string& password);

  // Uses the loaded Ssid and Password to connect to an AP.
  // Returns false if those values are not set or some unexpected
  // error occurs. See also SetApEventHandlers().
  //
  // Will attempt to reconnect if network is disconencted.
  //
  // This is mutually exclusive with CreateSetupNetwork().
  bool ConnectToAP();
  void ReconnectToAP();

  // Set handlers for events associated with connecting to an access point.
  //
  // |on_ap_connect| is called when the WIFI connection is established.
  // |on_ap_disconnect| is called when the WIFI connection is lost.
  void SetApEventHandlers(
      std::function<void(ip_event_got_ip_t*)> on_ap_connect,
      std::function<void(uint8_t)> on_ap_disconnect);

  // Creates a new ssid network that clients can connect to for
  // configurating this device.
  //
  // This is mutually exclusive with ConnectToAP().
  bool CreateSetupNetwork(const std::string& setup_ssid,
                          const std::string& setup_password);

  // Stops wifi system after either a ConnectToAP() or CreateSetupNetwork()
  // call.
  void Disconnect();

 private:
  Wifi();
  ~Wifi();

#ifndef FAKE_ESP_IDF
  static void EventHandlerThunk(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

  void OnWifiEvent(esp_event_base_t event_base, int32_t event_id,
                   void* event_data);

  void WifiConnect(const wifi_config_t& wifi_config, bool is_station);
#endif

  std::function<void(ip_event_got_ip_t*)> on_ap_connect_;
  std::function<void(uint8_t)> on_ap_disconnect_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_WIFI_H_
