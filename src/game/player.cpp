#include "game/player.h"

namespace {
constexpr float kThrust      = 34.0f;
constexpr float kBoostFactor = 2.1f;
constexpr float kMouseSens   = 0.0022f;
constexpr float kRollRate    = 2.2f;
}

void Player::applyInput(const InputState& in, float dt) {
    if (!alive_) return;

    Transform& t = body_.transform;

    // Mouse look is applied as a rotation in the ship's local frame, not world
    // yaw/pitch. That's what keeps a barrel roll from inverting the controls.
    float yaw   = -in.mouseDX * kMouseSens;
    float pitch = -in.mouseDY * kMouseSens;
    float roll  = (in.rollRight - in.rollLeft) * kRollRate * dt;

    glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qPitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qRoll  = glm::angleAxis(roll,  glm::vec3(0.0f, 0.0f, 1.0f));

    t.rotation = glm::normalize(t.rotation * qYaw * qPitch * qRoll);

    glm::vec3 accel(0.0f);
    if (in.forward) accel += t.forward();
    if (in.back)    accel -= t.forward();
    if (in.right)   accel += t.right();
    if (in.left)    accel -= t.right();
    if (in.up)      accel += t.up();
    if (in.down)    accel -= t.up();

    if (glm::dot(accel, accel) > 1e-6f) {
        accel = glm::normalize(accel) * kThrust * (in.boost ? kBoostFactor : 1.0f);
        body_.addForce(accel, dt);
    }
}

void Player::update(float dt) {
    if (fireTimer_ > 0.0f) fireTimer_ -= dt;

    if (!alive_) {
        respawnTimer_ -= dt;
        return;
    }
    body_.update(dt);
}

bool Player::takeDamage(float dmg) {
    if (!alive_) return false;
    health_ -= dmg;
    if (health_ <= 0.0f) {
        health_ = 0.0f;
        alive_ = false;
        respawnTimer_ = kRespawnDelay;
        return true;
    }
    return false;
}

void Player::respawn(glm::vec3 position) {
    health_ = kMaxHealth;
    alive_ = true;
    respawnTimer_ = 0.0f;
    body_.velocity = glm::vec3(0.0f);
    body_.angularVelocity = glm::vec3(0.0f);
    body_.transform.position = position;
    body_.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}
