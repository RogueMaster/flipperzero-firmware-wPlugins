#!/bin/sh
# Dump every draw call the firmware makes, screen by screen.
#
#   tools/trace.sh            every screen
#   tools/trace.sh pause      just that one
#
# The screens are the one part of this game that tests cannot check
# against the browser build: a test can say a layout is inside 128x64,
# never that it is the agreed layout. A trace can. Dump both builds and
# diff them.
set -e
cd "$(dirname "$0")/.."
gcc -std=gnu11 -Wall -Wextra -Werror -I test/stubs -I . -I tools \
    -o /tmp/bb_trace tools/trace_screens.c -lm
/tmp/bb_trace "$@"
