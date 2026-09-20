cmake_minimum_required(VERSION 3.20)
project(SpaceArena CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GAME_SOURCES
    src/main.cpp
    src/engine/mesh.cpp
    src/engine/renderer.cpp
    src/engine/particles.cpp
    src/game/player.cpp
    src/game/arena.cpp
    src/network/client.cpp
)

add_executable(game_client ${GAME_SOURCES})
target_include_directories(game_client PRIVATE include)
target_compile_options(game_client PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Headless engine checks. No GL context, so this builds and runs anywhere:
#   cmake --build build --target test_math && ./build/test_math
if(NOT EMSCRIPTEN)
    add_executable(test_math EXCLUDE_FROM_ALL tests/test_math.cpp)
    target_include_directories(test_math PRIVATE include)
    find_package(glm CONFIG QUIET)
    if(glm_FOUND)
        target_link_libraries(test_math PRIVATE glm::glm)
    endif()

    # Needs a real GL context, so it links the same deps as the client.
    add_executable(bench_render EXCLUDE_FROM_ALL
        tests/bench_render.cpp src/engine/renderer.cpp src/engine/mesh.cpp)
    target_include_directories(bench_render PRIVATE include)
endif()

if(EMSCRIPTEN)
    # GLFW and GLES3 come from Emscripten's own ports, so no find_package here.
    # glm is header-only, so the host copy works for a wasm build -- but the
    # Emscripten toolchain restricts find_path to its own sysroot, which is why
    # NO_CMAKE_FIND_ROOT_PATH is needed to see outside it.
    find_path(GLM_INCLUDE_DIR glm/glm.hpp
        HINTS /opt/homebrew/include /usr/local/include ${CMAKE_SOURCE_DIR}/third_party
        NO_CMAKE_FIND_ROOT_PATH)
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "glm headers not found. Install with: brew install glm")
    endif()
    target_include_directories(game_client PRIVATE ${GLM_INCLUDE_DIR})

    target_link_options(game_client PRIVATE
        -sUSE_GLFW=3
        -sFULL_ES3=1
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sALLOW_MEMORY_GROWTH=1
        -sEXIT_RUNTIME=0
        # main() is not run on load: the shell signs in and matchmakes first,
        # then calls it with the arena's websocket URL as argv[1].
        -sINVOKE_RUN=0
        -sEXPORTED_RUNTIME_METHODS=['callMain','ccall','cwrap']
        -sWEBSOCKET_SUBPROTOCOL='binary'
        -lwebsocket.js
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    )
    set_target_properties(game_client PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")
else()
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    target_link_libraries(game_client PRIVATE glfw glm::glm ${OPENGL_FRAMEWORK})
    if(TARGET bench_render)
        target_link_libraries(bench_render PRIVATE glfw glm::glm ${OPENGL_FRAMEWORK})
    endif()
endif()
