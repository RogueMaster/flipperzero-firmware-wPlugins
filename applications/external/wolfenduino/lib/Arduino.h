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
#include <furi_hal_random.h>

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

/* -------------------------------------------------------------------------- */
/* Random (Furi HAL random)                                                   */
/* -------------------------------------------------------------------------- */
/*
 * В Arduino randomSeed() задаёт seed для PRNG.
 * Здесь мы используем аппаратный/системный RNG Flipper,
 * seed не нужен — оставляем заглушку для совместимости.
 */
static inline void randomSeed(uint32_t seed) {
    (void)seed;
}

/* Случайное положительное long (как Arduino random()). */
static inline long randomLong(void) {
    return (long)(furi_hal_random_get() & 0x7FFFFFFFUL);
}

/* Аналог Arduino random(max) в C-стиле (название без конфликта с C++). */
static inline long randomMax(long max_value) {
    if(max_value <= 0) return 0;
    uint32_t r = furi_hal_random_get();
    return (long)(r % (uint32_t)max_value);
}

/* Аналог Arduino random(min, max) в C-стиле (название без конфликта). */
static inline long randomRange(long min_value, long max_value) {
    if(max_value <= min_value) return min_value;
    uint32_t span = (uint32_t)(max_value - min_value);
    uint32_t r = furi_hal_random_get();
    return (long)(min_value + (long)(r % span));
}

/* Заполнить буфер случайными байтами. */
static inline void randomBytes(uint8_t* buf, uint32_t len) {
    if(!buf || !len) return;
    furi_hal_random_fill_buf(buf, len);
}

/* Удобные функции на разные разрядности. */
static inline uint32_t random32(void) {
    return furi_hal_random_get();
}

static inline uint16_t random16(void) {
    return (uint16_t)(furi_hal_random_get() & 0xFFFFU);
}

static inline uint8_t random8(void) {
    return (uint8_t)(furi_hal_random_get() & 0xFFU);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

/* -------------------------------------------------------------------------- */
/* Arduino-like random() overloads for C++                                    */
/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
static inline long random(long max_value) {
    return randomMax(max_value);
}

static inline long random(long min_value, long max_value) {
    return randomRange(min_value, max_value);
}
#endif
