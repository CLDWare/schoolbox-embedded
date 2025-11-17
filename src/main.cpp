#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <websocket.hpp>
#include "Button2.h"

#include "secrets.h"

WiFiMulti wifiMulti;
WebSocket ws = WebSocket(WS_ADDR, WS_PORT, WS_PATH);

const size_t buttonAmount = sizeof(VOTE_BUTTONS) / sizeof(VOTE_BUTTONS[0]);
Button2 buttons[buttonAmount];

void (*reset)(void) = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  for (int i = 10; i > 1; i--) {
    Serial.println("[BOOT] Initializing serial takes a while.");
    Serial.flush();
    delay(100);
  }
  Serial.println("[BOOT] Serial up!");

  Serial.println("[BOOT] Setting up buttons.");
  setupButtons(buttons);
  for (int i = 0; i < buttonAmount; i++) {
    buttons[i].setTapHandler([i](Button2& btn) {
      Serial.printf("vote! %d\n", i);
      ws.vote(i + 1);
    });
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
    buttons[i].loop();
  }
}
