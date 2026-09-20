#pragma once
#include "engine/math.h"
#include <array>

// Six-plane view frustum extracted straight from a view-projection matrix
// (Gribb-Hartmann). No extra matrix work per object: building the frustum is
// six adds and six normalizes once per frame, and each test after that is a
// dot product.
struct Frustum {
    // xyz = outward-facing normal, w = distance. Order: L, R, B, T, N, F.
    std::array<glm::vec4, 6> planes{};

    void extract(const glm::mat4& viewProj) {
        // glm is column-major, so m[c][r]: the rows we need are strided.
        auto row = [&viewProj](int i) {
            return glm::vec4(viewProj[0][i], viewProj[1][i],
                             viewProj[2][i], viewProj[3][i]);
        };
        const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

        planes[0] = r3 + r0;   // left
        planes[1] = r3 - r0;   // right
        planes[2] = r3 + r1;   // bottom
        planes[3] = r3 - r1;   // top
        planes[4] = r3 + r2;   // near
        planes[5] = r3 - r2;   // far

        for (auto& p : planes) {
            float len = glm::length(glm::vec3(p));
            // Normalizing lets the sphere test compare against a real radius
            // instead of a scaled one.
            if (len > 1e-8f) p /= len;
        }
    }

    // Conservative: a sphere straddling a plane counts as visible. False
    // positives cost one wasted draw; false negatives would pop geometry out
    // of existence, so the test errs in the safe direction.
    bool containsSphere(const glm::vec3& center, float radius) const {
        for (const auto& p : planes) {
            if (glm::dot(glm::vec3(p), center) + p.w < -radius) return false;
        }
        return true;
    }
};
