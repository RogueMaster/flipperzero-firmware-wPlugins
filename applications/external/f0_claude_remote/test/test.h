#pragma once

#include <assert.h>
#include <stdio.h>

/* abort() from a failed assert does not flush stdout, and the counted lines must survive it. */
#define test_run(test)            \
    do {                          \
        test();                   \
        printf("ok %s\n", #test); \
        fflush(stdout);           \
    } while(0)
