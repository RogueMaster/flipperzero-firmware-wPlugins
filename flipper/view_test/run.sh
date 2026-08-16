#!/bin/sh
# Draws the app screens on the host and checks the layout. No Flipper needed.
set -e
cd "$(dirname "$0")"
cc -Wall -Wextra -I. -Istubs -I../tpms_bridge \
    -o /tmp/tpms_view_test \
    main.c canvas_stub.c \
    ../tpms_bridge/tpms_view.c \
    ../tpms_bridge/tpms_store.c \
    ../tpms_bridge/tpms_bits.c \
    ../tpms_bridge/tpms_protocols.c \
    ../tpms_bridge/tpms_proto_*.c
exec /tmp/tpms_view_test
