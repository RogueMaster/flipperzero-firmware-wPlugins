#!/bin/sh
# Прогоняет декодер прошивки (tpms_renault.c) на хосте, без Flipper.
set -e
cd "$(dirname "$0")"
cc -Wall -Wextra -I. -I../tpms_bridge -o /tmp/tpms_decoder_test main.c ../tpms_bridge/tpms_renault.c
exec /tmp/tpms_decoder_test
