#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;

    size_t print(const char* s) {
        if(!s) return 0;
        size_t n = 0;
        while(*s) n += write((uint8_t)*s++);
        return n;
    }

    size_t print(char c) {
        return write((uint8_t)c);
    }

    size_t print(int v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        return print(buf);
    }

    size_t print(unsigned int v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", v);
        return print(buf);
    }

    size_t print(long v) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%ld", v);
        return print(buf);
    }

    size_t print(unsigned long v) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lu", v);
        return print(buf);
    }
};
