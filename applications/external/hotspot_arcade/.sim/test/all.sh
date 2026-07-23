#!/usr/bin/env bash
# Run every headless engine test. Requires sim/engine/build.sh to have been run.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
for t in smoke.mjs trivia.mjs duel.mjs packs.mjs reactions.mjs content.mjs; do
    node "$t"
done
echo "all engine tests passed"
