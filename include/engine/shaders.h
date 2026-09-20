#pragma once

// Shader sources live here as string literals rather than files under
// assets/. The WASM target would otherwise need --preload-file and a virtual
// filesystem just to read two small shaders, and the desktop target would need
// to resolve paths relative to the binary. Embedding sidesteps both.
namespace shaders {

// Desktop macOS caps out at core profile 4.1; WebGL2 speaks GLSL ES 3.00.
// The bodies below are written in the subset both versions agree on.
#ifdef __EMSCRIPTEN__
inline const char* kVersionHeader = "#version 300 es\nprecision highp float;\n";
#else
inline const char* kVersionHeader = "#version 410 core\n";
#endif

inline const char* kLitVertex = R"(
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;
    // Good enough while scales stay uniform; a non-uniform scale would need
    // the inverse-transpose here.
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uProjection * uView * world;
}
)";

inline const char* kLitFragment = R"(
in vec3 vNormal;
in vec3 vWorldPos;

uniform vec3 uColor;
uniform vec3 uCameraPos;
uniform float uEmissive;

out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.35));
    float diffuse = max(dot(n, lightDir), 0.0);

    // Rim term: cheap, and it keeps ships readable against a black skybox.
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.5);

    vec3 ambient = uColor * 0.18;
    vec3 lit = uColor * diffuse * 0.85;
    vec3 rimLight = uColor * rim * 0.9;
    vec3 result = ambient + lit + rimLight + uColor * uEmissive;

    fragColor = vec4(result, 1.0);
}
)";

// Unlit path for bullets, the arena wireframe and particles.
inline const char* kFlatVertex = R"(
layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointSize;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    gl_PointSize = uPointSize;
}
)";

inline const char* kFlatFragment = R"(
uniform vec3 uColor;
uniform float uAlpha;

out vec4 fragColor;

void main() {
    fragColor = vec4(uColor, uAlpha);
}
)";

}  // namespace shaders
