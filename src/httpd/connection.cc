#include "esp_cxx/httpd/connection.h"

#include "esp_cxx/httpd/mongoose_event_manager.h"

#include "esp_cxx/logging.h"

namespace esp_cxx {

Connection::Connection(MongooseEventManager *event_manager,
                       std::function<void(std::string_view)> on_packet)
  : event_manager_(event_manager),
    on_packet_(std::move(on_packet)) {
}

Connection::~Connection() {
  if (connection_) {
    connection_->flags |= MG_F_CLOSE_IMMEDIATELY;
  }
}

// static
void Connection::OnEventThunk(struct mg_connection *nc, int event,
                              void *event_data, void *user_data) {
  Connection* self = static_cast<Connection*>(user_data);
  self->OnEvent(nc, event, event_data);
}

bool Connection::Connect(const std::string& udp_url) {
  connection_ = mg_connect(event_manager_->underlying_manager(),
                           udp_url.c_str(), &OnEventThunk, this);
  return !!connection_;
}

void Connection::Send(std::string_view data) {
  if (!connection_) {
    ESP_LOGW(kEspCxxTag, "UDP Connetion failed. droping data");
    return;
  }

  mg_send(connection_, data.data(), data.size());
}

void Connection::OnEvent(struct mg_connection *nc, int event, void *ev_data) {
  switch (event) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        ESP_LOGW(kEspCxxTag, "UDP Connect error: %d", status);
      }
      break;
    }

    case MG_EV_RECV: {
      if (on_packet_) {
        on_packet_({nc->recv_mbuf.buf, nc->recv_mbuf.len});
      }
      break;
    }

    case MG_EV_CLOSE: {
      connection_ = nullptr;
      break;
    }
  }
}

}  // namespace esp_cxx
