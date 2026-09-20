// Frustum-culling benchmark.
//
// Renders the same scene with culling off and on, and reports frame-time
// percentiles for each. The window is hidden and vsync is disabled, because a
// vsynced frame time measures the display refresh rather than the work done.
// glFinish() before each sample stops the CPU running ahead of the GPU, which
// would otherwise time command submission instead of rendering.

#include "engine/gl.h"
#include "engine/renderer.h"
#include "engine/profiler.h"
#include "engine/mesh.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kWarmupFrames = 60;
constexpr int kMeasureFrames = 400;
constexpr float kArenaExtent = 90.0f;
constexpr float kObjectRadius = 0.3f;

float rand01() { return (float)rand() / (float)RAND_MAX; }
float randSpan(float e) { return (rand01() * 2.0f - 1.0f) * e; }

struct Sample {
    Profiler::FrameStats stats;
    int drawCalls = 0;
    int culled = 0;
    int considered = 0;
};

Sample runPass(Renderer& renderer, GLFWwindow* win, const Mesh& mesh,
               const std::vector<Transform>& objects, bool culling) {
    renderer.setCullingEnabled(culling);
    renderer.profiler().reset();

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    if (fbh == 0) fbh = 1;

    const glm::mat4 proj =
        glm::perspective(glm::radians(70.0f), (float)fbw / (float)fbh, 0.1f, 2000.0f);

    int lastDraws = 0, lastCulled = 0, lastConsidered = 0;

    for (int frame = 0; frame < kWarmupFrames + kMeasureFrames; ++frame) {
        // Spin the camera so the visible subset changes frame to frame; a fixed
        // camera would let one lucky orientation stand in for the average.
        const float angle = (float)frame * 0.01f;
        const glm::vec3 eye(0.0f);
        const glm::vec3 at(cosf(angle) * 10.0f, 0.0f, sinf(angle) * 10.0f);
        const glm::mat4 view = glm::lookAt(eye, at, glm::vec3(0, 1, 0));

        auto t0 = std::chrono::high_resolution_clock::now();

        renderer.beginFrame(fbw, fbh);
        renderer.setCamera(view, proj, eye);
        for (const auto& t : objects)
            renderer.drawLit(t, mesh, glm::vec3(0.4f, 0.8f, 1.0f), 0.2f, kObjectRadius);

        glFinish();
        auto t1 = std::chrono::high_resolution_clock::now();

        glfwSwapBuffers(win);
        glfwPollEvents();

        if (frame >= kWarmupFrames) {
            float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
            renderer.profiler().addSample(ms);
        }
        lastDraws = renderer.profiler().counters().drawCalls;
        lastCulled = renderer.profiler().counters().culled;
        lastConsidered = renderer.profiler().counters().considered;
    }

    Sample s;
    s.stats = renderer.profiler().stats();
    s.drawCalls = lastDraws;
    s.culled = lastCulled;
    s.considered = lastConsidered;
    return s;
}

void report(const char* label, const Sample& s, int total) {
    printf("  %-14s avg %6.3f ms   p50 %6.3f   p95 %6.3f   p99 %6.3f   max %6.3f   %7.1f fps\n",
           label, s.stats.avgMs, s.stats.p50Ms, s.stats.p95Ms, s.stats.p99Ms,
           s.stats.maxMs, s.stats.fps);
    if (s.considered > 0) {
        printf("  %-14s drew %d/%d  (culled %d, %.1f%% of scene)\n",
               "", s.drawCalls, total, s.culled,
               100.0f * (float)s.culled / (float)total);
    } else {
        printf("  %-14s drew %d/%d  (culling disabled)\n", "", s.drawCalls, total);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const int objectCount = (argc > 1) ? atoi(argv[1]) : 2000;

    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* win = glfwCreateWindow(1280, 720, "bench", nullptr, nullptr);
    if (!win) { fprintf(stderr, "window failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);

    Renderer renderer;
    if (!renderer.init()) { fprintf(stderr, "renderer init failed\n"); return 1; }

    Mesh mesh = primitives::makeSphere(kObjectRadius, 8, 12);

    srand(20260920);
    std::vector<Transform> objects;
    objects.reserve(objectCount);
    for (int i = 0; i < objectCount; ++i) {
        Transform t;
        t.position = glm::vec3(randSpan(kArenaExtent), randSpan(kArenaExtent),
                               randSpan(kArenaExtent));
        objects.push_back(t);
    }

    printf("\nFrustum culling benchmark\n");
    printf("  %s\n", (const char*)glGetString(GL_RENDERER));
    printf("  %d objects across a %.0f-unit arena, %d measured frames, vsync off\n\n",
           objectCount, kArenaExtent * 2.0f, kMeasureFrames);

    Sample off = runPass(renderer, win, mesh, objects, false);
    report("culling off", off, objectCount);
    printf("\n");
    Sample on = runPass(renderer, win, mesh, objects, true);
    report("culling on", on, objectCount);

    const float speedup = off.stats.avgMs > 0.0f ? off.stats.avgMs / on.stats.avgMs : 0.0f;
    const float saved = off.stats.avgMs - on.stats.avgMs;
    printf("\n  net: %.3f ms/frame saved, %.2fx faster (%.1f fps -> %.1f fps)\n\n",
           saved, speedup, off.stats.fps, on.stats.fps);

    renderer.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
