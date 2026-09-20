#!/usr/bin/env bash
# Builds the WASM client into build-web/.
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v emcmake >/dev/null 2>&1; then
    echo "emcmake not found. Install with: brew install emscripten" >&2
    exit 1
fi

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo
echo "Built:"
ls -lh build-web/index.html build-web/index.js build-web/index.wasm
echo
echo "Serve it:  python3 -m http.server 8000 --directory build-web"
