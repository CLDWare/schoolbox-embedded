#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>

#include "secrets.h"
#include <websocket.hpp>

WiFiMulti wifiMulti;
WebSocket ws = WebSocket(WS_ADDR, WS_PORT, WS_PATH);

const size_t buttonAmount = sizeof(VOTE_BUTTONS) / sizeof(VOTE_BUTTONS[0]);
bool buttonPressed[buttonAmount] = {false};
unsigned long buttonDebounce[buttonAmount];

void (*reset)(void) = 0;

void setup() {
  Serial.begin(115200);
  for (int i = 10; i > 1; i--) {
    Serial.println("[BOOT] Initializing serial takes a while.");
    Serial.flush();
    delay(100);
  }
  Serial.println("[BOOT] Serial up!");

  Serial.println("[BOOT] Setting up buttons.");
  for (int button : VOTE_BUTTONS) {
    pinMode(button, INPUT_PULLUP);

    // Serial.printf("[BOOT] button %d is %d.\n", button, digitalRead(button));
  }

  Serial.println("[BOOT] Connecting to wifi...");
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWD);
  for (int i = 10; i > 1; i--) {
    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.println("[BOOT] Connected to wifi!");
      break;
    }

    Serial.print(".");
    delay(1000);
  }

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("[BOOT] Failed to connect to wifi.");
    reset();
    return; // for readability, reset stops the program anyway.
  }

  Serial.println(WiFi.localIP().toString());
  Serial.println("[BOOT] Connecting to websocket.");
  ws.connect();
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("[WIFI] Wifi disconnected, restarting.");
    reset();
    return;
  }

  ws.loop();

  for (int i = 0; i < buttonAmount; i++) {
    unsigned long now = millis();
    int pin = VOTE_BUTTONS[i];
    bool value = !digitalRead(pin);

    if (value != buttonPressed[i]) {
      buttonDebounce[i] = now;
    }

    if ((now - buttonDebounce[i]) >= 500) {
      if (value && !buttonPressed[i]) {
        buttonPressed[i] = true;
        Serial.printf("[VOTE] %d\n", i);
        ws.vote(i + 1);
      } else if (!value && buttonPressed[i]) {
        buttonPressed[i] = false;
      }
    } else {
      buttonPressed[i] = value;
    };
  }
}
