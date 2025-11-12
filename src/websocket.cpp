#include <websocket.hpp>

Preferences pref;
const char *PREFERENCE_STORE = "cldWare";

const char *generateHMAC(const char *key, const char *data) {
  static char out[65];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  const size_t keyLen = strlen(key);
  const size_t dataLen = strlen(data);
  unsigned char hmac[32];

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, keyLen);
  mbedtls_md_hmac_update(&ctx, (const unsigned char *)data, dataLen);
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  for (int i = 0; i < 32; ++i)
    sprintf(out + 2 * i, "%02x", hmac[i]);
  out[64] = '\0';

  return out;
}

WebSocket::WebSocket(String address, int port, String path) {
  this->address = address;
  this->port = port;
  this->path = path;

  ws.setReconnectInterval(5000);
  ws.disableHeartbeat();
  ws.onEvent([this](WStype_t t, uint8_t *p, size_t l) { wsHandler(t, p, l); });

  pref.begin(PREFERENCE_STORE, false);
  this->registered = pref.isKey("registered");

  if (this->registered) {
    this->id = pref.getString("id");
    this->password = pref.getString("password");
  } 
  pref.end();
}

void WebSocket::connect() { ws.begin(address, port, path); }
void WebSocket::reconnect() {
  ws.disconnect();
  delay(1000);
  this->connect();
}
void WebSocket::loop() { ws.loop(); }
void WebSocket::reset() {
  this->state = WSState::PREAUTH;
  this->pin = 0;
  this->sessionQuestion = "";
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

    Serial.println((char *)payload);

    const char *c = doc["c"];
    int e = doc["e"] | -1;

    if (e != -1) {
      Serial.print("[WSc] Invalid message: ");
      Serial.printf("ERROR %d.\n", e);
      Serial.println((char *)payload);
      // Server will drop connection if something horrible wrong, I hope.
      break;
    } else if (c == nullptr) {
      Serial.print("[WSc] Invalid message: ");
      Serial.println("missing command.");
    }

    this->messageHandler(doc);
    break;
  }

  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    break;
  case WStype_CONNECTED:
    Serial.printf("[WSc] Connected to url: %s\n", payload);
    reset();

    if (this->registered) {
      JsonDocument doc;
      String output;

      Serial.println("starting auth: ");
      Serial.println(this->id);

      doc["c"] = "auth_start";
      doc["d"]["id"] = this->id;
      doc.shrinkToFit();

      serializeJson(doc, output);
      this->ws.sendTXT(output);
      this->state = WSState::AUTHENTICATING;
    } else {
      this->ws.sendTXT("{\"c\": \"reg_start\"}");
      this->state = WSState::REGISTRATING;
    }

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
  const char *cmd = json["c"];

  Serial.print("Message received, command: '");
  Serial.println(cmd);


  if (!strcmp(cmd, "ping")) {
    this->ws.sendTXT("{\"c\": \"pong\"}");

    Serial.println("ping ping");
  } else if (!strcmp(cmd, "reg_pin")) {
    if (this->state != WSState::REGISTRATING) {
      Serial.println("[WSc] Received websocket registration command, while I'm "
                     "not registrating. Resetting websocket.");
      this->reconnect();
      return;
    }

    this->pin = json["d"]["pin"];
    if (this->pin == 0) {
      Serial.println("[WSc] Received registration pin command, but it didn't "
                     "contain a pin. Resetting websocket.");
      this->reconnect();
      return;
    }

    Serial.print("[WSc] Registration started, pin:");
    Serial.println(pin);
  } else if (!strcmp(cmd, "reg_ok")) {
    if (this->state != WSState::REGISTRATING) {
      Serial.println("[WSc] Received websocket registration command, while I'm "
                     "not registrating. Resetting websocket.");
      this->reconnect();
      return;
    }

    JsonVariant vId = json["d"]["id"];
    JsonVariant vPassword = json["d"]["password"];

    if (!vId.is<String>() || !vPassword.is<String>()) {
      Serial.println("[WSc] Received registration ok command, but it didn't "
                     "contain an id/passwd. Resetting websocket.");
      this->reconnect();
      return;
    }

    String id = vId.as<String>();
    String password = vPassword.as<String>();

    pref.begin(PREFERENCE_STORE, false);
    pref.putString("id", id);
    pref.putString("password", password);
    pref.putBool("registered", true);
    pref.end();

    this->id = id;
    this->password = password;
    this->registered = true;

    Serial.println("Ooh fancy, id and password");
    Serial.print(this->id);
    Serial.print(this->password);

    this->reconnect();
  } else if (!strcmp(cmd, "auth_nonce")) {
    if (this->state != WSState::AUTHENTICATING) {
      Serial.println("[WSc] Received websocket authentication command, while "
                     "I'm not authenticating. Resetting websocket.");
      this->reconnect();
      return;
    }

    const char *nonce = json["d"]["nonce"];
    if (nonce == nullptr) {
      Serial.println("[WSc] Received auth nonce command, but it didn't "
                     "contain a nonce. Resetting websocket.");
      this->reconnect();
      return;
    }

    const char *signature = generateHMAC(this->password.c_str(), nonce);
    Serial.println("signature:");
    Serial.println(signature);

    JsonDocument doc;
    String output;

    doc["c"] = "auth_validate";
    doc["d"]["signature"] = signature;

    doc.shrinkToFit();
    serializeJson(doc, output);
    Serial.println(output);

    ws.sendTXT(output);
  } else if (!strcmp(cmd, "auth_ok")) {
    if (this->state != WSState::AUTHENTICATING) {
      Serial.println("[WSc] Received websocket authentication command, while "
                     "I'm not authenticating. Resetting websocket.");
      this->reconnect();
      return;
    }
    this->state = WSState::AUTHENTICATED;

    Serial.println("[WSc] Authenticated!");
    return;
  } else if (!strcmp(cmd, "session_start")) {
    if (this->state != WSState::AUTHENTICATED) {
      Serial.println("[WSc] Received websocket session command, while "
                     "I'm not authenticated. Resetting websocket.");
      this->reconnect();
      return;
    }
    this->state = WSState::SESSION;

    const char *question = json["d"]["text"];
    if (question == nullptr) {
      Serial.println(
          "[WSc] received session start but didn't get any text. Canceling.");
      return;
    }

    this->sessionQuestion = String(question);
    Serial.print("[WSc] Started session with text: ");
    Serial.println(this->sessionQuestion);
    // TODO: update display
    return;
  } else if (!strcmp(cmd, "session_stop")) {
    if (this->state != WSState::SESSION) {
      Serial.println("[WSc] Received websocket stopsession command, while "
                     "I'm not in a session. Resetting websocket.");
      this->reconnect();
      return;
    }

    this->state = WSState::AUTHENTICATED;
    this->sessionQuestion = "";

    Serial.print("[WSc] Stopped session.");
    return;
  }
}