/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * A very small test harness. The suites run on the host so that the protocol
 * and cryptographic code can be checked against the published ICAO test
 * vectors without a Flipper or a passport in reach.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int emrtd_test_failures;
extern int emrtd_test_checks;
extern const char* emrtd_test_current;

void emrtd_test_begin(const char* name);
void emrtd_test_fail(const char* file, int line, const char* fmt, ...);

/** Parse a hex string, ignoring spaces. Returns the byte count. */
size_t emrtd_test_hex(const char* hex, uint8_t* out, size_t out_size);

/** Format bytes as uppercase hex into a static rotating buffer. */
const char* emrtd_test_hexstr(const uint8_t* data, size_t len);

#define TEST_CHECK(cond)                                      \
    do {                                                      \
        emrtd_test_checks++;                                  \
        if(!(cond)) {                                         \
            emrtd_test_fail(__FILE__, __LINE__, "%s", #cond); \
        }                                                     \
    } while(0)

#define TEST_EQ_INT(actual, expected)                                                        \
    do {                                                                                     \
        emrtd_test_checks++;                                                                 \
        const long long _a = (long long)(actual);                                            \
        const long long _e = (long long)(expected);                                          \
        if(_a != _e) {                                                                       \
            emrtd_test_fail(__FILE__, __LINE__, "%s: got %lld, want %lld", #actual, _a, _e); \
        }                                                                                    \
    } while(0)

#define TEST_EQ_STR(actual, expected)                                                              \
    do {                                                                                           \
        emrtd_test_checks++;                                                                       \
        if(strcmp((actual), (expected)) != 0) {                                                    \
            emrtd_test_fail(                                                                       \
                __FILE__, __LINE__, "%s: got \"%s\", want \"%s\"", #actual, (actual), (expected)); \
        }                                                                                          \
    } while(0)

/** Compare @p len bytes at @p actual with the hex string @p expected_hex. */
#define TEST_EQ_HEX(actual, len, expected_hex)                                \
    do {                                                                      \
        emrtd_test_checks++;                                                  \
        uint8_t _exp[512];                                                    \
        const size_t _n = emrtd_test_hex((expected_hex), _exp, sizeof(_exp)); \
        if((size_t)(len) != _n || memcmp((actual), _exp, _n) != 0) {          \
            emrtd_test_fail(                                                  \
                __FILE__,                                                     \
                __LINE__,                                                     \
                "%s:\n      got  %s\n      want %s",                          \
                #actual,                                                      \
                emrtd_test_hexstr((const uint8_t*)(actual), (size_t)(len)),   \
                emrtd_test_hexstr(_exp, _n));                                 \
        }                                                                     \
    } while(0)

/* Every suite implemented by the harness, declared from the central list. */
#include "suites.h"

#define EMRTD_TEST_DECLARE(name, label) void test_suite_##name(void);
EMRTD_TEST_SUITES(EMRTD_TEST_DECLARE)
#undef EMRTD_TEST_DECLARE
