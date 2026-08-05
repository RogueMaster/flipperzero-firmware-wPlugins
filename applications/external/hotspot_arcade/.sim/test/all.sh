#!/usr/bin/env bash
# Run every headless engine test. Requires sim/engine/build.sh to have been run.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
for t in smoke.mjs identity.mjs trivia.mjs duel.mjs packs.mjs reactions.mjs content.mjs guesscolor.mjs battleship.mjs spectrum.mjs kmk.mjs secrets.mjs gamevote.mjs wyr.mjs utf8.mjs lang.mjs chess.mjs; do
    node "$t"
done
echo "all engine tests passed"
