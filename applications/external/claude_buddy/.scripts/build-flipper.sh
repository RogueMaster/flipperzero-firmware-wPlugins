#!/bin/bash
# Build and optionally flash the Flipper Zero app
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$(dirname "$SCRIPT_DIR")/flipper-app"

cd "$APP_DIR"

if ! command -v ufbt &>/dev/null; then
    echo "Error: ufbt not found. Install with: pip3 install ufbt"
    exit 1
fi

echo "Building Claude Buddy for Flipper Zero..."
ufbt build

if [ "${1:-}" = "--flash" ]; then
    echo "Flashing to Flipper..."
    ufbt launch
fi

echo "Done."
