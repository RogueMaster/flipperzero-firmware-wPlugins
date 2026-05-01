#!/bin/sh
set -e

mkdir -p tests/build
cc -std=gnu99 -Wall -Wextra tests/packet_test.c src/packet.c src/aprs.c -o tests/build/packet_test
./tests/build/packet_test
cc -std=gnu99 -Wall -Wextra -Itests/unity tests/packet_test_unity.c tests/unity/unity.c src/packet.c src/aprs.c -o tests/build/packet_test_unity
./tests/build/packet_test_unity
cc -std=gnu99 -Wall -Wextra -Itests/unity tests/aprs_test.c tests/unity/unity.c src/aprs.c src/packet.c -o tests/build/aprs_test
./tests/build/aprs_test
