#!/bin/sh
# Runs the firmware decoders on the host, no Flipper needed.
set -e
cd "$(dirname "$0")"
cc -Wall -Wextra -O2 -I. -I../tpms_bridge -o /tmp/tpms_decoder_test \
    main.c \
    ../tpms_bridge/tpms_bits.c \
    ../tpms_bridge/tpms_decoder.c \
    ../tpms_bridge/tpms_protocols.c \
    ../tpms_bridge/tpms_proto_*.c
exec /tmp/tpms_decoder_test
