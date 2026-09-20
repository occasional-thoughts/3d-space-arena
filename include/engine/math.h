#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // identity: w,x,y,z
    glm::vec3 scale{1.0f};

    glm::mat4 getMatrix() const {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model *= glm::mat4_cast(rotation);
        model = glm::scale(model, scale);
        return model;
    }

    // Ship-local axes. -Z is forward to match the view matrix convention.
    glm::vec3 forward() const { return rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
    glm::vec3 right()   const { return rotation * glm::vec3(1.0f, 0.0f,  0.0f); }
    glm::vec3 up()      const { return rotation * glm::vec3(0.0f, 1.0f,  0.0f); }
};

namespace collision {

inline bool sphereSphere(glm::vec3 p1, float r1, glm::vec3 p2, float r2) {
    float sum = r1 + r2;
    // Compare squared lengths: avoids a sqrt in the inner loop.
    glm::vec3 d = p1 - p2;
    return glm::dot(d, d) < sum * sum;
}

// Slab method. Returns the near hit distance in tOut when it hits.
inline bool rayAABB(glm::vec3 origin, glm::vec3 dir,
                    glm::vec3 boxMin, glm::vec3 boxMax, float& tOut) {
    float tmin = 0.0f;
    float tmax = 1e30f;
    for (int i = 0; i < 3; ++i) {
        if (glm::abs(dir[i]) < 1e-8f) {
            // Parallel to this slab: miss unless the origin is already inside it.
            if (origin[i] < boxMin[i] || origin[i] > boxMax[i]) return false;
        } else {
            float inv = 1.0f / dir[i];
            float t1 = (boxMin[i] - origin[i]) * inv;
            float t2 = (boxMax[i] - origin[i]) * inv;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = glm::max(tmin, t1);
            tmax = glm::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    tOut = tmin;
    return true;
}

// Bullets are rays, ships are spheres, so this is the one that actually fires.
inline bool raySphere(glm::vec3 origin, glm::vec3 dir,
                      glm::vec3 center, float radius, float& tOut) {
    glm::vec3 oc = origin - center;
    float b = glm::dot(oc, dir);
    float c = glm::dot(oc, oc) - radius * radius;
    if (c > 0.0f && b > 0.0f) return false;      // behind the ray and outside
    float disc = b * b - c;
    if (disc < 0.0f) return false;
    float t = -b - glm::sqrt(disc);
    if (t < 0.0f) t = 0.0f;                      // origin started inside
    tOut = t;
    return true;
}

}  // namespace collision
