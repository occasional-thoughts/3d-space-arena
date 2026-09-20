#include "engine/renderer.h"
#include "engine/shaders.h"
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <string>

namespace {

GLuint compile(GLenum type, const char* body) {
    std::string src = std::string(shaders::kVersionHeader) + body;
    const char* cstr = src.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "[shader] %s compile failed:\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "[shader] link failed:\n%s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

}  // namespace

bool Renderer::init() {
    litProgram  = link(shaders::kLitVertex,  shaders::kLitFragment);
    flatProgram = link(shaders::kFlatVertex, shaders::kFlatFragment);
    if (!litProgram || !flatProgram) return false;

    litLoc.model     = glGetUniformLocation(litProgram, "uModel");
    litLoc.view      = glGetUniformLocation(litProgram, "uView");
    litLoc.proj      = glGetUniformLocation(litProgram, "uProjection");
    litLoc.color     = glGetUniformLocation(litProgram, "uColor");
    litLoc.cameraPos = glGetUniformLocation(litProgram, "uCameraPos");
    litLoc.emissive  = glGetUniformLocation(litProgram, "uEmissive");

    flatLoc.model     = glGetUniformLocation(flatProgram, "uModel");
    flatLoc.view      = glGetUniformLocation(flatProgram, "uView");
    flatLoc.proj      = glGetUniformLocation(flatProgram, "uProjection");
    flatLoc.color     = glGetUniformLocation(flatProgram, "uColor");
    flatLoc.alpha     = glGetUniformLocation(flatProgram, "uAlpha");
    flatLoc.pointSize = glGetUniformLocation(flatProgram, "uPointSize");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef __EMSCRIPTEN__
    // WebGL2 always honours gl_PointSize; desktop core profile needs asking.
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif
    return true;
}

void Renderer::shutdown() {
    if (litProgram)  glDeleteProgram(litProgram);
    if (flatProgram) glDeleteProgram(flatProgram);
    litProgram = flatProgram = 0;
}

void Renderer::beginFrame(int fbWidth, int fbHeight) {
    glViewport(0, 0, fbWidth, fbHeight);
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::setCamera(const glm::mat4& v, const glm::mat4& p, glm::vec3 eye) {
    view = v; proj = p; eyePos = eye;
}

void Renderer::drawLit(const Transform& t, const Mesh& mesh, glm::vec3 color, float emissive) {
    glUseProgram(litProgram);
    glm::mat4 model = t.getMatrix();
    glUniformMatrix4fv(litLoc.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(litLoc.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(litLoc.proj,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(litLoc.color, 1, glm::value_ptr(color));
    glUniform3fv(litLoc.cameraPos, 1, glm::value_ptr(eyePos));
    glUniform1f(litLoc.emissive, emissive);
    mesh.draw();
}

void Renderer::drawFlat(const Transform& t, const Mesh& mesh, glm::vec3 color,
                        float alpha, float pointSize) {
    glUseProgram(flatProgram);
    glm::mat4 model = t.getMatrix();
    glUniformMatrix4fv(flatLoc.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(flatLoc.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(flatLoc.proj,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(flatLoc.color, 1, glm::value_ptr(color));
    glUniform1f(flatLoc.alpha, alpha);
    glUniform1f(flatLoc.pointSize, pointSize);
    mesh.draw();
}

void Renderer::drawFlatRaw(const Transform& t, GLuint vaoId, GLsizei count, GLenum mode,
                           glm::vec3 color, float alpha, float pointSize) {
    if (!count) return;
    glUseProgram(flatProgram);
    glm::mat4 model = t.getMatrix();
    glUniformMatrix4fv(flatLoc.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(flatLoc.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(flatLoc.proj,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(flatLoc.color, 1, glm::value_ptr(color));
    glUniform1f(flatLoc.alpha, alpha);
    glUniform1f(flatLoc.pointSize, pointSize);
    glBindVertexArray(vaoId);
    glDrawArrays(mode, 0, count);
    glBindVertexArray(0);
}
