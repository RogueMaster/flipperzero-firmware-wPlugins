#!/bin/sh
# Build the .fap. Needs ufbt and the Flipper SDK, which this repo does
# not vendor:
#
#   pip install ufbt && ufbt update
#
# ufbt fetches the SDK from update.flipperzero.one on first run, so the
# machine doing this has to be able to reach it.
set -e
cd "$(dirname "$0")"
./test/run_tests.sh
ufbt "$@"
echo
echo "the .fap is at dist/beepback.fap"
echo "with the device plugged in, 'ufbt launch' installs and runs it"
