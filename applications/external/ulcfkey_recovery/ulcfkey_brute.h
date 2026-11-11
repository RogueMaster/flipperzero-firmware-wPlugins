#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ULCFKEY_BRUTE_KEY_SIZE     16U
#define ULCFKEY_BRUTE_TARGET_COUNT 4U
#define ULCFKEY_BRUTE_SPACE        (1UL << 28)

typedef struct {
    uint8_t key[ULCFKEY_BRUTE_KEY_SIZE];
    uint8_t phase;
    uint32_t next_candidate;
    bool complete;
} UlcfkeyBruteState;

/* Runtime-only acceleration cache. It is deliberately excluded from the
 * persisted session; reopening the app rebuilds it from UlcfkeyBruteState. */
typedef struct UlcfkeyBruteRuntime UlcfkeyBruteRuntime;

void ulcfkey_brute_init(UlcfkeyBruteState* state);
UlcfkeyBruteRuntime* ulcfkey_brute_runtime_alloc(void);
void ulcfkey_brute_runtime_free(UlcfkeyBruteRuntime* runtime);
void ulcfkey_brute_runtime_reset(UlcfkeyBruteRuntime* runtime);

/*
 * Search at most budget candidates. Targets are ordered nt100, nt75, nt50,
 * nt25, as recorded by ULCFKey. Returns true when a segment or the full key
 * was completed, so the caller can persist immediately.
 */
bool ulcfkey_brute_step(
    UlcfkeyBruteState* state,
    UlcfkeyBruteRuntime* runtime,
    const uint8_t targets[ULCFKEY_BRUTE_TARGET_COUNT][8],
    uint32_t budget);

uint8_t ulcfkey_brute_segment(const UlcfkeyBruteState* state);
uint8_t ulcfkey_brute_percent(const UlcfkeyBruteState* state);
