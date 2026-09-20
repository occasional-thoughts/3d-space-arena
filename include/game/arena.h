#pragma once
#include "game/player.h"
#include "engine/particles.h"
#include "engine/renderer.h"
#include <vector>
#include <functional>
#include <string>

struct Bullet {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    std::string ownerId;
    float life = 2.5f;
};

class Arena {
public:
    static constexpr float kHalfExtent   = 90.0f;
    static constexpr float kBulletSpeed  = 120.0f;
    static constexpr float kBulletDamage = 18.0f;
    static constexpr int   kWinScore     = 10;

    void init();
    void shutdown();

    void spawnBullet(const Player& shooter);
    // A bullet another client fired. Relayed so incoming fire is visible;
    // it is cosmetic locally -- only the shooter's client resolves its hits.
    void spawnRemoteBullet(glm::vec3 origin, glm::vec3 velocity,
                           glm::vec3 color, const std::string& ownerId);
    // Remotes are borrowed, not owned: the network layer owns the peer list
    // and rebuilds this view each frame.
    void update(float dt, Player& local, const std::vector<Player*>& remotes);
    void render(Renderer& r, const Player& local, const std::vector<Player*>& remotes);

    glm::vec3 randomSpawnPoint() const;
    int bulletCount() const { return (int)bullets_.size(); }
    int particleCount() const { return particles_.liveCount(); }
    const std::string& winner() const { return winner_; }
    bool hasWinner() const { return !winner_.empty(); }
    void reset();

    // Fired when the local player lands a killing blow, so the network layer
    // can report it without the arena knowing what a socket is.
    std::function<void(const std::string& victimId)> onLocalKill;
    // Every hit the local player lands. The victim owns its own health, so
    // damage is reported and applied there rather than assumed here.
    std::function<void(const std::string& victimId, float damage)> onLocalHit;
    // Every shot the local player fires, so peers can draw it.
    std::function<void(glm::vec3 origin, glm::vec3 velocity)> onLocalShot;

private:
    std::vector<Bullet> bullets_;
    ParticleSystem particles_;
    Mesh shipMesh_, bulletMesh_, boundsMesh_, starsMesh_;
    std::string winner_;

    void keepInBounds(Player& p);
};
