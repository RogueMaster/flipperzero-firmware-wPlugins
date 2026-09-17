#pragma once

#include <stdio.h>

/*
 * Shared assertion macro. On failure it prints file:line: expression and
 * bumps a failure counter instead of aborting the run.
 * Each TU that includes this header gets its own counter; test_<name>_run
 * returns that group's failure count.
 */

static int sr_test_failures;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if(!(expr)) {                                                      \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);     \
            sr_test_failures++;                                            \
        }                                                                  \
    } while(0)
