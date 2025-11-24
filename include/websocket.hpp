#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

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
    String id;
    String password;
    int pin;

    WSState state;
    WebSocketsClient ws;
    Preferences* prefs;

    public: 
        WebSocket(String address, int port, String path, Preferences* prefs);
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
};

#endif