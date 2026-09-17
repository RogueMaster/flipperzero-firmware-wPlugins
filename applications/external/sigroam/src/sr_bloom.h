#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * ★ Unique BSSID counting (bloom filter). Must not include any furi header (ADR-003).
 *
 * A fixed 4 KB bit array that counts only and stores no table. The Plan section 3.5 budget is
 * exactly 4 KB; a table would need 17+ B per entry, so 2000 APs would be 34 KB against roughly
 * 70 KB of usable Flipper heap.
 * Accepting a per-mille error rate and stating it on the About page is a settled trade-off.
 *
 * Parameters (settled in task card T2.5; must not be changed):
 *   m = 32768 bits = 4096 B
 *   k = 4
 *   Base hash = FNV-1a 32: offset 2166136261 (0x811C9DC5),
 *     prime 16777619 (0x01000193), in the order (hash XOR byte) * prime
 *   Double hashing = Kirsch-Mitzenmacher: g_i = (h1 + i*h2) mod m, for i = 0..k-1
 *   h2 is forced odd; h2 reruns FNV-1a over the same input with a different offset basis
 *   Before hashing, each byte a-f is uppercased to A-F; all other bytes pass through
 *
 * The error rate follows the measured host_test table, not a quoted formula.
 */

enum { SR_BLOOM_BITS = 32768, SR_BLOOM_BYTES = SR_BLOOM_BITS / 8, SR_BLOOM_K = 4 };

typedef struct {
    uint8_t bits[SR_BLOOM_BYTES];
    uint32_t added;  /* Calls to sr_bloom_add (duplicates included; NULL/empty strings excluded) */
    uint32_t unique; /* Of those, how many were judged new = the unique-count estimate */
} SrBloom;

/* 4096-bit array + two uint32 + alignment slack */
_Static_assert(sizeof(SrBloom) <= 4108, "SrBloom over 4 KB + counter budget");

/* Clear the bit array and the counters. No-op when b is NULL. */
void sr_bloom_init(SrBloom* b);

/*
 * Insert one BSSID.
 * Returns true when it had not been seen before (bits set, unique++); false means possibly seen.
 * When b or bssid is NULL, or bssid is an empty string: state is unchanged and it returns false.
 */
bool sr_bloom_add(SrBloom* b, const char* bssid);

/* Query. NULL or an empty string returns false. true means only "possibly in the set". */
bool sr_bloom_maybe_contains(const SrBloom* b, const char* bssid);

/* Unique-count estimate. Returns 0 when b is NULL. */
uint32_t sr_bloom_unique(const SrBloom* b);

/* Number of bits set. The UI uses it to show load. Returns 0 when b is NULL. */
uint32_t sr_bloom_fill_bits(const SrBloom* b);
