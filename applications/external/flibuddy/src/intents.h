/* User actions on a scanned tag, as the Flipper sends them in `intent`.
 *
 * An Intent is self-contained: it carries a copy of the tag dump it refers
 * to, so a future offline queue can replay it without its scan. Intent ids
 * come from an IdSeq; the host treats a repeated id as the same action.
 *
 * Intents in flight live in an IntentStore. The first version keeps one
 * intent in memory (SingleSlotIntentStore); an SD-backed queue replayed on
 * connect will implement the same interface.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "proto_encode.h"
#include "tag_dump.h"

/* One tag read and the id it was sent under. */
typedef struct {
    char id[PROTO_ID_MAX + 1];
    TagDump tag;
} Scan;

/* Generator of `<prefix>-NNNNNN` ids (zero-padded to six digits, more when
 * the counter grows). The app seeds the first value per run (e.g. from the
 * RTC) so that ids of an earlier run are not reused while the host still
 * remembers them. */
typedef struct {
    char prefix;
    uint32_t next;
} IdSeq;

void id_seq_init(IdSeq* seq, char prefix, uint32_t first);

/* A first counter value from the RTC: (seconds mod 10^6) * 1000. A new run
 * reuses no id of an earlier run that issued fewer than a thousand ids per
 * second it ran, until the seed wraps after about 11.5 days. */
uint32_t id_seed_from_time(uint32_t unix_seconds);

/* Writes the next id into `out` and advances the counter (wrapping at
 * UINT32_MAX); false when the prefix makes it an invalid protocol id. */
bool id_seq_next(IdSeq* seq, char out[PROTO_ID_MAX + 1]);

typedef enum {
    IntentActionWeigh,
    IntentActionNew,
    IntentActionCreateOnHost,
    IntentActionLinkOnHost,
    IntentActionFull, /* Mark as new */
    IntentActionEmpty, /* Mark as empty (and archive) */
    IntentActionDelete,
} IntentAction;

typedef enum {
    WeightModeGross,
    WeightModeNet,
} WeightMode;

typedef struct {
    char id[PROTO_ID_MAX + 1];
    char scan_id[PROTO_ID_MAX + 1];
    IntentAction action;
    uint16_t grams; /* weigh only */
    WeightMode mode; /* weigh only */
    TagDump tag;
} Intent;

/* A `weigh` intent for the scan; false (no id consumed) when grams exceed
 * PROTO_GRAMS_MAX, the mode is unknown or the scan id is invalid. */
bool intent_new_weigh(Intent* out, IdSeq* ids, const Scan* scan, uint16_t grams, WeightMode mode);

/* A `new`, `full`, `empty`, `delete`, `create_on_host` or `link_on_host`
 * intent; false (no id consumed) for IntentActionWeigh, an unknown action
 * or an invalid scan id. */
bool intent_new_action(Intent* out, IdSeq* ids, const Scan* scan, IntentAction action);

/* Encodes the intent as one compact JSON line without the '\n' (see
 * proto_encode.h for the buffer contract); 0 when it does not fit or the
 * tag is invalid. */
size_t intent_encode(const Intent* intent, char* buf, size_t cap);

typedef struct IntentStore IntentStore;

struct IntentStore {
    /* Takes a copy of the intent; false when the store is full. */
    bool (*put)(IntentStore* store, const Intent* intent);
    /* Completes the intent with this id; false (store unchanged) when no
     * such intent is in flight. */
    bool (*resolve)(IntentStore* store, const char* id);
    /* The intent to send or wait on next, NULL when none. */
    const Intent* (*current)(const IntentStore* store);
};

static inline bool intent_store_put(IntentStore* store, const Intent* intent) {
    return store->put(store, intent);
}

static inline bool intent_store_resolve(IntentStore* store, const char* id) {
    return store->resolve(store, id);
}

static inline const Intent* intent_store_current(const IntentStore* store) {
    return store->current(store);
}

/* Holds at most one intent: put replaces whatever is in flight (it timed
 * out or was abandoned), so a late result for it no longer matches. */
typedef struct {
    IntentStore base;
    bool occupied;
    Intent slot;
} SingleSlotIntentStore;

void single_slot_intent_store_init(SingleSlotIntentStore* store);
