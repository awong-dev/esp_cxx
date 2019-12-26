#include "esp_cxx/firebase/firebase_database.h"

#include "cJSON.h"
extern "C" {
#include "cJSON_Utils.h"
}

#include <array>

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/mongoose_event_manager.h"
#include "esp_cxx/logging.h"

namespace {

struct RemoveEmptyState {
  cJSON* parent;
  cJSON* cur;
  cJSON* entry;
};

}  // namespace

namespace esp_cxx {

FirebaseDatabase::FirebaseDatabase(MongooseEventManager* event_manager)
  : event_manager_(event_manager),
    update_template_(cJSON_CreateObject()),
    root_(cJSON_CreateObject()) {
}

FirebaseDatabase::~FirebaseDatabase() {
}

void FirebaseDatabase::SetConnectInfo(std::string host,
                                      std::string database,
                                      std::string listen_path) {
  host_ = std::move(host);
  database_ = std::move(database);
  listen_path_ = std::move(listen_path);
  websocket_ = WebsocketChannel(event_manager_,
                         "wss://" + host_ + "/.ws?v=5&ns=" + database_,
                         [this](WebsocketFrame frame) { OnWsFrame(std::move(frame)); },
                         [this] {
                           // Send reconnect on a different event frame to avoid reentrancy.
                           event_manager_->Run([this]{
                                               Reconnect();
                                               });
                         });
}

void FirebaseDatabase::SetAuthInfo(const std::string& auth_token_url,
                                   const std::string& device_id,
                                   const std::string& password) {
  if (auth_token_url.empty()) {
    firebase_id_token_url_.clear();
  } else {
    firebase_id_token_url_ =
      auth_token_url + "?device_id=" + device_id + "&password=" + password;
  }
}

void FirebaseDatabase::Connect() {
  connect_state_ = 0;  // Clear all bits, including reconnecting.
  if (!(websocket_.Connect())) {
    ESP_LOGE(kEspCxxTag, "Websocket connect failure");
    Reconnect();
  }
}

void FirebaseDatabase::Disconnect() {
  connect_generation_++;
  websocket_.Disconnect();
}

void FirebaseDatabase::SendPostConnectCommands() {
  assert(is_connected());
  SendVersion();
  SendKeepalive(connect_generation_);
  SendAuthentication(connect_generation_);
}

void FirebaseDatabase::SetUpdateHandler(std::function<void()> on_update) {
  on_update_ = on_update;
}

void FirebaseDatabase::SetAuthHandler(std::function<void(bool, cJSON*)> on_auth) {
  on_auth_ = on_auth;
}

void FirebaseDatabase::Publish(const std::string& path,
                               unique_cJSON_ptr new_value) {
  // Example packet:
  //  {"t":"d","d":{"r":4,"a":"p","b":{"p":"/test","d":{"hi":"mom","num":1547104593160},"h":""}}}

  // Insert data.
  unique_cJSON_ptr command(cJSON_CreateObject());
  cJSON_AddStringToObject(command.get(), "p", path.c_str());
  cJSON_AddItemToObject(command.get(), "d", cJSON_Duplicate(new_value.get(), true));
  WrapDataCommand("p", &command);

  ReplacePath(path.c_str(), std::move(new_value));

  Send(PrintJson(command.get()).get());
  // TODO(awong): If disconnected then we should mark dirty and handle
  // update merging.
}

cJSON* FirebaseDatabase::Get(const std::string& path) {
  cJSON* parent;
  cJSON* node;
  GetPath(path, &parent, &node);
  return node;
}

void FirebaseDatabase::GetPath(const std::string& path, cJSON** parent_out, cJSON** node_out,
                               bool create_path, std::string* last_key_out) {
  static constexpr char kPathSeparator[] = "/";

  cJSON* parent = nullptr;
  cJSON* cur = root_.get();

  unique_C_ptr<char> path_copy(strdup(path.c_str()));
  const char* last_key = nullptr;
  for (const char* key = strtok(path_copy.get(), kPathSeparator);
       key;
       key = strtok(NULL, kPathSeparator)) {
    last_key = key;
    parent = cur;
    cur = cJSON_GetObjectItemCaseSensitive(parent, key);

    // does not exist.
    if (create_path && !cur) {
      // If node is null, just start creating the parents.
      cur = cJSON_AddObjectToObject(parent, key);
    }
  }

  *parent_out = parent;
  *node_out = cur;
  if (last_key_out && last_key) {
    *last_key_out = last_key;
  }
}

bool FirebaseDatabase::RemoveEmptyNodes(cJSON* node) {
  if (!cJSON_IsObject(node))
    return true;

  std::array<RemoveEmptyState, 10> stack; // No more than 10 deep please.
  int stack_level = 0;
  stack.at(stack_level++) = {nullptr, node, node->child};

  while (stack_level > 0) {
    RemoveEmptyState state = stack.at(--stack_level);
    cJSON* entry = state.entry;
    while (entry) {
      if (cJSON_IsNull(entry)) {
        cJSON_Delete(cJSON_DetachItemViaPointer(state.cur, entry));
      } else if (cJSON_IsObject(entry)) {
        if (cJSON_GetArraySize(state.cur) == 0) {
          cJSON_Delete(cJSON_DetachItemViaPointer(state.parent, state.cur));
        } else {
          if (stack_level + 2 >= stack.size()) {
            return false;
          }
          stack.at(stack_level++) = {state.parent, state.cur, entry->next};
          stack.at(stack_level++) = {state.cur, entry, entry->child};
          goto recur;
        }
      }
      entry = entry->next;
    }

recur:
    ;
  }

  return true;
}

void FirebaseDatabase::OnWsFrame(WebsocketFrame frame) {
  switch(frame.opcode()) {
    case WebsocketOpcode::kBinary:
      break;

    case WebsocketOpcode::kText: {
      unique_cJSON_ptr json(cJSON_Parse(frame.data().data()));
      ESP_LOGI(kEspCxxTag, "Recv: %s", PrintJson(json.get()).get());
      OnCommand(json.get());
      if (on_update_) {
        on_update_();
      }
      break;
    }

    case WebsocketOpcode::kPing:
      // Pong is already sent by mongoose. This is just a notification.
    case WebsocketOpcode::kPong:
      break;

    case WebsocketOpcode::kClose:
      // Do nothing. The |on_disconnect_cb| in WebsocketChannel will be called.
      break;

    case WebsocketOpcode::kContinue:
      // TODO(awong): Shouldn't be here. The mongose implementation is supposed to reconstruct.
      break;
  }
}

void FirebaseDatabase::OnCommand(cJSON* command) {
  // Find the envelope.
  // Dispatch update.
  cJSON* type = cJSON_GetObjectItemCaseSensitive(command, "t");
  if (cJSON_IsString(type)) {
    cJSON* data = cJSON_GetObjectItemCaseSensitive(command, "d");

    // Type has 2 possibilities:
    //   c = connection oriented command like server information or
    //       redirect info.
    //   d = data commands such as publishing new database entries.
    if (strcmp(type->valuestring, "c") == 0) {
      OnControlCommand(data);
    } else if (strcmp(type->valuestring, "d") == 0) {
      OnDataCommand(data);
    }
  }
}

void FirebaseDatabase::OnControlCommand(cJSON* command) {
  cJSON* type = cJSON_GetObjectItemCaseSensitive(command, "t");
  cJSON* data = cJSON_GetObjectItemCaseSensitive(command, "d");
  cJSON* host = cJSON_GetObjectItemCaseSensitive(data, "h");

  // Two types of connection requests
  //   h - server hello host data
  //   r - reset
  //   e - error
  //   e - server pong
  //   n - end transmission
  if (cJSON_IsString(type) && cJSON_IsString(host) && host->valuestring != nullptr) {
    real_host_ = host->valuestring;
    if (strcmp(type->valuestring, "h") == 0) {
      // Example packet:
      //  {"t":"h","d":{"ts":1547104612018,"v":"5","h":"s-usc1c-nss-205.firebaseio.com","s":"i3lclKY3LAjoEsoOZBrhjIEQlKan2pqa"}}
      cJSON* session_id = cJSON_GetObjectItemCaseSensitive(data, "s");
      if (cJSON_IsString(type)) {
        session_id_ = session_id->valuestring;
      }
      connect_state_ |= kConnectedBit;
      backoff_.Reset();
      ESP_LOGI(kEspCxxTag, "Database Connected.");
      SendPostConnectCommands();
    } else if (strcmp(type->valuestring, "r") == 0) {
      Reconnect();
    }
  }
}

void FirebaseDatabase::OnDataCommand(cJSON* command) {
  cJSON* request_id = cJSON_GetObjectItemCaseSensitive(command, "r");
  cJSON* body = cJSON_GetObjectItemCaseSensitive(command, "b");

  if (request_id) {
    // If a request_id exists, this is a response and the body
    // has a "s" field for status with a "d" field for data.
    //
    // Example no error:
    //   {"t":"d","d":{"r":1,"b":{"s":"ok","d":""}}}
    // Example error:
    //   {"t":"d","d":{"r":3,"b":{"s":"permission_denied","d":"Permission denied"}}}
    //
    // Since sends are infrequent, just log all responses.
    ESP_LOGI(kEspCxxTag, "%s", PrintJson(command).get());
    if (cJSON_IsNumber(request_id)) {
      int r = request_id->valueint;
      cJSON* status = cJSON_GetObjectItemCaseSensitive(body, "s");
      bool is_ok = cJSON_IsString(status) && strncmp("ok", status->valuestring, 2) == 0;
      if (r == auth_request_num_) {
        if (is_ok) {
          connect_state_ |= kAuthBit;
        }
        ESP_LOGI(kEspCxxTag, "Database Authenticated.");
        on_auth_(is_ok, status);
        auth_request_num_ = -1;
      } else if (r == listen_request_num_) {
        if (is_ok) {
          connect_state_ |= kListenBit;
        }
        ESP_LOGI(kEspCxxTag, "Database Listening.");
        listen_request_num_ = -1;
      }
    }
  } else {
    // If request_id does not exist, then this is an update from the server.
    cJSON* action = cJSON_GetObjectItemCaseSensitive(command, "a");
    //  cJSON* hash = cJSON_GetObjectItemCaseSensitive(body, "h");
    if (cJSON_IsString(action) && action->valuestring != nullptr &&
      cJSON_IsObject(body)) {
      cJSON* path = cJSON_GetObjectItemCaseSensitive(body, "p");
      // There are 2 action types received:
      //   d - a JSON tree is being replaced.
      //   m - a JSON tree should be merged. [ TODO(awong): what does this mean? Don't delete? ]
      unique_cJSON_ptr new_data(cJSON_DetachItemFromObjectCaseSensitive(body, "d"));
      if (strcmp(action->valuestring, "d") == 0) {
        ReplacePath(path->valuestring, std::move(new_data));
      } if (strcmp(action->valuestring, "m") == 0) {
        MergePath(path->valuestring, std::move(new_data));
      }
    }
  }
}

void FirebaseDatabase::ReplacePath(const char* path, unique_cJSON_ptr new_data) {
  cJSON* parent;
  cJSON* node;
  std::string key;
  GetPath(path, &parent, &node, true, &key);
  if (parent) {
    cJSON_ReplaceItemInObjectCaseSensitive(parent, key.c_str(),  new_data.release());
  } else {
    root_ = std::move(new_data);
    // TODO(awong): Fix cJSON To correct item->string which isn't erased here after detach.
  }

  // Firebase doesn't support nulls. This garbage collection step keeps us consistent.
  RemoveEmptyNodes(root_.get());
}

void FirebaseDatabase::MergePath(const char* path, unique_cJSON_ptr new_data) {
  // new_data is actually a key/value pair of relative _paths_ from the root.
  // Each path is considered an overwrite operation.
  cJSON* update = new_data->child;
  while (update) {
    // Cache next pointer as item will be detached in loop.
    cJSON* next = update->next;
    std::string update_path = path;
    update_path += "/";
    update_path += update->string;
    unique_cJSON_ptr update_node(cJSON_DetachItemViaPointer(new_data.get(), update));
    ReplacePath(update_path.c_str(), std::move(update_node));
    update = next;
  }
}

bool FirebaseDatabase::Send(std::string_view text, bool should_log) {
  if (!is_connected()) {
    return false;
  }

  if (should_log) {
    ESP_LOGI(kEspCxxTag, "Send: %.*s", text.length(), text.data());
  }

  websocket_.SendText(text);
  return true;
}

void FirebaseDatabase::SendKeepalive(int generation) {
  if (connect_generation_ != generation) {
    // This has been cancelled.
    return;
  }

  static constexpr int kKeepAliveMs = 45000;
  Send("0", false);
  event_manager_->RunDelayed(
      [this, generation] { SendKeepalive(generation); },
      kKeepAliveMs);
}

void FirebaseDatabase::SendAuthentication(int generation) {
  if (connect_generation_ != generation) {
    // This has been cancelled.
    return;
  }

  if (firebase_id_token_url_.empty()) {
    SendListenIfNeeded();
    return;
  }

  event_manager_->HttpConnect(
      [this, generation](HttpRequest request) { HandleAuth(request, generation); },
      firebase_id_token_url_);
}

void FirebaseDatabase::SendListenIfNeeded() {
  if (is_listening()) {
    return;
  }
  
  // Add path.
  unique_cJSON_ptr command(cJSON_CreateObject());
  cJSON_AddStringToObject(command.get(), "p", listen_path_.c_str());
  cJSON_AddStringToObject(command.get(), "h", "");

  listen_request_num_ = WrapDataCommand("q", &command);

  Send(PrintJson(command.get()).get());
}

void FirebaseDatabase::SendVersion() {
  // Add path.
  unique_cJSON_ptr command(cJSON_CreateObject());
  // TODO(awong): Stamp the git commit hash of espcxx here.
  cJSON* client_info = cJSON_AddObjectToObject(command.get(), "c");
  cJSON_AddNumberToObject(client_info, "espcxx", 1);
  WrapDataCommand("s", &command);

  Send(PrintJson(command.get()).get());
}

int FirebaseDatabase::WrapDataCommand(const char* action,
                                      unique_cJSON_ptr* body) {
  // Create data envelope for auth.
  unique_cJSON_ptr command(cJSON_CreateObject());
  cJSON_AddStringToObject(command.get(), "t", "d");
  cJSON* data = cJSON_AddObjectToObject(command.get(), "d");

  // Add request number and action.
  cJSON_AddNumberToObject(data, "r", ++request_num_);
  cJSON_AddStringToObject(data, "a", action);

  // Add body and swap it back.
  body->swap(command);
  cJSON_AddItemToObject(data, "b", command.release());  // command is old body.

  return request_num_;
}

void FirebaseDatabase::HandleAuth(HttpRequest request, int generation) {
  if (connect_generation_ != generation) {
    // This has been cancelled.
    return;
  }
  if (request.body().empty()) {
    return;
  }

  ESP_LOGI(kEspCxxTag, "Received auth: %d, \nbody: %.*s",
           request.status(),
           request.body().length(), request.body().data());
  std::string body(request.body());
  unique_cJSON_ptr json(cJSON_Parse(request.body().data()));
  if (!json) {
    ESP_LOGW(kEspCxxTag, "Json parse error. Aborting authentication.");
    return;
  }
  unique_cJSON_ptr id_token(cJSON_DetachItemFromObjectCaseSensitive(json.get(), "id_token"));
  cJSON* expires_in = cJSON_GetObjectItemCaseSensitive(json.get(), "expires_in");
  if (!cJSON_IsString(id_token.get()) || !cJSON_IsNumber(expires_in)) {
    ESP_LOGW(kEspCxxTag, "Failed parsing ID token");
    return;
  }

  // Create auth request.
  unique_cJSON_ptr command(cJSON_CreateObject());
  cJSON_AddItemToObject(command.get(), "cred", id_token.release());
  auth_request_num_ = WrapDataCommand("auth", &command);

  // Send authentication request.
  Send(PrintJson(command.get()).get());

  // Send listen command if necessary. On subsequent auths, this is likely
  // a no-op.
  SendListenIfNeeded();

  // Schedule the next authentication refresh at 2 mins before expiration.
  event_manager_->RunDelayed([this, generation] {SendAuthentication(generation);},
                             (expires_in->valueint - 120) * 1000);
}

void FirebaseDatabase::Reconnect() {
  if (connect_state_ & kReconnectingBit) {
    // Don't reconnect while reconnecting.
    ESP_LOGI(kEspCxxTag, "Skipping reconnect");
    return;
  }

  Disconnect();

  connect_state_ = kReconnectingBit;
  int next_reconnect = backoff_.MsToNextTry();
  ESP_LOGI(kEspCxxTag, "Reconnecting WS in %d", next_reconnect);
  event_manager_->RunDelayed([this] { Connect(); }, next_reconnect);
}

}  // namespace esp_cxx
