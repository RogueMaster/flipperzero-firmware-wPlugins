#!/bin/sh
# Host-side simulation of the firmware logic. No Flipper required.
cd "$(dirname "$0")"
set -e
gcc -std=gnu11 -Wall -Wextra -Werror -I stubs -I .. -o test_rules test_rules.c -lm
./test_rules
