#!/bin/sh
# Build the .fap.
#
#   pip install ufbt
#   ufbt update          # fetches the SDK from update.flipperzero.one
#   ./build.sh
#
# The SDK is not vendored here and cannot be, so the machine running this
# has to reach update.flipperzero.one once. Everything before that step -
# the logic, the target build, and whether the firmware exports every
# symbol this app needs - test/run_tests.sh checks without it.
#
# Built and verified against SDK 1.4.3, firmware API 87.1, target 7.
set -e
cd "$(dirname "$0")"
./test/run_tests.sh
ufbt "$@"
echo
echo "the .fap is at dist/beepback.fap"
echo "with the device plugged in, 'ufbt launch' installs and runs it"
