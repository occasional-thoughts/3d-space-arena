#pragma once
#include "engine/gl.h"
#include <vector>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices);
    // Position-only geometry for the wireframe arena and the starfield.
    void uploadPositions(const std::vector<float>& xyz, GLenum drawMode);

    void draw() const;

    GLenum mode = GL_TRIANGLES;
    GLsizei count = 0;

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    bool indexed = false;
    void destroy();
};

namespace primitives {
Mesh makeCube(float halfExtent = 0.5f);
Mesh makeSphere(float radius = 0.5f, int rings = 16, int sectors = 24);
Mesh makeShip();
Mesh makeWireBox(float halfExtent);
Mesh makeStarfield(int count, float radius);
}  // namespace primitives
