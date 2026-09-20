#pragma once
#include "engine/gl.h"
#include "engine/math.h"
#include "engine/mesh.h"
#include "engine/frustum.h"
#include "engine/profiler.h"

class Renderer {
public:
    bool init();
    void shutdown();

    void beginFrame(int fbWidth, int fbHeight);
    // Rebuilds the frustum from view*proj, so it must be called before any draw.
    void setCamera(const glm::mat4& view, const glm::mat4& proj, glm::vec3 eye);

    // boundingRadius <= 0 opts an object out of culling (used for geometry
    // that is effectively unbounded, like the arena shell and the starfield).
    void drawLit(const Transform& t, const Mesh& mesh, glm::vec3 color,
                 float emissive = 0.0f, float boundingRadius = 0.0f);
    void drawFlat(const Transform& t, const Mesh& mesh, glm::vec3 color,
                  float alpha = 1.0f, float pointSize = 2.0f,
                  float boundingRadius = 0.0f);
    // Escape hatch for geometry that owns its own VAO and streams every frame
    // (the particle pool), instead of living in a static Mesh.
    void drawFlatRaw(const Transform& t, GLuint vao, GLsizei count, GLenum mode,
                     glm::vec3 color, float alpha, float pointSize);

    // Culling is switchable so the same build can measure itself with the
    // optimisation on and off.
    void setCullingEnabled(bool on) { cullingEnabled_ = on; }
    bool cullingEnabled() const { return cullingEnabled_; }

    Profiler& profiler() { return profiler_; }
    const Profiler& profiler() const { return profiler_; }
    const Frustum& frustum() const { return frustum_; }

    // Shared gate: true when the object should be drawn.
    bool testVisible(const glm::vec3& center, float radius);

private:
    Profiler profiler_;
    Frustum frustum_;
    bool cullingEnabled_ = true;

    GLuint litProgram = 0;
    GLuint flatProgram = 0;
    glm::mat4 view{1.0f}, proj{1.0f};
    glm::vec3 eyePos{0.0f};

    struct { GLint model, view, proj, color, cameraPos, emissive; } litLoc{};
    struct { GLint model, view, proj, color, alpha, pointSize; } flatLoc{};
};
