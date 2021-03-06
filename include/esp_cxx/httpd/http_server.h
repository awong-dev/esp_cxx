#ifndef ESPCXX_HTTPD_HTTP_SERVER_H_
#define ESPCXX_HTTPD_HTTP_SERVER_H_

#include "mongoose.h"

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"
#include "esp_cxx/httpd/http_multipart.h"
#include "esp_cxx/httpd/websocket.h"
#include "esp_cxx/task.h"

namespace esp_cxx {

class MongooseEventManager;

class HttpServer {
 public:
  HttpServer(MongooseEventManager* event_manager,
             std::string_view resp404_html = {});
  ~HttpServer();

  class Endpoint {
   public:
    virtual ~Endpoint() = default;

    // A plain HTTP request has arrived.
    virtual void OnHttp(HttpRequest request, HttpResponse response) {}

    // TODO(awong): Support chunked http.

    // Multipart lifecycle events.
    // OnMultipartStart - client has requested a multipart upload.
    // OnMultipart - a segment of data from the multipart stream is available.
    //               The filename and varname are available for all related
    //               pieces of a chunk.
    virtual void OnMultipartStart(HttpRequest request, HttpResponse response) {}
    virtual void OnMultipart(HttpMultipart multipart, HttpResponse response) {}

    // Websocket lifecycle events.
    // OnWebsocketHandshake - client has requested a websocket connection.
    // OnWebsocketHandshakeComplete - client has completed handshake.
    // OnWebsocketFrame - websocket frame is received from client. Only called between
    //                    OnWebsocketHandshakeComplete() and OnWebsocketClosed().
    // OnWebsocketClosed - websocket connection closed. Always called once
    //                     after OnWebsocketHandshake().
    virtual void OnWebsocketHandshake(HttpRequest request, HttpResponse response) {}
    virtual void OnWebsocketHandshakeComplete(WebsocketSender sender) {}
    virtual void OnWebsocketFrame(WebsocketFrame frame, WebsocketSender sender) {}
    virtual void OnWebsocketClosed(WebsocketSender sender) {}

    static void OnHttpEventThunk(mg_connection *nc, int event,
                                 void *ev_data, void *user_data);
  };

  // Binds the port and starts listening.
  void Listen(const char* port);

  // Enables WebSocket events.
  void EnableWebsockets();

  // Adds an Endpoint handler for the given path_pattern.
  void RegisterEndpoint(const char* path_pattern, Endpoint* endpoint);

  // Conveience wrapper for adding simple http endpoint handler using a bare function.
  typedef void (*HttpCallback)(HttpRequest request, HttpResponse response);
  template <HttpCallback handler>
  void RegisterEndpoint(const char* path_pattern) {
    static struct : Endpoint {
      void OnHttp(HttpRequest request, HttpResponse response) {
        // TODO(awong): rename handler to be consistent with multipart_cb here and elsewhere.
        return handler(request, response);
      }
    } endpoint;
    // TODO(awong): Creating a whole Endpoint for just 1 function is heavyweight. Thin it down.
    RegisterEndpoint(path_pattern, &endpoint);
  }

 private:
  // Pumps events for the http server.
  void EventPumpRunLoop();

  // Default event handler for the bound port. Run if no other handler
  // intercepts first.
  static void DefaultHandlerThunk(struct mg_connection *nc,
                                  int event,
                                  void *event_data,
                                  void *user_data);

  // Document to return on a 404.
  std::string_view resp404_html_;

  // Event manager for all connections on this HTTP server.
  MongooseEventManager* event_manager_ = nullptr;

  // Bound connection to |port_|.
  mg_connection* connection_ = nullptr;

  // Handle of event pump task.
  Task pump_task_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_SERVER_H_
