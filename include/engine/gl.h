#pragma once

// Single place where the two build targets diverge on GL.
// macOS ships core-profile entry points directly in <OpenGL/gl3.h>, and
// Emscripten exposes GLES3 the same way, so neither target needs a loader
// library (no GLAD, no GLEW).
#ifdef __EMSCRIPTEN__
  #include <GLES3/gl3.h>
  #include <emscripten/emscripten.h>
#else
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#endif

#include <GLFW/glfw3.h>
