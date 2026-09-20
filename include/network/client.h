#pragma once
#include "game/player.h"
#include <string>
#include <vector>
#include <functional>

// Wire format for one ship. Sent at a fixed tick, not every frame.
struct PlayerState {
    std::string id;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    float health = 100.0f;
    int score = 0;
    bool alive = true;
};

class NetworkClient {
public:
    // Called with every state the server broadcasts except our own.
    std::function<void(const PlayerState&)> onPeerState;
    std::function<void(const std::string& id)> onPeerLeft;
    std::function<void(const std::string& winnerId)> onMatchOver;
    // A peer reported hitting us. The victim applies its own damage, which is
    // what keeps one client's health from being two different numbers.
    std::function<void(float damage)> onLocalDamaged;
    // A peer fired; spawn a cosmetic bullet so incoming fire is visible.
    std::function<void(const std::string& shooterId, glm::vec3 origin,
                       glm::vec3 velocity)> onPeerShot;

    bool connect(const std::string& url, const std::string& playerId);
    void disconnect();

    bool isConnected() const { return connected_; }
    bool isSupported() const;

    void sendState(const PlayerState& state);
    void sendKill(const std::string& victimId);
    void sendHit(const std::string& victimId, float damage);
    void sendShot(glm::vec3 origin, glm::vec3 velocity);

    // Drains whatever the transport buffered. Called once per frame.
    void poll();

    const std::string& playerId() const { return playerId_; }

private:
    bool connected_ = false;
    std::string playerId_;
    std::string url_;
};
