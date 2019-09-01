#include "esp_cxx/httpd/websocket.h"

#include "esp_cxx/httpd/mongoose_event_manager.h"

#include "esp_cxx/logging.h"

namespace esp_cxx {

WebsocketChannel::WebsocketChannel(MongooseEventManager* event_manager,
                                   const std::string& ws_url,
                                   std::function<void(WebsocketFrame)> on_frame_cb)
  : event_manager_(event_manager),
    ws_url_(ws_url),
    on_frame_cb_(std::move(on_frame_cb)) {
}

WebsocketChannel::~WebsocketChannel() {
  // TODO(awong): Will this UAF an attempt to dispatch a CLOSE message?
  Disconnect();
}

bool WebsocketChannel::Connect() {
  ESP_LOGI(kEspCxxTag, "Websocket connecting to %s", ws_url_.c_str());
  connection_ = mg_connect_ws(event_manager_->underlying_manager(),
                              &WebsocketChannel::OnWsEventThunk,
                              this, ws_url_.c_str(), NULL, NULL);
  return !!connection_;
}

void WebsocketChannel::Disconnect() {
  if (connection_) {
    mg_send_websocket_frame(connection_, WEBSOCKET_OP_CLOSE, "", 0);
    connection_ = nullptr;
  }
}

void WebsocketChannel::SendText(std::string_view text) {
  if (connection_) {
    mg_send_websocket_frame(connection_, WEBSOCKET_OP_TEXT, text.data(), text.size());
  }
}

void WebsocketChannel::OnWsEvent(mg_connection *new_connection, int event, websocket_message *ev_data) {
  switch (event) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        ESP_LOGW(kEspCxxTag, "WS Connect error: %d", status);
      } else {
        ESP_LOGI(kEspCxxTag, "WS Connect success");
      }
      break;
    }

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
      // TODO(awong): Does there need to be a timeout on WS handshake failure?
      // Otherwise do nothing.
      break;

    case MG_EV_WEBSOCKET_FRAME:
      // Mongoose already handles merging fragmented messages. Thus a received
      // frame in mongoose IS a complete message. Pass it straight along.
      on_frame_cb_(WebsocketFrame(static_cast<websocket_message*>(ev_data)));
      break;

    case MG_EV_CLOSE:
      connection_ = nullptr;
      ESP_LOGI(kEspCxxTag, "WS Connect closing");
      break;
  }
}

void WebsocketChannel::OnWsEventThunk(mg_connection *new_connection, int event,
                                      void *ev_data, void *user_data) {
  static_cast<WebsocketChannel*>(user_data)->OnWsEvent(new_connection, event,
                                                       static_cast<websocket_message*>(ev_data));
}

}  // namespace esp_cxx
