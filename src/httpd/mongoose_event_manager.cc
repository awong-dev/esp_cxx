#include "esp_cxx/httpd/mongoose_event_manager.h"

#include <chrono>
#include <memory>

#include "esp_cxx/event_manager.h"
#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/logging.h"

namespace esp_cxx {

namespace {
void DoNothing(mg_connection* nc, int ev, void* ev_data, void* user_data) {
}

class HttpRequestAdaptor {
 public:
  using HandlerType = std::function<void(HttpRequest)>;

  explicit HttpRequestAdaptor(HandlerType handler) : handler_(std::move(handler)) {}

  static void HandleThunk(struct mg_connection *c, int ev, void *p, void* user_data) {
    HttpRequest request;
    HttpRequestAdaptor* adaptor = static_cast<HttpRequestAdaptor*>(user_data);

    // The only events that come in are MG_EV_HTTP_REPLY and MG_EV_CLOSE. Only send
    // the data on MG_EV_HTTP_REPLY. MG_EV_CLOSE is used for cleanup.
    if (ev == MG_EV_HTTP_REPLY) {
      struct http_message *hm = (struct http_message *)p;
      request = HttpRequest(hm);
      c->flags |= MG_F_CLOSE_IMMEDIATELY;
      adaptor->handler_(request);
    } else if (ev == MG_EV_CLOSE) {
      delete adaptor;
    }
  }

 private:
  HandlerType handler_;
};

}  // namespace

MongooseEventManager::MongooseEventManager()
  : signaling_task_(&MongooseEventManager::SignalTask, this, "signal", 8192) {
  mg_mgr_init(&underlying_manager_, this);
}

MongooseEventManager::~MongooseEventManager() {
}

void MongooseEventManager::HttpConnect(std::function<void(HttpRequest)> handler,
                                       const std::string& uri,
                                       const char* extra_headers,
                                       const char* post_data) {
  ESP_LOGI(kEspCxxTag, "HttpConnect: %s", uri.c_str());
  auto adaptor = std::make_unique<HttpRequestAdaptor>(std::move(handler));
  mg_connect_http(underlying_manager(), &HttpRequestAdaptor::HandleThunk,
                  adaptor.release(), uri.c_str(), extra_headers, post_data);
}

void MongooseEventManager::Poll(int timeout_ms) {
  mg_mgr_poll(underlying_manager(), timeout_ms);
}

void MongooseEventManager::Wake() {
  // Warning: This function must be FAST. Otherwise, signaling from things
  // wifi-handlers may trigger a watchdog timer to expire. This means no
  // logging, no attempts to write to sockets, etc.
  if (!executing_task_.is_current()) {
    signaling_task_.Notify();
  }
}

// static
void MongooseEventManager::SignalTask(void* param) {
 static char dummy;
  MongooseEventManager* self = reinterpret_cast<MongooseEventManager*>(param);
  TaskRef current_task = TaskRef::CreateForCurrent();
  while (1) {
    current_task.Wait();
    mg_broadcast(self->underlying_manager(), &DoNothing, &dummy, sizeof(dummy));
  }
}

}  // namespace esp_cxx

