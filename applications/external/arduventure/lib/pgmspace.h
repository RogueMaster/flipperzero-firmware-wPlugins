#pragma once

#include <stdint.h>
#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif

#ifndef pgm_read_word
static inline uint16_t pgm_read_word_safe(const void* addr) {
    uint16_t v;
    memcpy(&v, addr, sizeof(v));
    return v;
}
#define pgm_read_word(addr) pgm_read_word_safe((const void*)(addr))
#endif

#ifndef pgm_read_ptr
static inline void* pgm_read_ptr_safe(const void* addr) {
    void* p;
    memcpy(&p, addr, sizeof(p));
    return p;
}
#define pgm_read_ptr(addr) pgm_read_ptr_safe((const void*)(addr))
#endif
