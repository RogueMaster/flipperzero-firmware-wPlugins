#!/bin/sh
# Host-side simulation of the firmware logic. No Flipper required.
cd "$(dirname "$0")"
set -e
CFLAGS="-std=gnu11 -Wall -Wextra -Werror -I stubs -I .."
for t in rules nav game; do
    gcc $CFLAGS -o "test_$t" "test_$t.c" -lm
    "./test_$t"
    echo
done
