#include <websocket.hpp>

const char *PREF_STORE = "cldWare"; // FIXME

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

WebSocket::WebSocket(String address, int port, String path,
                     Preferences *prefs) {
  this->address = address;
  this->port = port;
  this->path = path;
  this->prefs = prefs;

  this->ws.setReconnectInterval(5000);
  this->ws.disableHeartbeat();
  this->ws.onEvent(
      [this](WStype_t t, uint8_t *p, size_t l) { wsHandler(t, p, l); });

  this->registered = false; // will be read at init.
}
void WebSocket::init() {
  Preferences *prefs = this->prefs;

  prefs->begin(PREF_STORE);
  this->registered = prefs->isKey("id") && prefs->isKey("password");
  if (this->registered) {
    this->id = prefs->getString("id");
    this->password = prefs->getString("password");
  }
  this->prefs->end();
}

void WebSocket::connect() { this->ws.begin(address, port, path); }
void WebSocket::loop() { this->ws.loop(); }
void WebSocket::disconnect() { this->ws.disconnect(); }

void WebSocket::vote(uint8_t vote) {
  if (!this->ws.isConnected() || this->state != WSState::SESSION) {
    Serial.println("[WSc] tried to vote while I'm not in a session. Aborting.");
    return;
  }

  JsonDocument doc;
  String output;
  doc["c"] = "session_vote";
  doc["d"]["vote"] = vote;

  doc.shrinkToFit();
  serializeJson(doc, output);
  this->ws.sendTXT(output);
}

void WebSocket::wsHandler(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED: {
    Serial.printf("[WSc] Connected to url: %s\n", payload);

    this->state = WSState::PREAUTH;
    this->pin = 0;
    this->sessionQuestion = "";

    if (this->registered) {
      this->authenticate();
    } else {
      this->registrate();
    }

    break;
  }

  case WStype_TEXT: {
    JsonDocument doc;
    DeserializationError error;
    deserializeJson(doc, payload);

    if (error) {
      Serial.print("[WSc] Error parsing json, ignoring message: ");
      Serial.println(error.c_str());
      break;
    }

    if (doc["e"].is<int>()) {
      int e = doc["e"];
      String info = doc["d"]["info"] | "no message";
      Serial.print("[WSc] Error: ");
      Serial.printf("ERROR %d.\n", e);
      Serial.println((char *)payload);
      this->errorHandler(e, info);
      // Server will drop connection if something horrible wrong, I hope.
      break;
    } else if (!doc["c"].is<String>()) {
      Serial.println("[WSc] Invalid message: missing command");
    }

    this->messageHandler(doc);
    break;
  }

  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    break;
  default:
    break;
  }
}

void WebSocket::authenticate() {
  JsonDocument doc;
  String output;

  Serial.println("[WSc] Starting auth.");
  Serial.println(this->id);

  doc["c"] = "auth_start";
  doc["d"]["id"] = this->id;
  doc.shrinkToFit();

  serializeJson(doc, output);
  this->ws.sendTXT(output);
  this->state = WSState::AUTHENTICATING;
}
void WebSocket::registrate() {
  this->ws.sendTXT("{\"c\": \"reg_start\"}");
  this->state = WSState::REGISTRATING;
}

void WebSocket::errorHandler(int ecode, String message) {
  switch (ecode) {
  // TODO: workign on this.
  case 2: // dev server invalid id
  case 3: // dev server invalid signature
    Serial.println("[WSc] Failed to authenticate! CRITICAL \n[WSc] Please "
                   "manually restart the device");
    while (true)
      ;
    break;

  default:
    break;
  }
}

void WebSocket::messageHandler(JsonDocument json) {
  const char *cmd = json["c"];

  /// PING
  if (!strcmp(cmd, "ping")) {
    this->ws.sendTXT("{\"c\": \"pong\"}");
  } else if (!strcmp(cmd, "reg_pin")) {
    if (this->state != WSState::REGISTRATING) {
      Serial.println("[WSc] Received websocket registration command, while I'm "
                     "not registrating. Resetting websocket.");
      this->disconnect();
      return;
    }

    this->pin = json["d"]["pin"];
    if (this->pin == 0) {
      Serial.println("[WSc] Received registration pin command, but it didn't "
                     "contain a pin. Resetting websocket.");
      this->disconnect();
      return;
    }

    Serial.print("[WSc] Registration started, pin:");
    Serial.println(pin);
  } else if (!strcmp(cmd, "reg_ok")) {
    /// REG_OK
    if (this->state != WSState::REGISTRATING) {
      Serial.println("[WSc] Received websocket registration command, while I'm "
                     "not registrating. Resetting websocket.");
      this->disconnect();
      return;
    }

    JsonVariant vId = json["d"]["id"];
    JsonVariant vPassword = json["d"]["password"];

    if (!vId.is<String>() || !vPassword.is<String>()) {
      Serial.println("[WSc] Received registration ok command, but it didn't "
                     "contain an id/passwd. Resetting websocket.");
      this->disconnect();
      return;
    }

    String id = vId.as<String>();
    String password = vPassword.as<String>();
    Preferences *prefs = this->prefs;

    prefs->begin(PREF_STORE);
    prefs->putString("id", id);
    prefs->putString("password", password);
    prefs->end();

    this->id = id;
    this->password = password;
    this->registered = true;

    this->disconnect();
  } else if (!strcmp(cmd, "auth_nonce")) {
    /// AUTH_NONCE
    if (this->state != WSState::AUTHENTICATING) {
      Serial.println("[WSc] Received websocket authentication command, while "
                     "I'm not authenticating. Resetting websocket.");
      this->disconnect();
      return;
    }

    const char *nonce = json["d"]["nonce"];
    if (nonce == nullptr) {
      Serial.println("[WSc] Received auth nonce command, but it didn't "
                     "contain a nonce. Resetting websocket.");
      this->disconnect();
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
    /// AUTH_OK
    if (this->state != WSState::AUTHENTICATING) {
      Serial.println("[WSc] Received websocket authentication command, while "
                     "I'm not authenticating. Resetting websocket.");
      this->disconnect();
      return;
    }
    this->state = WSState::AUTHENTICATED;

    Serial.println("[WSc] Authenticated!");
    return;
  } else if (!strcmp(cmd, "session_start")) {
    /// SESSION_START
    if (this->state != WSState::AUTHENTICATED) {
      Serial.println("[WSc] Received websocket session command, while "
                     "I'm not authenticated/idle. Resetting websocket.");
      this->disconnect();
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
    /// SESSION_STOP
    if (this->state != WSState::SESSION) {
      Serial.println("[WSc] Received websocket stopsession command, while "
                     "I'm not in a session. Resetting websocket.");
      this->disconnect();
      return;
    }

    this->state = WSState::AUTHENTICATED;
    this->sessionQuestion = "";

    Serial.print("[WSc] Stopped session.");
    return;
  }
}