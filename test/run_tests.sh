#!/bin/sh
# Host-side simulation of the firmware logic. No Flipper required.
cd "$(dirname "$0")"
set -e

SUITES="rules nav game challenge reflex draw save"
CFLAGS="-std=gnu11 -Wall -Wextra -Werror -I stubs -I .."

# First the half of a build the host tests cannot see: every source file
# compiled on its device path, with no BB_HOST_TEST, and linked together
# against do-nothing hardware. One definition of everything, two of
# nothing.
gcc $CFLAGS -o link_check link_check.c ../beepback_app.c ../beepback_nav.c \
    ../beepback_game.c ../beepback_rules.c ../beepback_draw.c ../beepback_intro.c \
    ../beepback_save.c ../beepback_led.c -lm
./link_check
echo

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
