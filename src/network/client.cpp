#include "network/client.h"
#include <cstdio>

// The transport is the one place where "same C++, two targets" genuinely
// cannot be one implementation: the browser will not hand a WASM module a raw
// TCP socket, so the Emscripten build borrows the page's WebSocket object.
//
// Native builds are currently offline (single-player sandbox). Wiring
// IXWebSocket here is the next milestone; the interface above is already
// shaped for it so nothing in the game layer has to change.

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#include <emscripten/emscripten.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

namespace {

EMSCRIPTEN_WEBSOCKET_T g_socket = 0;
std::vector<std::string> g_inbox;

// Minimal field pluck. The payloads are ours and flat, so a real JSON parser
// would be more dependency than the format warrants.
bool pluck(const std::string& src, const char* key, std::string& out) {
    std::string needle = std::string("\"") + key + "\":";
    size_t k = src.find(needle);
    if (k == std::string::npos) return false;
    size_t v = k + needle.size();
    while (v < src.size() && (src[v] == ' ')) ++v;
    if (v >= src.size()) return false;
    if (src[v] == '"') {
        size_t end = src.find('"', v + 1);
        if (end == std::string::npos) return false;
        out = src.substr(v + 1, end - v - 1);
    } else {
        size_t end = src.find_first_of(",}", v);
        if (end == std::string::npos) return false;
        out = src.substr(v, end - v);
    }
    return true;
}

float pluckF(const std::string& src, const char* key, float fallback = 0.0f) {
    std::string s;
    return pluck(src, key, s) ? (float)atof(s.c_str()) : fallback;
}

EM_BOOL onOpen(int, const EmscriptenWebSocketOpenEvent*, void* user) {
    auto* self = (NetworkClient*)user;
    printf("[net] connected as %s\n", self->playerId().c_str());
    return EM_TRUE;
}

EM_BOOL onMessage(int, const EmscriptenWebSocketMessageEvent* e, void*) {
    if (!e->isText) return EM_TRUE;
    g_inbox.emplace_back((const char*)e->data, e->numBytes ? e->numBytes - 1 : 0);
    return EM_TRUE;
}

EM_BOOL onError(int, const EmscriptenWebSocketErrorEvent*, void*) {
    fprintf(stderr, "[net] socket error\n");
    return EM_TRUE;
}

EM_BOOL onClose(int, const EmscriptenWebSocketCloseEvent* e, void*) {
    printf("[net] closed (code %d)\n", e->code);
    return EM_TRUE;
}

}  // namespace

bool NetworkClient::isSupported() const { return emscripten_websocket_is_supported(); }

bool NetworkClient::connect(const std::string& url, const std::string& id) {
    if (!emscripten_websocket_is_supported()) {
        fprintf(stderr, "[net] no WebSocket support in this browser\n");
        return false;
    }
    playerId_ = id;
    url_ = url;

    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = url.c_str();
    // The game loop runs on the main thread, so the socket belongs there too.
    attrs.createOnMainThread = true;
    g_socket = emscripten_websocket_new(&attrs);
    if (g_socket <= 0) return false;

    emscripten_websocket_set_onopen_callback(g_socket, this, onOpen);
    emscripten_websocket_set_onmessage_callback(g_socket, this, onMessage);
    emscripten_websocket_set_onerror_callback(g_socket, this, onError);
    emscripten_websocket_set_onclose_callback(g_socket, this, onClose);
    connected_ = true;
    return true;
}

void NetworkClient::disconnect() {
    if (g_socket > 0) emscripten_websocket_close(g_socket, 1000, "bye");
    g_socket = 0;
    connected_ = false;
}

void NetworkClient::sendState(const PlayerState& s) {
    if (g_socket <= 0) return;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"state\",\"id\":\"%s\",\"px\":%.3f,\"py\":%.3f,\"pz\":%.3f,"
        "\"qw\":%.4f,\"qx\":%.4f,\"qy\":%.4f,\"qz\":%.4f,"
        "\"vx\":%.3f,\"vy\":%.3f,\"vz\":%.3f,\"hp\":%.1f,\"score\":%d,\"alive\":%d}",
        s.id.c_str(), s.position.x, s.position.y, s.position.z,
        s.rotation.w, s.rotation.x, s.rotation.y, s.rotation.z,
        s.velocity.x, s.velocity.y, s.velocity.z, s.health, s.score, s.alive ? 1 : 0);
    emscripten_websocket_send_utf8_text(g_socket, buf);
}

void NetworkClient::sendKill(const std::string& victimId) {
    if (g_socket <= 0) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"type\":\"kill\",\"id\":\"%s\",\"victim\":\"%s\"}",
             playerId_.c_str(), victimId.c_str());
    emscripten_websocket_send_utf8_text(g_socket, buf);
}

void NetworkClient::sendHit(const std::string& victimId, float damage) {
    if (g_socket <= 0) return;
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"hit\",\"id\":\"%s\",\"victim\":\"%s\",\"dmg\":%.1f}",
             playerId_.c_str(), victimId.c_str(), damage);
    emscripten_websocket_send_utf8_text(g_socket, buf);
}

void NetworkClient::sendShot(glm::vec3 origin, glm::vec3 velocity) {
    if (g_socket <= 0) return;
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"shot\",\"id\":\"%s\",\"px\":%.2f,\"py\":%.2f,\"pz\":%.2f,"
             "\"vx\":%.2f,\"vy\":%.2f,\"vz\":%.2f}",
             playerId_.c_str(), origin.x, origin.y, origin.z,
             velocity.x, velocity.y, velocity.z);
    emscripten_websocket_send_utf8_text(g_socket, buf);
}

void NetworkClient::poll() {
    for (const auto& msg : g_inbox) {
        std::string type, id;
        if (!pluck(msg, "type", type)) continue;

        if (type == "state") {
            if (!pluck(msg, "id", id) || id == playerId_) continue;
            PlayerState s;
            s.id = id;
            s.position = {pluckF(msg, "px"), pluckF(msg, "py"), pluckF(msg, "pz")};
            s.rotation = glm::quat(pluckF(msg, "qw", 1.0f), pluckF(msg, "qx"),
                                   pluckF(msg, "qy"), pluckF(msg, "qz"));
            s.velocity = {pluckF(msg, "vx"), pluckF(msg, "vy"), pluckF(msg, "vz")};
            s.health = pluckF(msg, "hp", 100.0f);
            s.score = (int)pluckF(msg, "score");
            s.alive = pluckF(msg, "alive", 1.0f) > 0.5f;
            if (onPeerState) onPeerState(s);
        } else if (type == "hit") {
            std::string victim;
            if (pluck(msg, "victim", victim) && victim == playerId_ && onLocalDamaged)
                onLocalDamaged(pluckF(msg, "dmg", 18.0f));
        } else if (type == "shot") {
            if (!pluck(msg, "id", id) || id == playerId_) continue;
            if (onPeerShot)
                onPeerShot(id,
                           glm::vec3(pluckF(msg, "px"), pluckF(msg, "py"), pluckF(msg, "pz")),
                           glm::vec3(pluckF(msg, "vx"), pluckF(msg, "vy"), pluckF(msg, "vz")));
        } else if (type == "leave") {
            if (pluck(msg, "id", id) && onPeerLeft) onPeerLeft(id);
        } else if (type == "over") {
            std::string w;
            if (pluck(msg, "winner", w) && onMatchOver) onMatchOver(w);
        }
    }
    g_inbox.clear();
}

#else  // native

bool NetworkClient::isSupported() const { return false; }

bool NetworkClient::connect(const std::string&, const std::string& id) {
    playerId_ = id;
    printf("[net] native build: offline sandbox (no transport wired yet)\n");
    return false;
}

void NetworkClient::disconnect() { connected_ = false; }
void NetworkClient::sendState(const PlayerState&) {}
void NetworkClient::sendKill(const std::string&) {}
void NetworkClient::sendHit(const std::string&, float) {}
void NetworkClient::sendShot(glm::vec3, glm::vec3) {}
void NetworkClient::poll() {}

#endif
