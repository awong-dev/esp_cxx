#ifndef ESPCXX_HTTPD_EVENT_MANAGER_H_
#define ESPCXX_HTTPD_EVENT_MANAGER_H_

#include <array>
#include <functional>

#include "esp_cxx/event_manager.h"
#include "esp_cxx/task.h"
#include "mongoose.h"

namespace esp_cxx {

class HttpRequest;

class MongooseEventManager : public EventManager {
 public:
  MongooseEventManager();
  ~MongooseEventManager() override;

  // Makes an http connection and asynchronously sends the result to |handler|
  void HttpConnect(std::function<void(HttpRequest)> handler,
                   const std::string& uri,
                   const char* extra_headers = nullptr,
                   const char* post_data = nullptr);

  mg_mgr* underlying_manager() { return &underlying_manager_; }

  void Wake() override;

 private:
  void Poll(int timeout_ms) override;

  // See |signaling_task_| below.
  static void SignalTask(void* param);

  mg_mgr underlying_manager_;
  TaskRef executing_task_ = TaskRef::CreateForCurrent();

  // Calling mg_broadcast() from Wake() in various contexts such as wifi
  // events can deadlock the LWIP stack. Use a separate thread to bounce
  // the wake message off of in order to avoid the deadlock.
  Task signaling_task_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_EVENT_MANAGER_H_
