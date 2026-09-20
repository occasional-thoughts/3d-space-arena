#pragma once
#include "engine/math.h"

struct RigidBody {
    Transform transform;
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};  // radians/sec, world space
    float mass = 1.0f;
    float linearDamping = 0.6f;       // space arena, not real space: ships settle
    float angularDamping = 6.0f;

    void update(float dt) {
        transform.position += velocity * dt;

        // Integrate the quaternion derivative: q' = q + (dt/2) * w * q.
        // Adding the derivative is what makes this a rotation *step*; assigning
        // it outright would throw away the current orientation every frame.
        glm::quat spin(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
        glm::quat deltaRot = 0.5f * dt * (spin * transform.rotation);
        transform.rotation = glm::normalize(transform.rotation + deltaRot);

        // Exponential damping, framerate independent.
        velocity        *= glm::max(0.0f, 1.0f - linearDamping  * dt);
        angularVelocity *= glm::max(0.0f, 1.0f - angularDamping * dt);
    }

    void addForce(glm::vec3 f, float dt) { velocity += (f / mass) * dt; }
};
