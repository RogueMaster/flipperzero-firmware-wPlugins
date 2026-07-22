#!/usr/bin/env bash
# Serve the repo root so the harness can reach both sim/web/ and web/dist/.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${PORT:-8123}"
echo "==> http://localhost:$PORT/sim/web/"
cd "$REPO" && python3 -m http.server "$PORT"
