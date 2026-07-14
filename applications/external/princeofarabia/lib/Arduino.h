#pragma once
/*
 * lib/Arduino.h
 *
 * Минимальная Arduino-совместимость для Flipper Zero (Furi):
 * - базовые Arduino-типы и макросы
 * - PROGMEM / pgm_read_* совместимость
 * - millis/micros/delay/delayMicroseconds и тики
 * - randomSeed random randomBytes (через furi_hal_random)
 *
 * Подходит для C и C++.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <furi.h>
#include "include/random.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Arduino basic types                                                        */
/* -------------------------------------------------------------------------- */
typedef uint8_t  byte;
typedef bool     boolean;
typedef uint16_t word;

#ifndef __uint24
typedef uint32_t __uint24;
#endif

#ifndef NULL
#define NULL 0
#endif

/* -------------------------------------------------------------------------- */
/* PROGMEM / pgm_read_* compatibility                                         */
/* На Flipper это обычная память, поэтому PROGMEM пустой.                     */
/* -------------------------------------------------------------------------- */
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef F_CPU
#define F_CPU 64000000UL
#endif

#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) \
    do { if(bitvalue) bitSet(value, bit); else bitClear(value, bit); } while(0)
#endif


#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif

#ifndef PSTR
#define PSTR(x) (x)
#endif

#ifndef strlen_P
#define strlen_P(s) strlen((const char*)(s))
#endif

static inline void* pgm_read_ptr_safe(const void* addr) {
    void* p;
    memcpy(&p, addr, sizeof(p));
    return p;
}

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr) pgm_read_ptr_safe((const void*)(addr))
#endif


/* Безопасное чтение 16/32 бит без требований к выравниванию */
static inline uint16_t pgm_read_word_safe(const void* addr) {
    uint16_t v;
    memcpy(&v, addr, sizeof(v));
    return v;
}

static inline uint32_t pgm_read_dword_safe(const void* addr) {
    uint32_t v;
    memcpy(&v, addr, sizeof(v));
    return v;
}

#ifndef pgm_read_word
#define pgm_read_word(addr) pgm_read_word_safe((const void*)(addr))
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr) pgm_read_dword_safe((const void*)(addr))
#endif

/* -------------------------------------------------------------------------- */
/* Common Arduino macros                                                      */
/* -------------------------------------------------------------------------- */
#ifndef F
#define F(x) (x)
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef _BV
#define _BV(bit) (1U << (bit))
#endif

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x1U)
#endif

#ifndef bitSet
#define bitSet(value, bit) ((value) |= (uint8_t)_BV(bit))
#endif

#ifndef bitClear
#define bitClear(value, bit) ((value) &= (uint8_t)~_BV(bit))
#endif

/* -------------------------------------------------------------------------- */
/* Time functions (Furi tick based)                                           */
/* -------------------------------------------------------------------------- */
/* На Flipper furi_get_tick() возвращает "тики" (обычно 1 тик = 1 мс). */
static inline uint32_t ticks(void) {
    return furi_get_tick();
}

/* Arduino-like millis(): миллисекунды от старта. */
static inline uint32_t millis(void) {
    return furi_get_tick();
}

/*
 * Arduino-like micros():
 * В Furi нет гарантированного микросекундного счетчика через tick,
 * поэтому делаем простую аппроксимацию: tick * 1000.
 * Если у тебя есть более точный источник времени — подмени реализацию.
 */
static inline uint32_t micros(void) {
    return furi_get_tick() * 1000UL;
}

static inline void delay(uint32_t ms) {
    furi_delay_ms(ms);
}

static inline void delayMicroseconds(uint32_t us) {
    furi_delay_us(us);
}

static inline uint32_t millisToTicks(uint32_t ms) {
    return furi_ms_to_ticks(ms);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
