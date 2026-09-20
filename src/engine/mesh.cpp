#include "engine/mesh.h"
#include <glm/glm.hpp>
#include <cmath>
#include <cstdlib>

Mesh::~Mesh() { destroy(); }

void Mesh::destroy() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
}

Mesh::Mesh(Mesh&& o) noexcept { *this = std::move(o); }

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        destroy();
        vao = o.vao; vbo = o.vbo; ebo = o.ebo;
        mode = o.mode; count = o.count; indexed = o.indexed;
        o.vao = o.vbo = o.ebo = 0;
        o.count = 0;
    }
    return *this;
}

void Mesh::upload(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices) {
    destroy();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    mode = GL_TRIANGLES;
    count = (GLsizei)indices.size();
    indexed = true;
}

void Mesh::uploadPositions(const std::vector<float>& xyz, GLenum drawMode) {
    destroy();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, xyz.size() * sizeof(float), xyz.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    mode = drawMode;
    count = (GLsizei)(xyz.size() / 3);
    indexed = false;
}

void Mesh::draw() const {
    if (!count) return;
    glBindVertexArray(vao);
    if (indexed) glDrawElements(mode, count, GL_UNSIGNED_INT, 0);
    else         glDrawArrays(mode, 0, count);
    glBindVertexArray(0);
}

namespace primitives {

Mesh makeCube(float h) {
    // Six separate faces so each gets a flat normal; a shared-vertex cube
    // would average normals at the corners and look like a rounded blob.
    const glm::vec3 n[6] = {{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    const glm::vec3 t[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
    const glm::vec3 b[6] = {{0,1,0},{0,1,0},{0,1,0},{0,1,0},{0,0,-1},{0,0,1}};

    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    for (int f = 0; f < 6; ++f) {
        unsigned int base = (unsigned int)verts.size();
        for (int c = 0; c < 4; ++c) {
            float sx = (c == 0 || c == 3) ? -1.0f : 1.0f;
            float sy = (c < 2) ? -1.0f : 1.0f;
            glm::vec3 p = n[f] * h + t[f] * (sx * h) + b[f] * (sy * h);
            verts.push_back({p.x, p.y, p.z, n[f].x, n[f].y, n[f].z});
        }
        idx.insert(idx.end(), {base, base+1, base+2, base, base+2, base+3});
    }
    Mesh m; m.upload(verts, idx); return m;
}

Mesh makeSphere(float radius, int rings, int sectors) {
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    for (int r = 0; r <= rings; ++r) {
        float phi = (float)M_PI * (float)r / (float)rings;
        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * (float)M_PI * (float)s / (float)sectors;
            glm::vec3 nrm(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));
            glm::vec3 p = nrm * radius;
            verts.push_back({p.x, p.y, p.z, nrm.x, nrm.y, nrm.z});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            unsigned int a = r * (sectors + 1) + s;
            unsigned int c = a + sectors + 1;
            idx.insert(idx.end(), {a, c, a + 1, a + 1, c, c + 1});
        }
    }
    Mesh m; m.upload(verts, idx); return m;
}

Mesh makeShip() {
    // Arrowhead pointing down -Z, matching Transform::forward().
    const glm::vec3 nose( 0.0f,  0.0f, -1.6f);
    const glm::vec3 tl  (-0.9f,  0.0f,  0.7f);
    const glm::vec3 tr  ( 0.9f,  0.0f,  0.7f);
    const glm::vec3 top ( 0.0f,  0.42f, 0.45f);
    const glm::vec3 bot ( 0.0f, -0.3f,  0.45f);

    const glm::vec3 tris[][3] = {
        {nose, tl, top}, {nose, top, tr},      // upper hull
        {nose, bot, tl}, {nose, tr, bot},      // lower hull
        {tl, bot, top},  {tr, top, bot},       // tail fins
    };

    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    for (const auto& tri : tris) {
        glm::vec3 nrm = glm::normalize(glm::cross(tri[1] - tri[0], tri[2] - tri[0]));
        for (int i = 0; i < 3; ++i) {
            idx.push_back((unsigned int)verts.size());
            verts.push_back({tri[i].x, tri[i].y, tri[i].z, nrm.x, nrm.y, nrm.z});
        }
    }
    Mesh m; m.upload(verts, idx); return m;
}

Mesh makeWireBox(float h) {
    const float c[8][3] = {
        {-h,-h,-h},{ h,-h,-h},{ h, h,-h},{-h, h,-h},
        {-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}};
    const int e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
                          {6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    std::vector<float> xyz;
    for (auto& pair : e)
        for (int k = 0; k < 2; ++k)
            xyz.insert(xyz.end(), {c[pair[k]][0], c[pair[k]][1], c[pair[k]][2]});
    Mesh m; m.uploadPositions(xyz, GL_LINES); return m;
}

Mesh makeStarfield(int count, float radius) {
    // Without fixed distant reference points, flying in an empty black volume
    // reads as standing still. These are the motion cue.
    std::vector<float> xyz;
    xyz.reserve(count * 3);
    srand(1337);
    for (int i = 0; i < count; ++i) {
        glm::vec3 d(0.0f);
        do {
            d = glm::vec3(rand(), rand(), rand()) / (float)RAND_MAX * 2.0f - 1.0f;
        } while (glm::dot(d, d) < 0.01f);
        d = glm::normalize(d) * radius;
        xyz.insert(xyz.end(), {d.x, d.y, d.z});
    }
    Mesh m; m.uploadPositions(xyz, GL_POINTS); return m;
}

}  // namespace primitives
