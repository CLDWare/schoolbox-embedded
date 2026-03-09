#include <Arduino.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiSTA.h>
#include "Button2.h"

#include <websocket.hpp>
#include <menu.hpp>
#include "secrets.h"
#include "esp_log.h"
#include <LiquidCrystal_I2C.h>

Preferences prefs;
LiquidCrystal_I2C lcd(0x27,20,4);  
Menu menu;
WebSocket ws = WebSocket(WS_ADDR, WS_PORT, WS_PATH, &prefs, &lcd);

const int buttonAmount = sizeof(VOTE_BUTTONS) / sizeof(VOTE_BUTTONS[0]);
Button2 buttons[buttonAmount];

void (*reset)(void) = 0;

void setup() {
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  Serial.begin(115200);
  delay(100);

  for (int i = 10; i > 1; i--) {
    Serial.println("[BOOT] Initializing serial takes a while.");
    Serial.flush();
    delay(100);
  }
  Serial.println("[BOOT] Serial up!");
  menu.showMenu();

  Serial.println("[BOOT] Setting up buttons.");
  setupButtons(buttons);
  for (int i = 0; i < buttonAmount; i++) {
    buttons[i].setTapHandler([i](Button2& btn) {
      Serial.printf("vote! %d\n", i+1);
      ws.vote(i+1);
    });
  }

  Serial.println("[BOOT] Setting up display.");
  lcd.init();                      // initialize the lcd 
  lcd.noBacklight();
  lcd.setCursor(0,0);

  lcd.print("Connecting to wifi..");
  Serial.println("[BOOT] Connecting to wifi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 

  for (int i = 10; i > 1; i--) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[BOOT] Connected to wifi!");
      break;
    }

    int prev = millis();
    int now = millis();
    while (prev + 1000 > now) {
      now = millis();
      menu.loop();
      delay(10);
    } 
    Serial.print(".");
  }
  lcd.clear();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[BOOT] Failed to connect to wifi.");
    delay(200);
    reset();
    return; // for readability, reset stops the program anyway.
  }

  Serial.println(WiFi.localIP().toString());
  Serial.println("[BOOT] Connecting to websocket.");

  ws.init();
  ws.connect();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Wifi disconnected, restarting.");
    reset();
    return;
  }

  menu.loop();
  ws.loop();

  for (int i = 0; i < buttonAmount; i++) {
    buttons[i].loop();
  }
}
