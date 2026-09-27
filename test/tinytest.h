#pragma once
#include <stdio.h>
#include <string.h>

static int gt_checks = 0;
static int gt_fails = 0;
static const char* gt_group = "";

#define GROUP(name)                     \
    do {                                \
        gt_group = (name);              \
        printf("\n  %s\n", gt_group);   \
    } while(0)

#define CHECK(cond, what)                                              \
    do {                                                              \
        gt_checks++;                                                  \
        if(cond) {                                                    \
            printf("    ok   %s\n", (what));                          \
        } else {                                                      \
            gt_fails++;                                               \
            printf("    FAIL %s   (%s:%d)\n", (what), __FILE__, __LINE__); \
        }                                                             \
    } while(0)

#define REPORT()                                                            \
    do {                                                                    \
        printf("\n%d checks, %d failed\n", gt_checks, gt_fails);            \
        return gt_fails ? 1 : 0;                                            \
    } while(0)
