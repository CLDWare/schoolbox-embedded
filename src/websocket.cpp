#include <websocket.hpp>

Preferences pref;

WSCommand mapCommand(const char *command) {
  if (command == "ping")
    return WSCommand::PING;
  if (command == "reg_pin")
    return WSCommand::REG_PIN;
  if (command == "reg_ok")
    return WSCommand::REG_OK;
  if (command == "auth_nonce")
    return WSCommand::AUTH_NONCE;
  if (command == "auth_ok")
    return WSCommand::AUTH_OK;
  if (command == "session_start")
    return WSCommand::SESSION_START;
  if (command == "session_stop")
    return WSCommand::SESSION_STOP;

  return WSCommand::INVALID;
}

WebSocket::WebSocket(String address, int port, String path) {
  this->address = address;
  this->port = port;
  this->path = path;

  ws.setReconnectInterval(5000);
  ws.disableHeartbeat();
  ws.onEvent([this](WStype_t t, uint8_t *p, size_t l) { wsHandler(t, p, l); });

  pref.begin("cldWare", false);
  this->registered = pref.isKey("registered");
  
  if (this->registered) {
    this->id = pref.getString("id");
    this->token = pref.getString("token");
  }
  pref.end();
}

void WebSocket::connect() { ws.begin(address, port, path); }
void WebSocket::loop() { ws.loop(); }
void WebSocket::reset() {
  this->state = WSState::PREAUTH;
  this->pin = 0;
}

void WebSocket::wsHandler(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_TEXT: {
    JsonDocument doc;
    DeserializationError error;
    deserializeJson(doc, payload);

    if (error) {
      Serial.print("[WSc] Error parsing json, ignoring message: ");
      Serial.println(error.c_str());
      break;
    }

    const char *c = doc["c"] | nullptr;
    int e = doc["e"] | -1;
    if (e == -1) {
      Serial.print("[WSc] Invalid message: ");
      Serial.printf("ERROR %d.\n", e);
      break;
    } else if (!c) {
      Serial.print("[WSc] Invalid message: ");
      Serial.println("missing command.");
    }

    break;
  }

  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    break;
  case WStype_CONNECTED:
    Serial.printf("[WSc] Connected to url: %s\n", payload);
    reset();

    break;
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

void WebSocket::messageHandler(JsonDocument json) {
  const char *command = json["c"];

  switch (mapCommand(command)) {
  case WSCommand::PING:
    this->ws.sendTXT("{\"c\": \"pong\"}");
    break;

  case WSCommand::AUTH_OK:
    this->state = WSState::AUTHENTICATED;

    Serial.println("[WSc] Authenticated!");
    break;

  case WSCommand::SESSION_START:
    this->state = WSState::SESSION;

    const char *question = json["d"]["text"];
    if (question == nullptr) {
      Serial.println(
          "[WSc] received session start but didn't get any text. Canceling.");
      break;
    }

    this->sessionQuestion = String(question);
    Serial.print("[WSc] Started session with text: ");
    Serial.println(this->sessionQuestion);
    // TODO: update display
    break;

  case WSCommand::SESSION_STOP:
    this->state = WSState::AUTHENTICATED;
    this->sessionQuestion = "";

    Serial.print("[WSc] Stopped session.");
    break;
  }

  // case WSCommand::
}