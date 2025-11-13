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

    public: 
        WebSocket(String address, int port, String path);
        void connect();
        void loop();
        void reconnect();
        void vote(uint8_t vote);

    private:
        void wsHandler(WStype_t type, uint8_t * payload, size_t length);
        void messageHandler(JsonDocument json);
        void reset();
};