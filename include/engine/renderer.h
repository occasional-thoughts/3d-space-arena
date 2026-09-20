#pragma once
#include "engine/gl.h"
#include "engine/math.h"
#include "engine/mesh.h"

class Renderer {
public:
    bool init();
    void shutdown();

    void beginFrame(int fbWidth, int fbHeight);
    void setCamera(const glm::mat4& view, const glm::mat4& proj, glm::vec3 eye);

    void drawLit(const Transform& t, const Mesh& mesh, glm::vec3 color, float emissive = 0.0f);
    void drawFlat(const Transform& t, const Mesh& mesh, glm::vec3 color,
                  float alpha = 1.0f, float pointSize = 2.0f);
    // Escape hatch for geometry that owns its own VAO and streams every frame
    // (the particle pool), instead of living in a static Mesh.
    void drawFlatRaw(const Transform& t, GLuint vao, GLsizei count, GLenum mode,
                     glm::vec3 color, float alpha, float pointSize);

private:
    GLuint litProgram = 0;
    GLuint flatProgram = 0;
    glm::mat4 view{1.0f}, proj{1.0f};
    glm::vec3 eyePos{0.0f};

    struct { GLint model, view, proj, color, cameraPos, emissive; } litLoc{};
    struct { GLint model, view, proj, color, alpha, pointSize; } flatLoc{};
};
