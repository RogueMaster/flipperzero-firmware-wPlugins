/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_test.h"

#include <stdarg.h>
#include <stdlib.h>

int emrtd_test_failures = 0;
int emrtd_test_checks = 0;
const char* emrtd_test_current = "";

static int emrtd_test_suite_failures = 0;

void emrtd_test_begin(const char* name) {
    if(emrtd_test_current[0] != '\0') {
        printf("  %s %s\n", emrtd_test_suite_failures == 0 ? "ok  " : "FAIL", emrtd_test_current);
    }
    emrtd_test_current = name;
    emrtd_test_suite_failures = 0;
}

void emrtd_test_fail(const char* file, int line, const char* fmt, ...) {
    emrtd_test_failures++;
    emrtd_test_suite_failures++;
    const char* base = strrchr(file, '/');
    printf("  FAIL %s\n    %s:%d: ", emrtd_test_current, base ? base + 1 : file, line);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

size_t emrtd_test_hex(const char* hex, uint8_t* out, size_t out_size) {
    size_t n = 0;
    int hi = -1;
    for(const char* c = hex; *c != '\0'; c++) {
        int value;
        if(*c >= '0' && *c <= '9') {
            value = *c - '0';
        } else if(*c >= 'A' && *c <= 'F') {
            value = *c - 'A' + 10;
        } else if(*c >= 'a' && *c <= 'f') {
            value = *c - 'a' + 10;
        } else {
            continue; /* separators */
        }
        if(hi < 0) {
            hi = value;
        } else {
            if(n >= out_size) {
                printf("  hex literal longer than the %zu byte buffer\n", out_size);
                abort();
            }
            out[n++] = (uint8_t)((hi << 4) | value);
            hi = -1;
        }
    }
    return n;
}

const char* emrtd_test_hexstr(const uint8_t* data, size_t len) {
    static char buffers[4][1024];
    static size_t next = 0;
    char* buf = buffers[next];
    next = (next + 1) % 4;

    size_t pos = 0;
    for(size_t i = 0; i < len && pos + 3 < sizeof(buffers[0]); i++) {
        pos += (size_t)snprintf(buf + pos, sizeof(buffers[0]) - pos, "%02X", data[i]);
    }
    buf[pos] = '\0';
    return buf;
}

int main(void) {
    printf("eMRTD host test suite\n\n");

    struct {
        const char* name;
        void (*run)(void);
    } suites[] = {
#define EMRTD_TEST_ENTRY(name, label) {label, test_suite_##name},
        EMRTD_TEST_SUITES(EMRTD_TEST_ENTRY)
#undef EMRTD_TEST_ENTRY
    };

    for(size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        printf("%s\n", suites[i].name);
        suites[i].run();
        emrtd_test_begin("");
        printf("\n");
    }

    printf(
        "%d checks, %d failure%s\n",
        emrtd_test_checks,
        emrtd_test_failures,
        emrtd_test_failures == 1 ? "" : "s");
    return emrtd_test_failures == 0 ? 0 : 1;
}
