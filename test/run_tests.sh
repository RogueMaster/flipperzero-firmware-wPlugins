#!/bin/sh
# Host-side simulation of the firmware logic. No Flipper required.
cd "$(dirname "$0")"
set -e

SUITES="rules nav game challenge reflex draw save"
CFLAGS="-std=gnu11 -Wall -Wextra -Werror -I stubs -I .."

# The device is a Cortex-M4F and the host is not, so where a bare-metal
# ARM compiler is available, build for the real target first. It is the
# only thing here that would catch a 32-bit or float assumption, and the
# undefined symbols it leaves are exactly what a .fap asks the firmware
# for at load time - so anything in that list beyond the Flipper API and
# a little libc is a symbol the app might not find on the device.
# The stubs only do their job while they say what the device's headers
# say. A drifted stub compiles perfectly here and then fails under ufbt -
# or worse, compiles there and behaves differently, the way an FS_Error
# read as a bool inverts every success test. Needs the network, so a
# failure to fetch is a skip and only a real mismatch is a failure.
# set -e would kill the script on a non-zero exit before the case below
# could report it, so take the status without tripping it
rc=0
python3 ../tools/check_stubs.py > /tmp/bb_stubs.log 2>&1 || rc=$?
case $rc in
    0) echo "ok   $(grep "shared signatures" /tmp/bb_stubs.log)" ;;
    1) echo "FAIL the stubs have drifted from the device headers"
       cat /tmp/bb_stubs.log
       exit 1 ;;
    *) echo "--   could not reach the firmware headers, skipping the stub check" ;;
esac
echo

ARMCC=arm-none-eabi-gcc
if command -v $ARMCC > /dev/null 2>&1; then
    ARMFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
        -std=gnu11 -Os -Wall -Wextra -Werror -Wdouble-promotion \
        -fdata-sections -ffunction-sections -I stubs -I .."
    rm -rf armobj && mkdir -p armobj
    for f in ../beepback_*.c; do
        $ARMCC $ARMFLAGS -c -o "armobj/$(basename "${f%.c}").o" "$f"
    done
    arm-none-eabi-ld -r -o armobj/beepback.o armobj/beepback_*.o
    extra=$(arm-none-eabi-nm -u armobj/beepback.o | awk '{print $2}' | grep -vE \
        '^(canvas_|furi_|gui_|view_port_|storage_|sequence_)|^(notification_message|message_do_not_reset|malloc|free|memcpy|memset|snprintf)$' || true)
    if [ -n "$extra" ]; then
        echo "FAIL the app wants symbols beyond the Flipper API:"
        echo "$extra" | sed 's/^/       /'
        exit 1
    fi
    echo "ok   builds clean for Cortex-M4F and asks the firmware for nothing unexpected"
    # and the stronger version of the same question, against the real table
    rc=0
    arm-none-eabi-nm -u armobj/beepback.o | awk '{print $2}' |
        python3 ../tools/check_api.py > /tmp/bb_api.log 2>&1 || rc=$?
    case $rc in
        0) echo "ok   $(cat /tmp/bb_api.log)" ;;
        1) echo "FAIL a symbol this app needs is not in the firmware's API table"
           cat /tmp/bb_api.log
           exit 1 ;;
        *) echo "--   could not reach the API table, skipping the export check" ;;
    esac
    arm-none-eabi-size -t armobj/beepback_*.o | tail -1 | \
        awk '{print "ok   " $1 " bytes of code, " $2 " of data, " $3 " of bss"}'
    echo
else
    echo "--   no arm-none-eabi-gcc, skipping the target build"
    echo
fi

# Then the half of a build the host tests cannot see: every source file
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
