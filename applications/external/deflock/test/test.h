// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Tiny host-test assert harness (plain C, no dependencies). Each CHECK_* records
// a check and prints a one-line diagnostic on failure; the process exits non-zero
// if any check failed (see test_main.c), so CI can gate on it.
#pragma once

#include <stdio.h>
#include <string.h>

extern int g_checks; /**< total checks evaluated */
extern int g_fails; /**< checks that failed */

#define CHECK(cond)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if(!(cond)) {                                                       \
            g_fails++;                                                      \
            printf("  FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                                   \
    } while(0)

#define CHECK_INT_EQ(actual, expected)                          \
    do {                                                        \
        g_checks++;                                             \
        long _a = (long)(actual);                               \
        long _e = (long)(expected);                             \
        if(_a != _e) {                                          \
            g_fails++;                                          \
            printf(                                             \
                "  FAIL %s:%d: %s == %s (got %ld, want %ld)\n", \
                __FILE__,                                       \
                __LINE__,                                       \
                #actual,                                        \
                #expected,                                      \
                _a,                                             \
                _e);                                            \
        }                                                       \
    } while(0)

#define CHECK_STR_EQ(actual, expected)                       \
    do {                                                     \
        g_checks++;                                          \
        const char* _a = (actual);                           \
        const char* _e = (expected);                         \
        if(!_a || !_e || strcmp(_a, _e) != 0) {              \
            g_fails++;                                       \
            printf(                                          \
                "  FAIL %s:%d: %s == \"%s\" (got \"%s\")\n", \
                __FILE__,                                    \
                __LINE__,                                    \
                #actual,                                     \
                _e ? _e : "(null)",                          \
                _a ? _a : "(null)");                         \
        }                                                    \
    } while(0)

#define CHECK_STR_CONTAINS(hay, needle)                         \
    do {                                                        \
        g_checks++;                                             \
        const char* _h = (hay);                                 \
        const char* _n = (needle);                              \
        if(!_h || !_n || !strstr(_h, _n)) {                     \
            g_fails++;                                          \
            printf(                                             \
                "  FAIL %s:%d: \"%s\" should contain \"%s\"\n", \
                __FILE__,                                       \
                __LINE__,                                       \
                _h ? _h : "(null)",                             \
                _n ? _n : "(null)");                            \
        }                                                       \
    } while(0)
