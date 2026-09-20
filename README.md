# Space Arena

A 3D multiplayer space shooter. One C++17 engine, two build targets: a native
macOS binary for development and a WebAssembly build that anyone can play from
a URL. Spring Boot handles accounts, matchmaking and the socket relay.

```
src/engine   math, physics, renderer, particles   (target-agnostic)
src/game     player controller, arena rules       (target-agnostic)
src/network  transport                            (the one target-specific file)
backend/     Spring Boot + JPA + WebSocket
web/         the HTML shell the WASM build is injected into
```

## Why two targets

The engine is the same C++ either way. Only `src/network/client.cpp` forks,
because a browser will not hand a WASM module a raw TCP socket — the web build
borrows the page's `WebSocket` object instead. Everything above the transport
(`NetworkClient`'s interface) is shared, so the game layer never knows which
one it is running on.

The native build is for iteration: faster compiles, real debuggers, GPU
profilers. The web build is how people actually play, because "download this
unsigned binary and click through Gatekeeper" loses almost everyone.

## Build

**Native (macOS):**

```bash
brew install glfw glm
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build -j8
./build/game_client
```

No GL loader is needed. macOS exposes core-profile entry points through
`<OpenGL/gl3.h>` and Emscripten does the same through `<GLES3/gl3.h>`, so
there is no GLAD or GLEW in the tree.

**Web:**

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
source ~/emsdk/emsdk_env.sh
cd - && ./scripts/build-web.sh
```

Use emsdk rather than `brew install emscripten`. The Homebrew formula pulls a
full LLVM as a separate dependency and took over an hour here without producing
a working `wasm-ld`; emsdk ships one prebuilt toolchain that works out of the
box. Output is `build-web/` (index.html + index.js + a ~83KB index.wasm).

**Backend:**

```bash
cd backend && mvn spring-boot:run     # H2 in memory, no MySQL required
```

**Everything at once:**

```bash
docker compose up --build             # MySQL + backend + nginx on :8000
```

## Controls

<kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> thrust, mouse aims,
<kbd>Q</kbd>/<kbd>E</kbd> roll, <kbd>Shift</kbd> boost, <kbd>Space</kbd>/<kbd>C</kbd>
climb and dive, click or <kbd>F</kbd> fires, <kbd>Esc</kbd> frees the cursor.

Mouse look is applied in the ship's local frame rather than as world yaw/pitch,
which is what stops the controls inverting after a roll.

## API

| Method | Path | Notes |
|---|---|---|
| POST | `/auth/register` | 409 if the callsign is taken |
| POST | `/auth/login` | Returns a bearer token |
| POST | `/auth/logout` | Revokes it |
| POST | `/matchmaking/join` | Bearer token required; returns an arena and its socket URL |
| GET | `/leaderboard?limit=20` | Ordered by score, then wins |
| WS | `/game?arena=<id>` | State relay, scoped to one arena |

Wire format is flat JSON, one frame per tick:

```json
{"type":"state","id":"p1234","px":1.5,"py":0,"pz":-3,"qw":1,"qx":0,"qy":0,"qz":0,"hp":100,"score":2,"alive":1}
```

## Testing

```bash
cmake --build build --target test_math && ./build/test_math
cd backend && mvn test
```

`test_math` covers the things that fail silently rather than crash: quaternion
integration, the swept bullet test, and framerate-independent damping. It needs
no GL context.

The web build also exports a headless test surface (`arena_test_*`, see the
bottom of `src/main.cpp`). It exists because "hold W and watch" is not a usable
test in an automated browser: `requestAnimationFrame` is throttled hard when a
page is backgrounded or embedded, and at ~1fps a key press and its release both
land between frames, so the engine never observes a held key. These entry
points drive the same simulation on a fixed timestep:

```js
Module.ccall('arena_test_input', null,
             ['number','number','number','number','number','number','number'],
             [1,0,0,0,0,0,0]);          // hold forward
Module.ccall('arena_test_step', null, ['number','number'], [1/60, 120]);
Module.ccall('arena_test_speed', 'number', [], []);
```

## Decisions worth knowing about

**20 Hz network tick, not 60.** The original plan sent a full state update
every 16ms. That is 60 packets per second per player of mostly redundant data.
The client sends at 20 Hz and eases peers toward their last known transform
each frame, which looks the same and costs a third of the bandwidth.

**Matchmaking starts immediately.** Queueing until four players gather
deadlocks a game nobody plays yet — the first two arrivals would wait forever.
Players join the emptiest arena with a free slot and fly straight away; the
arena fills as others arrive.

**Bullets are swept, not stepped.** At 120 units/sec a bullet moves ~2 units
per frame, which is larger than a ship. Testing the bullet's position after
moving it would let it tunnel clean through a target. `raySphere` tests the
whole segment instead.

**Elastic arena walls.** A kill volume would be more punishing than fun while
there is nothing outside the box to fly toward.

**The player id is the account callsign, not a random string.** The backend
credits kills with `findByUsername(playerId)`. While the client generated its
own random id, that lookup never matched and every kill was silently dropped on
the floor -- the leaderboard would have stayed at zero forever. The web shell
now passes the signed-in callsign to `main()` as `argv[2]`.

**Tokens are opaque and in-memory, not JWT.** One backend process, no third
party validating tokens, and logout is real revocation instead of a blocklist.
A restart logs everyone out and a second instance would not share the map —
that is the point to move to Redis or JWT, not before.

## Known limitations

**The server relays, it does not simulate.** Clients own their own position and
report their own kills. A modified client can claim anything. Making this
authoritative means moving bullet simulation and hit detection into
`GameWebSocketHandler` and sending input intent rather than state — the wire
format already carries velocity, which is most of what that needs.

**The native build has no transport.** It runs as an offline sandbox.
`NetworkClient`'s interface is already shaped for IXWebSocket; nothing in the
game layer changes when it is added.

**Particles share one colour per frame.** The pool draws in a single call with
an averaged tint. Per-particle colour needs a second vertex attribute.

**No LOD or frustum culling yet.** At current arena sizes and ship counts the
GPU is not the constraint.
