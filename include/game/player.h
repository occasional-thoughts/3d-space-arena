#pragma once
#include "engine/physics.h"
#include "engine/gl.h"
#include <string>

struct InputState {
    bool forward = false, back = false, left = false, right = false;
    bool boost = false, up = false, down = false;
    bool fire = false;
    float mouseDX = 0.0f, mouseDY = 0.0f;
    float rollLeft = 0.0f, rollRight = 0.0f;
};

class Player {
public:
    static constexpr float kRadius       = 1.0f;
    static constexpr float kMaxHealth    = 100.0f;
    static constexpr float kFireCooldown = 0.18f;
    static constexpr float kRespawnDelay = 3.0f;

    Player() = default;
    explicit Player(std::string id, glm::vec3 color) : id_(std::move(id)), color_(color) {}

    void applyInput(const InputState& in, float dt);
    void update(float dt);

    bool canFire() const { return alive_ && fireTimer_ <= 0.0f; }
    void noteFired() { fireTimer_ = kFireCooldown; }

    // Returns true when this hit was the killing blow.
    bool takeDamage(float dmg);
    void respawn(glm::vec3 position);

    const Transform& transform() const { return body_.transform; }
    Transform& transform() { return body_.transform; }
    RigidBody& body() { return body_; }
    const RigidBody& body() const { return body_; }

    const std::string& id() const { return id_; }
    glm::vec3 color() const { return color_; }
    float health() const { return health_; }
    int score() const { return score_; }
    void addScore(int n) { score_ += n; }
    bool alive() const { return alive_; }
    float respawnTimer() const { return respawnTimer_; }

    void setId(std::string v) { id_ = std::move(v); }
    void setColor(glm::vec3 c) { color_ = c; }
    void setHealth(float h) { health_ = h; alive_ = h > 0.0f; }
    void setScore(int s) { score_ = s; }

private:
    RigidBody body_;
    std::string id_;
    glm::vec3 color_{0.2f, 0.9f, 1.0f};
    float health_ = kMaxHealth;
    int score_ = 0;
    bool alive_ = true;
    float fireTimer_ = 0.0f;
    float respawnTimer_ = 0.0f;
};
