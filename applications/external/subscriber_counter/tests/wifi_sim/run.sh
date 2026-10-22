#!/bin/sh
# Host test of yt_wifi.c against a scripted FlipperHTTP board (no hardware).
# yt_wifi.c is compiled next to the stub tree so its includes resolve to the stubs.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/yt-wifi-sim.XXXXXX")
cp -R "$root/tests/wifi_sim/." "$work/"
cp "$root/yt_wifi.c" "$root/yt_wifi.h" "$root/yt_parse.c" "$root/yt_parse.h" "$work/"
cc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I"$work" \
    "$work/test_yt_wifi.c" "$work/yt_wifi.c" "$work/yt_parse.c" -o "$work/test_yt_wifi"
"$work/test_yt_wifi"
