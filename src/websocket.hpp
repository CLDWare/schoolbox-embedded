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
enum WSCommand {
    INVALID,
    PING,
    REG_PIN,
    REG_OK,
    AUTH_NONCE,
    AUTH_OK,
    SESSION_START,
    SESSION_STOP
};

class WebSocket {
    String address;
    int port;
    String path;

    String sessionQuestion;

    bool registered = false;
    String id;
    String token;
    int pin;

    WSState state;
    WebSocketsClient ws;

    public: 
        WebSocket(String address, int port, String path);
        void connect();
        void loop();

    private:
        void wsHandler(WStype_t type, uint8_t * payload, size_t length);
        void messageHandler(JsonDocument json);
        void reset();
};