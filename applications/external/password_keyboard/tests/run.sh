#!/bin/sh
set -eu

mkdir -p /tmp/password-keyboard-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -Itests/mocks -I. \
    zk_crypto.c tests/test_crypto.c \
    -o /tmp/password-keyboard-tests/test_crypto
/tmp/password-keyboard-tests/test_crypto
