#include "game/arena.h"
#include <cstdlib>

void Arena::init() {
    shipMesh_   = primitives::makeShip();
    bulletMesh_ = primitives::makeSphere(0.28f, 8, 12);
    boundsMesh_ = primitives::makeWireBox(kHalfExtent);
    starsMesh_  = primitives::makeStarfield(1200, kHalfExtent * 7.0f);
    particles_.init(3000);
    bullets_.reserve(256);
}

void Arena::shutdown() { particles_.shutdown(); }

void Arena::reset() {
    bullets_.clear();
    winner_.clear();
}

glm::vec3 Arena::randomSpawnPoint() const {
    float r = kHalfExtent * 0.65f;
    auto axis = [r]() { return ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * r; };
    return glm::vec3(axis(), axis(), axis());
}

void Arena::spawnBullet(const Player& shooter) {
    Bullet b;
    const Transform& t = shooter.transform();
    // Start the bullet ahead of the nose so it never collides with its owner.
    b.position = t.position + t.forward() * (Player::kRadius + 1.8f);
    b.velocity = t.forward() * kBulletSpeed + shooter.body().velocity;
    b.color = shooter.color();
    b.ownerId = shooter.id();
    bullets_.push_back(b);
    if (onLocalShot) onLocalShot(b.position, b.velocity);
}

void Arena::spawnRemoteBullet(glm::vec3 origin, glm::vec3 velocity,
                              glm::vec3 color, const std::string& ownerId) {
    Bullet b;
    b.position = origin;
    b.velocity = velocity;
    b.color = color;
    b.ownerId = ownerId;
    bullets_.push_back(b);
}

void Arena::keepInBounds(Player& p) {
    // Elastic walls. A kill volume would be more punishing than fun while
    // there's nothing else out there to fly toward.
    glm::vec3& pos = p.transform().position;
    glm::vec3& vel = p.body().velocity;
    for (int i = 0; i < 3; ++i) {
        if (pos[i] >  kHalfExtent) { pos[i] =  kHalfExtent; vel[i] = -vel[i] * 0.4f; }
        if (pos[i] < -kHalfExtent) { pos[i] = -kHalfExtent; vel[i] = -vel[i] * 0.4f; }
    }
}

void Arena::update(float dt, Player& local, const std::vector<Player*>& remotes) {
    local.update(dt);
    keepInBounds(local);

    if (!local.alive() && local.respawnTimer() <= 0.0f)
        local.respawn(randomSpawnPoint());

    for (Player* p : remotes) {
        p->update(dt);
        keepInBounds(*p);
    }

    for (size_t i = 0; i < bullets_.size();) {
        Bullet& b = bullets_[i];
        b.life -= dt;

        glm::vec3 step = b.velocity * dt;
        float travel = glm::length(step);
        glm::vec3 dir = travel > 1e-6f ? step / travel : glm::vec3(0.0f, 0.0f, -1.0f);

        bool consumed = false;

        // Swept test against every ship except the shooter. Stepping the
        // position first and testing points would let fast bullets tunnel
        // straight through a ship between frames.
        const bool isLocalShot = (b.ownerId == local.id());

        auto testHit = [&](Player& target) {
            if (consumed || !target.alive() || target.id() == b.ownerId) return;
            // Peers resolve their own shots and tell us about them; resolving
            // a relayed bullet here too would double-count every hit.
            if (!isLocalShot) return;
            float t = 0.0f;
            if (collision::raySphere(b.position, dir, target.transform().position,
                                     Player::kRadius, t) && t <= travel) {
                bool killed = target.takeDamage(kBulletDamage);
                particles_.burst(b.position + dir * t, b.color, killed ? 90 : 14,
                                 killed ? 26.0f : 9.0f);
                if (onLocalHit) onLocalHit(target.id(), kBulletDamage);
                if (killed) {
                    local.addScore(1);
                    if (onLocalKill) onLocalKill(target.id());
                    if (local.score() >= kWinScore) winner_ = local.id();
                }
                consumed = true;
            }
        };

        for (Player* p : remotes) testHit(*p);
        testHit(local);

        if (consumed || b.life <= 0.0f) {
            bullets_[i] = bullets_.back();
            bullets_.pop_back();
            continue;
        }
        b.position += step;
        ++i;
    }

    particles_.update(dt);
}

void Arena::render(Renderer& r, const Player& local, const std::vector<Player*>& remotes) {
    Transform identity;

    // Stars ride along with the camera so they never get closer, which is what
    // sells them as distant rather than as nearby specks. Radius 0 opts both
    // the starfield and the arena shell out of culling: they are centred on the
    // camera and larger than the frustum respectively, so a sphere test would
    // always pass and only waste work.
    Transform starXform;
    starXform.position = local.transform().position;
    r.drawFlat(starXform, starsMesh_, glm::vec3(0.75f, 0.8f, 1.0f), 0.85f, 2.0f, 0.0f);

    r.drawFlat(identity, boundsMesh_, glm::vec3(0.15f, 0.55f, 0.75f), 0.55f, 2.0f, 0.0f);

    for (const Player* p : remotes) {
        if (!p->alive()) continue;
        Transform t = p->transform();
        t.scale = glm::vec3(1.0f);
        r.drawLit(t, shipMesh_, p->color(), 0.12f, kShipBoundingRadius);
    }

    if (local.alive()) {
        // Third-person: the local ship is drawn too, camera sits behind it.
        Transform t = local.transform();
        r.drawLit(t, shipMesh_, local.color(), 0.18f, kShipBoundingRadius);
    }

    for (const auto& b : bullets_) {
        Transform t;
        t.position = b.position;
        r.drawLit(t, bulletMesh_, b.color, 0.9f, kBulletBoundingRadius);
    }

    particles_.render(r);
}
