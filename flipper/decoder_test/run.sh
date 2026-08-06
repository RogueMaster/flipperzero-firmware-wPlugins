#!/bin/sh
# Runs the firmware decoder (tpms_renault.c) on the host, no Flipper needed.
set -e
cd "$(dirname "$0")"
cc -Wall -Wextra -I. -I../tpms_bridge -o /tmp/tpms_decoder_test main.c ../tpms_bridge/tpms_renault.c
exec /tmp/tpms_decoder_test
