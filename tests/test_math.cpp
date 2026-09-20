// Headless checks for the parts that fail silently rather than crash:
// rotation integration and the swept bullet test.
#include "engine/math.h"
#include "engine/physics.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

static void check(bool cond, const char* what) {
    printf("%s  %s\n", cond ? "  ok  " : "  FAIL", what);
    if (!cond) ++failures;
}

static void testQuaternionIntegration() {
    // Spin at pi/2 rad/s about Y for exactly 1s: forward should swing from
    // -Z to -X. The original spec assigned the derivative instead of adding
    // it, which lands somewhere else entirely.
    RigidBody body;
    body.angularDamping = 0.0f;
    body.angularVelocity = glm::vec3(0.0f, (float)M_PI * 0.5f, 0.0f);

    const float dt = 1.0f / 240.0f;
    for (int i = 0; i < 240; ++i) body.update(dt);

    glm::vec3 fwd = body.transform.forward();
    check(std::fabs(fwd.x - (-1.0f)) < 0.02f, "90 deg yaw sends forward to -X");
    check(std::fabs(fwd.z) < 0.02f, "  and leaves no -Z component");
    check(std::fabs(glm::length(body.transform.rotation) - 1.0f) < 1e-4f,
          "rotation stays normalized");
}

static void testNoRotationWhenStill() {
    RigidBody body;
    body.angularVelocity = glm::vec3(0.0f);
    for (int i = 0; i < 100; ++i) body.update(1.0f / 60.0f);
    check(std::fabs(body.transform.forward().z - (-1.0f)) < 1e-5f,
          "zero angular velocity leaves orientation untouched");
}

static void testSweptBulletDoesNotTunnel() {
    // One frame at 60fps moves a 120 units/sec bullet 2 units -- wider than a
    // 1-unit-radius ship. A position-only test would miss this entirely.
    glm::vec3 origin(0.0f, 0.0f, 0.0f);
    glm::vec3 dir(0.0f, 0.0f, -1.0f);
    float travel = 2.0f;
    glm::vec3 shipCenter(0.0f, 0.0f, -1.0f);
    float t = 0.0f;

    bool hit = collision::raySphere(origin, dir, shipCenter, 1.0f, t);
    check(hit && t <= travel, "swept test catches a ship mid-step");

    glm::vec3 afterStep = origin + dir * travel;
    bool naive = glm::distance(afterStep, shipCenter) < 1.0f;
    check(!naive, "  (a position-only test would have missed it)");
}

static void testRayMissesWhatIsBehindIt() {
    float t = 0.0f;
    bool hit = collision::raySphere(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                    glm::vec3(0.0f, 0.0f, 50.0f), 1.0f, t);
    check(!hit, "ray does not hit a sphere behind its origin");
}

static void testSphereOverlap() {
    check(collision::sphereSphere(glm::vec3(0.0f), 1.0f, glm::vec3(1.5f, 0, 0), 1.0f),
          "overlapping spheres collide");
    check(!collision::sphereSphere(glm::vec3(0.0f), 1.0f, glm::vec3(2.5f, 0, 0), 1.0f),
          "separated spheres do not");
}

static void testDampingIsFramerateIndependent() {
    // The same elapsed time at two step sizes should land close, or ships
    // would handle differently on a 144Hz monitor than a 60Hz one.
    auto simulate = [](float dt, int steps) {
        RigidBody b;
        b.velocity = glm::vec3(0.0f, 0.0f, -10.0f);
        b.angularDamping = 0.0f;
        for (int i = 0; i < steps; ++i) b.update(dt);
        return b.transform.position.z;
    };
    float at60 = simulate(1.0f / 60.0f, 60);
    float at240 = simulate(1.0f / 240.0f, 240);
    check(std::fabs(at60 - at240) < 0.35f, "damping is roughly framerate independent");
}

int main() {
    printf("quaternion integration\n");   testQuaternionIntegration();
    printf("rest state\n");               testNoRotationWhenStill();
    printf("swept bullets\n");            testSweptBulletDoesNotTunnel();
    printf("ray direction\n");            testRayMissesWhatIsBehindIt();
    printf("sphere overlap\n");           testSphereOverlap();
    printf("damping\n");                  testDampingIsFramerateIndependent();

    printf("\n%s\n", failures ? "FAILURES PRESENT" : "all checks passed");
    return failures ? 1 : 0;
}
