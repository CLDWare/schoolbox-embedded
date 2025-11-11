#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "secrets.h"


WiFiMulti wifiMulti;

void (*reset)(void) = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("[BOOT] Serial up!");
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWD);

  Serial.println("[BOOT] Connecting to wifi...");
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
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("[WIFI] Wifi disconnected, restarting.");
    reset();
    return;
  }


}
