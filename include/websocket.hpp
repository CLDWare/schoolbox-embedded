#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>

enum WSState {
    PREAUTH,
    REGISTRATING,
    AUTHENTICATING,
    AUTHENTICATED,
    SESSION
};

class WebSocket {
    String address;
    int port;
    String path;

    String sessionQuestion;

    bool registered = false;
    String password;
    int id;
    int pin;

    WSState state;
    WebSocketsClient ws;
    Preferences* prefs;
    LiquidCrystal_I2C* lcd;

    public: 
        WebSocket(String address, int port, String path, Preferences* prefs, LiquidCrystal_I2C* lcd);
        void init();
        void connect();
        void disconnect();
        void loop();
        void vote(uint8_t vote);

    private:
        void authenticate();
        void registrate();

        void wsHandler(WStype_t type, uint8_t * payload, size_t length);
        void messageHandler(JsonDocument json);
        void errorHandler(int ecode, String message);
        void resetLCD(bool backlight = false);

};

#endif