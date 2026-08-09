#!/usr/bin/env bash
# Compile and run host-side unit tests (no Flipper SDK).
#
# Links pure modules under src/ that have matching host tests.
# When DCF77 modules land (e.g. src/dcf77_*.c), add them to SRC_MODULES
# the same way; any tests/test_*.c present are picked up automatically.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/internet_time_all_tests"

# Pure C modules safe to compile on the host (no Flipper APIs).
# Future: append dcf77_decode.c, dcf77_time.c, etc. when those files exist.
SRC_MODULES=(
  internet_time.c
  clock_model.c
)

SRC_FILES=()
for mod in "${SRC_MODULES[@]}"; do
  SRC_FILES+=("${ROOT}/src/${mod}")
done

# Include every tests/test_*.c (test_runner.c plus suite files).
mapfile -t TEST_FILES < <(find "${ROOT}/tests" -maxdepth 1 -name 'test_*.c' | sort)

cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/src" \
  "${SRC_FILES[@]}" \
  "${TEST_FILES[@]}" \
  -o "${OUT}"

exec "${OUT}"
