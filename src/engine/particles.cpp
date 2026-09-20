#include "engine/particles.h"
#include <cstdlib>

namespace {
float rand01() { return (float)rand() / (float)RAND_MAX; }
float randSigned() { return rand01() * 2.0f - 1.0f; }
}

void ParticleSystem::init(int cap) {
    capacity = cap;
    live.reserve(cap);
    scratch.reserve(cap * 3);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cap * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void ParticleSystem::shutdown() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = 0;
}

void ParticleSystem::burst(glm::vec3 origin, glm::vec3 baseColor, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        if ((int)live.size() >= capacity) break;
        glm::vec3 dir(randSigned(), randSigned(), randSigned());
        float len2 = glm::dot(dir, dir);
        if (len2 < 1e-6f) continue;
        dir = glm::normalize(dir);

        Particle p;
        p.position = origin;
        // Vary the speed per particle so the burst front isn't a hard shell.
        p.velocity = dir * speed * (0.35f + rand01() * 0.65f);
        // Push each particle toward white at its core, tinted by baseColor.
        p.color = glm::mix(baseColor, glm::vec3(1.0f), rand01() * 0.5f);
        p.maxLife = 0.5f + rand01() * 0.7f;
        p.life = p.maxLife;
        live.push_back(p);
    }
}

void ParticleSystem::update(float dt) {
    for (size_t i = 0; i < live.size();) {
        Particle& p = live[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            // Swap-and-pop: order doesn't matter for particles, and this keeps
            // removal O(1) instead of shifting the tail every death.
            live[i] = live.back();
            live.pop_back();
            continue;
        }
        p.position += p.velocity * dt;
        p.velocity *= glm::max(0.0f, 1.0f - 1.2f * dt);
        ++i;
    }
}

void ParticleSystem::render(Renderer& r) {
    if (live.empty()) return;

    scratch.clear();
    for (const auto& p : live)
        scratch.insert(scratch.end(), {p.position.x, p.position.y, p.position.z});

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, scratch.size() * sizeof(float), scratch.data());

    // Additive blending makes overlapping sparks read as heat rather than mud.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    Transform identity;
    glm::vec3 avgColor(0.0f);
    float avgLife = 0.0f;
    for (const auto& p : live) { avgColor += p.color; avgLife += p.life / p.maxLife; }
    avgColor /= (float)live.size();
    avgLife  /= (float)live.size();

    // One draw call for the whole pool. Per-particle colour would need a second
    // attribute; the averaged tint is close enough at these lifetimes.
    r.drawFlatRaw(identity, vao, (GLsizei)live.size(), GL_POINTS,
                  avgColor, glm::clamp(avgLife, 0.0f, 1.0f), 5.0f);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
