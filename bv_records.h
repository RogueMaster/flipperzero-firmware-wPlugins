/*
 * BioVault records — the in-memory vault data model.
 *
 * The whole vault is one GCM-sealed blob on the tag (see bv_vault). In RAM it is
 * a small list of entries. Indexing/search happens here, in memory, after a full
 * decrypt — the vault is < 1 KB so there is no need for on-tag indexing.
 *
 * Serialized (plaintext, before sealing) as a compact length-prefixed format,
 * NOT CSV (passwords may contain commas/newlines):
 *   [u8 count]
 *   repeated: [u8 type][u8 label_len][label][u8 user_len][user][u8 secret_len][secret]
 *
 * An entry is either a credential (label + user + secret) or a freeform note
 * (label + secret, user empty). Fields are plain UTF-8, not NUL-terminated on the
 * wire; the in-RAM struct keeps NUL-terminated copies for convenience.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BV_MAX_ENTRIES 24
#define BV_LABEL_CAP 48
#define BV_USER_CAP 48
#define BV_SECRET_CAP 64

typedef enum {
    BvEntryCred = 0, // label + username + secret(password)
    BvEntryNote = 1, // label + secret(freeform), username empty
} BvEntryType;

typedef struct {
    uint8_t type;
    char label[BV_LABEL_CAP]; // NUL-terminated
    char user[BV_USER_CAP];
    char secret[BV_SECRET_CAP];
} BvEntry;

typedef struct {
    uint8_t count;
    BvEntry entries[BV_MAX_ENTRIES];
} BvVaultData;

// Reset to an empty vault.
void bv_records_init(BvVaultData* v);

// Append an entry. Returns false if full or fields exceed caps. user may be "".
bool bv_records_add(
    BvVaultData* v,
    BvEntryType type,
    const char* label,
    const char* user,
    const char* secret);

// Remove entry at index. Returns false if out of range.
bool bv_records_remove(BvVaultData* v, uint8_t index);

// Replace the entry at `index` in place (keeps its position). Returns false if
// out of range or a field exceeds its cap; on failure the entry is unchanged.
// user may be "" (makes it a note when type is BvEntryNote).
bool bv_records_set(
    BvVaultData* v,
    uint8_t index,
    BvEntryType type,
    const char* label,
    const char* user,
    const char* secret);

// Find the first entry whose label equals `label` (case-insensitive). Returns
// its index, or -1 if not found.
int bv_records_find(const BvVaultData* v, const char* label);

// Serialize into `buf` (capacity cap). Writes the length to `out_len`.
bool bv_records_serialize(const BvVaultData* v, uint8_t* buf, size_t cap, size_t* out_len);

// Parse a serialized blob into `v`. Returns false on malformed input.
bool bv_records_parse(BvVaultData* v, const uint8_t* buf, size_t len);

// Wipe a vault (including secrets) from memory.
void bv_records_clear(BvVaultData* v);

// Self-test: build a vault, serialize/parse round-trip, then round-trip through
// the encrypted vault blob (bv_vault). Runs at startup during bring-up.
bool bv_records_selftest(void);
