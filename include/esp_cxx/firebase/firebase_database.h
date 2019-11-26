#ifndef ESPCXX_FIREBASE_FIREBASE_DATABASE_H_
#define ESPCXX_FIREBASE_FIREBASE_DATABASE_H_

#include <functional>
#include <string>

#include "esp_cxx/backoff.h"
#include "esp_cxx/cpointer.h"
#include "esp_cxx/httpd/websocket.h"

#include "gtest/gtest_prod.h"

struct cJSON;

namespace esp_cxx {
class MongooseEventManager;
class HttpRequest;

class FirebaseDatabase {
 public:
  explicit FirebaseDatabase(MongooseEventManager* event_manager);
  ~FirebaseDatabase();

  //  wss://anger2action-f3698.firebaseio.com/.ws?v=5&ns=anger2action-f3698
  void SetConnectInfo(std::string host, std::string database,
                      std::string listen_path);

  void SetAuthInfo(const std::string& auth_token_url,
                   const std::string& device_id,
                   const std::string& password);

  // Connects to the DB and processes updates.
  void Connect();

  // Disconnets from the DB.
  void Disconnect();

  // Calls |on_update| when an update is received from the server.
  void SetUpdateHandler(std::function<void()> on_update);

  // Sends an update the firebase database.
  void Publish(const std::string& path, unique_cJSON_ptr new_value);

  // Retrieves a fragment of the JSON tree.
  cJSON* Get(const std::string& path);

 private:
  FRIEND_TEST(Firebase, PathUpdate);
  FRIEND_TEST(Firebase, MergeUpdate);
  FRIEND_TEST(Firebase, OverwriteUpdate);

  enum ConnectState {
    kConnectedBit    = 0x1,
    kAuthBit         = 0x2,
    kListenBit       = 0x4,
    kReconnectingBit = 0x8,
  };

  bool is_connected() const { return connect_state_ & kConnectedBit; }
  bool is_authenticated() const { return connect_state_ & kAuthBit; }
  bool is_listening() const { return connect_state_ & kListenBit; }

  void GetPath(const std::string& path, cJSON** parent, cJSON** node,
               bool create_parent_path = false,
               std::string* last_key = nullptr);

  // Sends the set of command after connection that configures the session
  // for updates.
  void SendPostConnectCommands();

  // Handle incoming websocket frames and errors.
  void OnWsFrame(WebsocketFrame frame);

  // Entrypoint to firebase protocol parsing. Takes the full JSON command
  // from the server.
  void OnCommand(cJSON* command);

  // Handles commands with an envelope of of type "c".
  void OnControlCommand(cJSON* command);

  // Handles commands with an envelope of of type "d".
  void OnDataCommand(cJSON* command);
  
  // Sets |new_data| at |path| in the stored json tree.
  void ReplacePath(const char* path, unique_cJSON_ptr new_data);

  // Merges |new_data| into the existing tree at |path| in the stored json tree.
  void MergePath(const char* path, unique_cJSON_ptr new_data);

  // Remove all null elements and objects with no entries.
  bool RemoveEmptyNodes(cJSON* node);

  // Sends |text| over the |websocket_| if connected.
  bool Send(std::string_view text, bool should_log = true);

  // Send Keepalive if connected. |generation| is use to break the resend
  // loop if disconnected.
  void SendKeepalive(int generation);

  // Get the Firebase ID token, send it, and schedule a periodic refresh.
  // |generation| is used to break the resend loop if disconnected.
  void SendAuthentication(int generation);

  // Sends command to listen.
  void SendVersion();
  
  // Sends command to listen.
  void SendListenIfNeeded();

  // Takes a json object that is just the body of a command, and then
  // wraps it in the appropriate envelope for a data command.
  int WrapDataCommand(const char* action, unique_cJSON_ptr* body);

  // Handles the authentication response and loops to refresh. |generation|
  // is used to cancel the refresh loop on disconnect.
  void HandleAuth(HttpRequest request, int generation);

  // Reinitiates the connection with a delay to avoid banging the server.
  void Reconnect();

  // Basic connection configuration
  std::string host_;
  std::string database_;
  std::string listen_path_;
  std::function<void(void)> on_update_;

  // Network objects.
  MongooseEventManager* event_manager_ = nullptr;
  WebsocketChannel websocket_;

  // Firebase protocol state information.
  int connect_state_ = 0;
  int auth_request_num_ = -1;
  int listen_request_num_ = -1;
  int connect_generation_ = 0;
  std::string real_host_;
  std::string session_id_;
  size_t request_num_ = 0;
  unique_cJSON_ptr update_template_;
  std::string firebase_id_token_url_;
  BackoffCalculator<500> backoff_;

  // actual data.
  unique_cJSON_ptr root_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_FIREBASE_FIREBASE_DATABASE_H_
