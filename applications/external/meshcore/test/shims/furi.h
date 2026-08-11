/*
 * Host-side stand-in for the Flipper <furi.h>.
 *
 * Only what protocol/meshcore_link.c and the headers it pulls in actually
 * touch. This is NOT a Flipper emulator — it exists so the protocol layer can
 * be compiled and exercised on a PC, where a failing assertion prints a stack
 * trace instead of rebooting a Flipper.
 *
 * The clock is fake and driven by the test (see test/fakes.c): the link layer
 * computes deadlines from furi_get_tick(), so time only moves when a blocking
 * read is asked to wait. That makes timeout tests deterministic and instant.
 */
#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_assert(expr) assert(expr)
#define furi_check(expr)  assert(expr)
#define UNUSED(x)         ((void)(x))

/* The SDK spells printf-format attributes this way (newlib's sys/cdefs.h). */
#ifndef _ATTRIBUTE
#define _ATTRIBUTE(x) __attribute__(x)
#endif

#define FuriWaitForever 0xFFFFFFFFU

/* Declared only so meshcore_log.h's prototypes compile; never dereferenced
 * here, and the fake log ignores it. */
typedef struct FuriString FuriString;

/* Fake clock, in milliseconds. */
uint32_t furi_get_tick(void);
uint32_t furi_ms_to_ticks(uint32_t milliseconds);

/* Test-side control over the fake clock. */
void fake_clock_reset(void);
void fake_clock_advance(uint32_t ms);
