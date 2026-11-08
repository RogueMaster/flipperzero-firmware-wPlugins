// Minimal Arduino stand-in for host-side tests. Not used by firmware builds.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pretend to be the official ESP32-S3 build with USB CDC on boot.
#define CONFIG_IDF_TARGET_ESP32S3 1
#define ARDUINO_USB_CDC_ON_BOOT   1
static const uint8_t TX = 43;
static const uint8_t RX = 44;

typedef int gpio_num_t;
#define GPIO_NUM_1  1
#define GPIO_NUM_2  2
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_43 43
#define GPIO_NUM_44 44

namespace host_clock {
inline uint32_t& ms() {
    static uint32_t v = 1000;
    return v;
}
inline uint32_t& us() {
    static uint32_t v = 1000000;
    return v;
}
} // namespace host_clock

inline uint32_t millis() {
    return host_clock::ms();
}
inline uint32_t micros() {
    return host_clock::us();
}

class Stream {
public:
    virtual ~Stream() {
    }
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t b) = 0;
    virtual size_t write(const uint8_t* buf, size_t len) {
        size_t n = 0;
        for(size_t i = 0; i < len; i++) {
            n += write(buf[i]);
        }
        return n;
    }
    virtual int availableForWrite() {
        return 4096;
    }
};
