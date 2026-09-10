#!/bin/sh
# Host-side simulation of the firmware logic. No Flipper required.
cd "$(dirname "$0")"
set -e

SUITES="rules nav game challenge reflex draw"
CFLAGS="-std=gnu11 -Wall -Wextra -Werror -I stubs -I .."

for t in $SUITES; do
    gcc $CFLAGS -o "test_$t" "test_$t.c" -lm
    "./test_$t"
    echo
done

# A second pass under the sanitizers. The screens read cursors and index
# name tables, and three real out-of-range reads only showed up here.
if [ "${BB_NO_SAN:-}" != "1" ]; then
    echo "--- again, with the sanitizers ---"
    for t in $SUITES; do
        gcc $CFLAGS -g -fsanitize=address,undefined -fno-sanitize-recover=all \
            -o "san_$t" "test_$t.c" -lm
        "./san_$t" > /dev/null
        echo "ok   $t is clean under address and undefined behaviour"
    done
    echo
fi
