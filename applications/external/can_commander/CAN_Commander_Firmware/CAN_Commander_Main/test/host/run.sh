#!/bin/sh
# Builds and runs the host regression tests with AddressSanitizer and
# UndefinedBehaviorSanitizer. Usage: test/host/run.sh
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../src"
OUT="${TMPDIR:-/tmp}/can_commander_host_tests"
mkdir -p "$OUT"

CXX="${CXX:-c++}"
"$CXX" -std=c++17 -g -O1 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -I"$HERE/stubs" -I"$HERE" -I"$SRC" \
  "$HERE/test_main.cpp" \
  "$SRC"/decode/*.cpp \
  "$SRC"/tools/*.cpp \
  "$SRC"/io/CliShell.cpp \
  "$SRC"/io/FlipperProtocol.cpp \
  "$SRC"/cmd/CommandRouter.cpp \
  -o "$OUT/host_tests"

"$OUT/host_tests"
