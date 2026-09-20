#include "engine/gl.h"
#include "engine/renderer.h"
#include "game/arena.h"
#include "game/player.h"
#include "network/client.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int   kWidth      = 1280;
constexpr int   kHeight     = 720;
constexpr float kNetTickHz  = 20.0f;   // 20 Hz + interpolation, not 60 Hz raw
constexpr float kMaxDt      = 0.05f;

struct RemoteView {
    std::unique_ptr<Player> player;
    glm::vec3 targetPos{0.0f};
    glm::quat targetRot{1.0f, 0.0f, 0.0f, 0.0f};
};

struct App {
    GLFWwindow* window = nullptr;
    Renderer renderer;
    Arena arena;
    Player local;
    NetworkClient net;

    std::unordered_map<std::string, RemoteView> remoteViews;
    std::vector<Player*> remoteList;   // rebuilt each frame from remoteViews

    InputState input;
    double lastTime = 0.0;
    double lastMouseX = 0.0, lastMouseY = 0.0;
    bool mouseCaptured = false;
    bool firstMouse = true;
    float netAccumulator = 0.0f;
    float hudTimer = 0.0f;
};

App g_app;

std::string makePlayerId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "p%04d", rand() % 10000);
    return buf;
}

glm::vec3 colorForId(const std::string& id) {
    // Deterministic hue per id so the same ship is the same colour on every
    // client without the server having to assign palettes.
    unsigned h = 2166136261u;
    for (char c : id) { h ^= (unsigned char)c; h *= 16777619u; }
    float hue = (float)(h % 360) / 360.0f;
    float r = fabsf(hue * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - fabsf(hue * 6.0f - 2.0f);
    float b = 2.0f - fabsf(hue * 6.0f - 4.0f);
    glm::vec3 c = glm::clamp(glm::vec3(r, g, b), 0.0f, 1.0f);
    return glm::mix(c, glm::vec3(1.0f), 0.25f);  // lift so nothing reads black
}

void setMouseCapture(bool on) {
    g_app.mouseCaptured = on;
    glfwSetInputMode(g_app.window, GLFW_CURSOR,
                     on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    g_app.firstMouse = true;
}

void onMouseButton(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // The browser only grants pointer lock inside a user gesture, so the
        // first click is what arms the controls on both targets.
        if (!g_app.mouseCaptured) setMouseCapture(true);
        else g_app.input.fire = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        g_app.input.fire = false;
}

void onKey(GLFWwindow* win, int key, int, int action, int) {
    bool down = (action != GLFW_RELEASE);
    switch (key) {
        case GLFW_KEY_W: g_app.input.forward = down; break;
        case GLFW_KEY_S: g_app.input.back    = down; break;
        case GLFW_KEY_A: g_app.input.left    = down; break;
        case GLFW_KEY_D: g_app.input.right   = down; break;
        case GLFW_KEY_SPACE: g_app.input.up  = down; break;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_C: g_app.input.down    = down; break;
        case GLFW_KEY_LEFT_SHIFT: g_app.input.boost = down; break;
        case GLFW_KEY_Q: g_app.input.rollLeft  = down ? 1.0f : 0.0f; break;
        case GLFW_KEY_E: g_app.input.rollRight = down ? 1.0f : 0.0f; break;
        case GLFW_KEY_F: if (down) g_app.input.fire = true; else g_app.input.fire = false; break;
        case GLFW_KEY_ESCAPE:
            if (down) setMouseCapture(false);
            break;
        default: break;
    }
    (void)win;
}

void pumpMouse() {
    double x, y;
    glfwGetCursorPos(g_app.window, &x, &y);
    if (g_app.firstMouse) {
        g_app.lastMouseX = x;
        g_app.lastMouseY = y;
        g_app.firstMouse = false;
    }
    if (g_app.mouseCaptured) {
        g_app.input.mouseDX = (float)(x - g_app.lastMouseX);
        g_app.input.mouseDY = (float)(y - g_app.lastMouseY);
    } else {
        g_app.input.mouseDX = g_app.input.mouseDY = 0.0f;
    }
    g_app.lastMouseX = x;
    g_app.lastMouseY = y;
}

void applyPeerState(const PlayerState& s) {
    auto it = g_app.remoteViews.find(s.id);
    if (it == g_app.remoteViews.end()) {
        RemoteView view;
        view.player = std::make_unique<Player>(s.id, colorForId(s.id));
        view.player->transform().position = s.position;
        view.player->transform().rotation = s.rotation;
        view.targetPos = s.position;
        view.targetRot = s.rotation;
        auto [inserted, ok] = g_app.remoteViews.emplace(s.id, std::move(view));
        (void)ok;
        it = inserted;
    }
    RemoteView& view = it->second;
    view.targetPos = s.position;
    view.targetRot = s.rotation;
    view.player->body().velocity = s.velocity;
    view.player->setHealth(s.health);
    view.player->setScore(s.score);
}

void interpolateRemotes(float dt) {
    // Snapping to each 20 Hz packet would stutter visibly. Easing toward the
    // last received transform costs one lerp and hides the tick rate.
    const float k = glm::min(1.0f, dt * 12.0f);
    g_app.remoteList.clear();
    g_app.remoteList.reserve(g_app.remoteViews.size());
    for (auto& kv : g_app.remoteViews) {
        RemoteView& v = kv.second;
        Transform& t = v.player->transform();
        t.position = glm::mix(t.position, v.targetPos, k);
        t.rotation = glm::slerp(t.rotation, v.targetRot, k);
        g_app.remoteList.push_back(v.player.get());
    }
}

void frame() {
    double now = glfwGetTime();
    float dt = (float)(now - g_app.lastTime);
    g_app.lastTime = now;
    // A long stall (tab backgrounded, window dragged) would otherwise teleport
    // every ship across the arena in one step.
    if (dt > kMaxDt) dt = kMaxDt;

    glfwPollEvents();
    pumpMouse();

    g_app.local.applyInput(g_app.input, dt);
    if (g_app.input.fire && g_app.local.canFire()) {
        g_app.arena.spawnBullet(g_app.local);
        g_app.local.noteFired();
    }

    g_app.net.poll();

    // Ease peers toward their last received transform, then hand the arena a
    // borrowed view of them for collision and drawing.
    interpolateRemotes(dt);
    g_app.arena.update(dt, g_app.local, g_app.remoteList);

    g_app.netAccumulator += dt;
    if (g_app.netAccumulator >= 1.0f / kNetTickHz) {
        g_app.netAccumulator = 0.0f;
        if (g_app.net.isConnected()) {
            PlayerState s;
            s.id = g_app.local.id();
            s.position = g_app.local.transform().position;
            s.rotation = g_app.local.transform().rotation;
            s.velocity = g_app.local.body().velocity;
            s.health = g_app.local.health();
            s.score = g_app.local.score();
            s.alive = g_app.local.alive();
            g_app.net.sendState(s);
        }
    }

    // Chase camera: sit behind and above the ship, look where it's heading.
    const Transform& lt = g_app.local.transform();
    glm::vec3 eye = lt.position - lt.forward() * 9.0f + lt.up() * 3.2f;
    glm::mat4 view = glm::lookAt(eye, lt.position + lt.forward() * 6.0f, lt.up());

    int fbw, fbh;
    glfwGetFramebufferSize(g_app.window, &fbw, &fbh);
    if (fbh == 0) fbh = 1;
    glm::mat4 proj = glm::perspective(glm::radians(70.0f),
                                      (float)fbw / (float)fbh, 0.1f, 2000.0f);

    g_app.renderer.beginFrame(fbw, fbh);
    g_app.renderer.setCamera(view, proj, eye);
    g_app.arena.render(g_app.renderer, g_app.local, g_app.remoteList);

    glfwSwapBuffers(g_app.window);

    g_app.hudTimer += dt;
    if (g_app.hudTimer > 1.0f) {
        g_app.hudTimer = 0.0f;
        glm::vec3 v = g_app.local.body().velocity;
        glm::vec3 p = g_app.local.transform().position;
        printf("[hud] hp=%.0f score=%d peers=%zu %s | pos %.0f,%.0f,%.0f vel %.1f\n",
               g_app.local.health(), g_app.local.score(),
               g_app.remoteViews.size(),
               g_app.net.isConnected() ? "online" : "offline",
               p.x, p.y, p.z, glm::length(v));
        fflush(stdout);
    }
}

// --- Headless test surface -------------------------------------------------
// The browser throttles requestAnimationFrame hard when a page is backgrounded
// or embedded, which makes "hold W and watch" useless as a test: at 1fps a key
// press and its release both land between frames. These entry points drive the
// same simulation on a fixed timestep so flight, firing and collision can be
// verified without depending on the frame clock.
#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE void arena_test_input(int forward, int back, int left, int right,
                                           int fire, float mouseDX, float mouseDY) {
    g_app.input.forward = forward != 0;
    g_app.input.back    = back != 0;
    g_app.input.left    = left != 0;
    g_app.input.right   = right != 0;
    g_app.input.fire    = fire != 0;
    g_app.input.mouseDX = mouseDX;
    g_app.input.mouseDY = mouseDY;
}

// One fixed-timestep tick of input -> physics -> arena, with no rendering.
EMSCRIPTEN_KEEPALIVE void arena_test_step(float dt, int steps) {
    for (int i = 0; i < steps; ++i) {
        g_app.local.applyInput(g_app.input, dt);
        if (g_app.input.fire && g_app.local.canFire()) {
            g_app.arena.spawnBullet(g_app.local);
            g_app.local.noteFired();
        }
        interpolateRemotes(dt);
        g_app.arena.update(dt, g_app.local, g_app.remoteList);
        // Mouse deltas are per-event, not per-frame: consume them.
        g_app.input.mouseDX = g_app.input.mouseDY = 0.0f;
    }
}

EMSCRIPTEN_KEEPALIVE float arena_test_pos(int axis) {
    return g_app.local.transform().position[axis];
}
EMSCRIPTEN_KEEPALIVE float arena_test_forward(int axis) {
    return g_app.local.transform().forward()[axis];
}
EMSCRIPTEN_KEEPALIVE float arena_test_speed() {
    return glm::length(g_app.local.body().velocity);
}
EMSCRIPTEN_KEEPALIVE float arena_test_health() { return g_app.local.health(); }
EMSCRIPTEN_KEEPALIVE int arena_test_peers() { return (int)g_app.remoteViews.size(); }
// Drains the socket without needing a rendered frame.
EMSCRIPTEN_KEEPALIVE void arena_test_poll() { g_app.net.poll(); }
EMSCRIPTEN_KEEPALIVE float arena_test_peer_pos(int axis) {
    for (auto& kv : g_app.remoteViews)
        if (kv.first != "dummy") return kv.second.targetPos[axis];
    return -999.0f;
}
EMSCRIPTEN_KEEPALIVE int arena_test_bullets() { return g_app.arena.bulletCount(); }
EMSCRIPTEN_KEEPALIVE int arena_test_particles() { return g_app.arena.particleCount(); }

// Drops a stationary target in front of the ship so hit detection can be
// exercised without a second browser.
EMSCRIPTEN_KEEPALIVE void arena_test_spawn_dummy(float distance) {
    const std::string id = "dummy";
    RemoteView view;
    view.player = std::make_unique<Player>(id, glm::vec3(1.0f, 0.4f, 0.4f));
    const Transform& lt = g_app.local.transform();
    glm::vec3 spot = lt.position + lt.forward() * distance;
    view.player->respawn(spot);
    view.targetPos = spot;
    view.targetRot = lt.rotation;
    g_app.remoteViews[id] = std::move(view);
}

EMSCRIPTEN_KEEPALIVE float arena_test_dummy_health() {
    auto it = g_app.remoteViews.find("dummy");
    return it == g_app.remoteViews.end() ? -1.0f : it->second.player->health();
}

}  // extern "C"
#endif

}  // namespace

int main(int argc, char** argv) {
    srand((unsigned)time(nullptr));

    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#ifndef __EMSCRIPTEN__
    // macOS only exposes 3.2+ through a forward-compatible core profile.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    g_app.window = glfwCreateWindow(kWidth, kHeight, "Space Arena", nullptr, nullptr);
    if (!g_app.window) {
        fprintf(stderr, "window creation failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(g_app.window);
#ifndef __EMSCRIPTEN__
    glfwSwapInterval(1);
#endif

    printf("[gl] %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    if (!g_app.renderer.init()) {
        fprintf(stderr, "renderer init failed\n");
        return 1;
    }

    g_app.arena.init();

    // argv[2] is the signed-in callsign, passed by the web shell. It must match
    // the account name or the backend's kill bookkeeping (which looks players
    // up by username) silently records nothing. The random id is only a
    // fallback for the native sandbox, where there is no login.
    std::string id = (argc > 2 && argv[2] && argv[2][0]) ? std::string(argv[2])
                                                         : makePlayerId();
    g_app.local = Player(id, colorForId(id));
    g_app.local.respawn(g_app.arena.randomSpawnPoint());

    g_app.net.onPeerState = applyPeerState;
    g_app.net.onPeerLeft = [](const std::string& pid) { g_app.remoteViews.erase(pid); };
    g_app.arena.onLocalKill = [](const std::string& victim) { g_app.net.sendKill(victim); };
    g_app.arena.onLocalHit = [](const std::string& victim, float dmg) {
        g_app.net.sendHit(victim, dmg);
    };
    g_app.arena.onLocalShot = [](glm::vec3 origin, glm::vec3 velocity) {
        g_app.net.sendShot(origin, velocity);
    };
    // A peer says it hit us. Health is owned by the client it belongs to, so
    // the damage is applied here rather than on the shooter's machine.
    g_app.net.onLocalDamaged = [](float dmg) { g_app.local.takeDamage(dmg); };
    g_app.net.onPeerShot = [](const std::string& shooterId, glm::vec3 origin,
                              glm::vec3 velocity) {
        g_app.arena.spawnRemoteBullet(origin, velocity, colorForId(shooterId), shooterId);
    };

    std::string url = (argc > 1) ? argv[1] : "ws://localhost:8080/game";
    g_app.net.connect(url, id);

    glfwSetKeyCallback(g_app.window, onKey);
    glfwSetMouseButtonCallback(g_app.window, onMouseButton);

    g_app.lastTime = glfwGetTime();

    printf("[game] click to capture mouse. WASD thrust, mouse aim, Q/E roll, "
           "Shift boost, Space/C up-down, click or F to fire, Esc releases.\n");

#ifdef __EMSCRIPTEN__
    // The browser owns the frame clock; handing it a while(1) would hang the tab.
    //
    // The last argument must be 0 (do not simulate an infinite loop). With 1,
    // Emscripten throws an 'unwind' exception to escape main() -- which the
    // runtime swallows on a normal startup, but here main() is invoked by hand
    // from the shell's launch button via callMain(), and the throw escapes into
    // that click handler and kills the frame callback before it ever runs.
    // Returning normally leaves the loop registered and the runtime alive
    // (EXIT_RUNTIME=0).
    emscripten_set_main_loop(frame, 0, 0);
#else
    while (!glfwWindowShouldClose(g_app.window)) frame();
    g_app.arena.shutdown();
    g_app.renderer.shutdown();
    glfwDestroyWindow(g_app.window);
    glfwTerminate();
#endif
    return 0;
}
