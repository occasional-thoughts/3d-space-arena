#!/usr/bin/env bash
# Starts the backend (H2 in memory, no MySQL needed) and serves the web build.
set -euo pipefail
cd "$(dirname "$0")/.."

JAR=backend/target/space-arena-backend-0.1.0.jar
[ -f "$JAR" ] || (cd backend && mvn -q clean package -DskipTests)

java -jar "$JAR" &
BACKEND_PID=$!
trap 'kill $BACKEND_PID 2>/dev/null || true' EXIT

echo "backend on :8080 — serving client on http://localhost:8000"
python3 -m http.server 8000 --directory build-web
