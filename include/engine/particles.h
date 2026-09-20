#pragma once
#include "engine/gl.h"
#include "engine/renderer.h"
#include <vector>

class ParticleSystem {
public:
    void init(int capacity = 2048);
    void shutdown();

    void burst(glm::vec3 origin, glm::vec3 baseColor, int count, float speed);
    void update(float dt);
    void render(Renderer& r);

    int liveCount() const { return (int)live.size(); }

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 color;
        float life;
        float maxLife;
    };

    std::vector<Particle> live;
    int capacity = 0;

    // Positions are re-uploaded every frame, so this VBO is written with
    // GL_STREAM_DRAW and lives outside the Mesh class.
    GLuint vao = 0, vbo = 0;
    std::vector<float> scratch;
};
