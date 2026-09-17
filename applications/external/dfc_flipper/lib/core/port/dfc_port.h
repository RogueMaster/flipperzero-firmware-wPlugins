/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 CinderSocket
 *
 * Platform interface for the DESFire engine.
 *
 * Assertions, logging and randomness. This is the whole platform surface the
 * engine depends on; everything else it needs is freestanding C.
 *
 * The firmware supplies the implementations in desfire_shim.c; the host test
 * harness supplies its own in tests/support/.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef DFC_UNUSED
#define DFC_UNUSED(x) ((void)(x))
#endif

#ifndef DFC_MIN
#define DFC_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef DFC_MAX_OF
#define DFC_MAX_OF(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef DFC_FIRMWARE_BUILD
void dfc_assert_fail(const char* file, int line);
#define DFC_ASSERT(expr)                          \
    do {                                          \
        if(!(expr)) dfc_assert_fail(__FILE__, __LINE__); \
    } while(0)
#else
#include <assert.h>
#define DFC_ASSERT(expr) assert(expr)
#endif

/* Logging compiles out entirely. The engine's log call sites are kept because
 * they document protocol decision points, but no formatted-output machinery is
 * linked -- deliberately, since printf costs ~14 KiB of flash. */
#define DFC_LOG_D(tag, ...) ((void)0)
#define DFC_LOG_T(tag, ...) ((void)0)
#define DFC_LOG_I(tag, ...) ((void)0)
#define DFC_LOG_W(tag, ...) ((void)0)
#define DFC_LOG_E(tag, ...) ((void)0)

/* Fill `buf` with `len` cryptographically usable random bytes.
 *
 * Called from the NFC interrupt handler during authentication (to generate
 * RndB), so the implementation must not block and must not issue a SoftDevice
 * SVC from a high-priority context. The firmware satisfies this by draining a
 * ring that the main loop keeps topped up. Never returns predictable bytes:
 * DESFire authentication security rests on RndB being unguessable. */
void dfc_random_fill(uint8_t* buf, size_t len);

/* Object allocation.
 *
 * The engine allocates exactly four kinds of long-lived object, so allocation
 * is tagged rather than being a generic heap call. That lets the firmware serve
 * each tag from a dedicated static slot instead of a heap: its heap is 8 KiB
 * and shared, these four objects total ~4.4 KiB, and dfc_virtual_picc_field_off
 * releases and reacquires the emulator from an interrupt handler on every RF
 * field drop -- where a general-purpose allocator is neither safe nor bounded.
 *
 * Returns NULL if the tag's storage is already in use. Host tests back these
 * with plain malloc/free, so behaviour is identical either way. */
typedef enum {
    DfcAllocEmulator,
    DfcAllocSecureMessaging,
    DfcAllocSession,
} DfcAllocTag;

void* dfc_platform_alloc(size_t size, DfcAllocTag tag);
void dfc_platform_free(void* ptr);

/* Points in the exchange a user interface may want to react to.
 *
 * The engine reports them and carries on; nothing in the protocol depends on
 * the result. A build with no user interface implements dfc_port_notify as an
 * empty function. `context` is the pointer the caller handed to
 * dfc_emulator_handle_command, which the engine never inspects. */
typedef enum {
    DfcEventApplicationSelected,
    DfcEventAuthenticated,
    DfcEventFileRequested,
} DfcEvent;

void dfc_port_notify(void* context, DfcEvent event);
