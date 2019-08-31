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

FirebaseDatabase::FirebaseDatabase(
    const std::string& host,
    const std::string& database,
    const std::string& listen_path,
    MongooseEventManager* event_manager,
    const std::string& auth_token_url,
    const std::string& device_id,
    const std::string& password)
  : host_(host),
    database_(database),
    listen_path_(listen_path),
    event_manager_(event_manager),
    websocket_(event_manager_,
               "wss://" + host_ + "/.ws?v=5&ns=" + database_),
    update_template_(cJSON_CreateObject()),
    firebase_id_token_url_(
        auth_token_url.empty() ? std::string() : 
        auth_token_url + "?device_id=" + device_id + "&password=" + password),
    root_(cJSON_CreateObject()) {
}

FirebaseDatabase::~FirebaseDatabase() {
}

void FirebaseDatabase::Connect() {
  websocket_.Connect<FirebaseDatabase, &FirebaseDatabase::OnWsFrame>(this);
  ESP_LOGI(kEspCxxTag, "Firebase connecting to %s", host_.c_str());
  SendVersion();
  SendKeepalive();
  SendAuthentication();
  SendListen();
}

void FirebaseDatabase::SetUpdateHandler(std::function<void()> on_update) {
  on_update_ = on_update;
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

  Send(cJSON_PrintUnformatted(command.get()));
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
  ESP_LOGI(kEspCxxTag, "got a frame");
  switch(frame.opcode()) {
    case WebsocketOpcode::kBinary:
      break;

    case WebsocketOpcode::kText: {
      unique_cJSON_ptr json(cJSON_Parse(frame.data().data()));
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
      // TODO(awong): Invalidate socket. Reconnect.
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
      OnConnectionCommand(data);
    } else if (strcmp(type->valuestring, "d") == 0) {
      OnDataCommand(data);
    }
  }
}

void FirebaseDatabase::OnConnectionCommand(cJSON* command) {
  cJSON* type = cJSON_GetObjectItemCaseSensitive(command, "t");
  cJSON* data = cJSON_GetObjectItemCaseSensitive(command, "d");
  cJSON* host = cJSON_GetObjectItemCaseSensitive(data, "h");

  // Two types of connection requests
  //   h - host data
  //   r - redirect.
  if (cJSON_IsString(type) && cJSON_IsString(host) && host->valuestring != nullptr) {
    real_host_ = host->valuestring;
    if (strcmp(type->valuestring, "h") == 0) {
      // Example packet:
      //  {"t":"h","d":{"ts":1547104612018,"v":"5","h":"s-usc1c-nss-205.firebaseio.com","s":"i3lclKY3LAjoEsoOZBrhjIEQlKan2pqa"}}
      is_connected_ = true;
      // TODO(awong): Get session_id from packet here.
      SendAuthentication();
    } else if (strcmp(type->valuestring, "r") == 0) {
      is_connected_ = false;
      // TODO(awong): Should the tree be dropped? Probably not...
      // TODO(awong): Reconnect.
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
    ESP_LOGI(kEspCxxTag, "%s", cJSON_PrintUnformatted(command));
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

bool FirebaseDatabase::Send(std::string_view text) {
  if (!is_connected_) {
    return false;
  }

  websocket_.SendText(text);
  return true;
}

void FirebaseDatabase::SendKeepalive() {
  ESP_LOGI(kEspCxxTag, "Sending keepalive");
  static constexpr int kKeepAliveMs = 45000;
  Send("0");
  event_manager_->RunDelayed([&] {SendKeepalive();}, kKeepAliveMs);
}

void FirebaseDatabase::SendAuthentication() {
  if (firebase_id_token_url_.empty()) {
    return;
  }
  ESP_LOGI(kEspCxxTag, "Sending Authentication");

  auto handle_auth = [&](HttpRequest request) {
    if (request.body().empty()) {
      return;
    }
    unique_cJSON_ptr json(cJSON_Parse(request.body().data()));
    unique_cJSON_ptr id_token(cJSON_DetachItemFromObjectCaseSensitive(json.get(), "id_token"));
    cJSON* expires_in = cJSON_GetObjectItemCaseSensitive(json.get(), "expires_in");
    if (!cJSON_IsString(id_token.get()) || !cJSON_IsNumber(expires_in)) {
      return;
    }

    // Create auth request.
    unique_cJSON_ptr command(cJSON_CreateObject());
    cJSON_AddItemToObject(command.get(), "cred", id_token.release());
    WrapDataCommand("auth", &command);

    // Send authentication request.
    Send(cJSON_PrintUnformatted(command.get()));

    // Schedule the next authentication refresh at 2 mins before expiration.
    event_manager_->RunDelayed([&] {SendAuthentication();},
                               expires_in->valueint - 120);
  };

  event_manager_->HttpConnect(handle_auth, firebase_id_token_url_);
}

void FirebaseDatabase::SendListen() {
  ESP_LOGI(kEspCxxTag, "Sending Listen");
  // Add path.
  unique_cJSON_ptr command(cJSON_CreateObject());
  cJSON_AddStringToObject(command.get(), "p", listen_path_.c_str());
  cJSON_AddStringToObject(command.get(), "h", "");

  WrapDataCommand("q", &command);

  Send(cJSON_PrintUnformatted(command.get()));
}

void FirebaseDatabase::SendVersion() {
  ESP_LOGI(kEspCxxTag, "Sending version");
  // Add path.
  unique_cJSON_ptr command(cJSON_CreateObject());
  // TODO(awong): Stamp the git commit hash of espcxx here.
  cJSON* client_info = cJSON_AddObjectToObject(command.get(), "c");
  cJSON_AddNumberToObject(client_info, "espcxx", 1);
  WrapDataCommand("s", &command);

  Send(cJSON_PrintUnformatted(command.get()));
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

}  // namespace esp_cxx
