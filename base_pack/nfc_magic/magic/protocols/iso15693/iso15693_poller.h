#pragma once

#include <nfc/nfc_poller.h>
#include "iso15693_data.h"

typedef struct Iso15693Poller Iso15693Poller;

// Size (bytes) of the per-block failure bitmap; covers up to 256 blocks (the ISO15693 max).
#define ISO15693_POLLER_BLOCK_BITMAP_SIZE (32U)

typedef enum {
    Iso15693PollerModeInfo, // detect + read UID / system info
    Iso15693PollerModeWriteUid, // magic backdoor UID write (gen2 first, then gen1 if untouched)
    Iso15693PollerModeClone, // write UID + all writable data blocks from a source image
    Iso15693PollerModeWipe, // zero every writable data block (UID left unchanged)
} Iso15693PollerMode;

typedef enum {
    Iso15693PollerEventSuccess, // info read ok, or the write/clone verified against the target
    Iso15693PollerEventPartial, // clone: UID written, but some data blocks could not be written
    Iso15693PollerEventFail, // card present but the backdoor write was not accepted (not a magic tag)
    Iso15693PollerEventCardLost, // no card in the field / card removed before the operation finished
} Iso15693PollerEvent;

typedef void (*Iso15693PollerCallback)(Iso15693PollerEvent event, void* context);

Iso15693Poller* iso15693_poller_alloc(Nfc* nfc);

void iso15693_poller_free(Iso15693Poller* instance);

// Detect + read (Info mode). Emits Success once a card is read, or CardLost after a bounded number
// of activation attempts with no card in the field.
void iso15693_poller_start(Iso15693Poller* instance, Iso15693PollerCallback callback, void* context);

// Magic UID write. `uid` is ISO15693_3_UID_SIZE bytes, MSB-first (uid[0] must be 0xE0).
// The poller writes the gen2 backdoor sequence first (a harmless custom command on a non-magic tag)
// and, only if the gen2 write left the card's UID unchanged, falls back to the destructive gen1
// WRITE-BLOCK sequence. Before each read-back it power-cycles the field (like proxmark's
// switch_off + getUID) so a card that only latches the new UID after a reset is not misreported as a
// failure. Reports Success only if a read-back inventory returns the requested UID.
// See .notes/protocol-reference.md for the byte-level frames.
void iso15693_poller_start_write_uid(
    Iso15693Poller* instance,
    const uint8_t* uid,
    Iso15693PollerCallback callback,
    void* context);

// Full clone: write `source`'s UID (magic backdoor) and every writable data block (standard WRITE
// BLOCK, locked blocks skipped) onto a magic card. `source` is an ISO15693-3 image loaded from a
// saved .nfc. Reports Success (UID + all blocks), Partial (UID ok, some blocks failed), Fail (UID
// not accepted) or CardLost. Data blocks are written first, then the UID.
void iso15693_poller_start_clone(
    Iso15693Poller* instance,
    const Iso15693_3Data* source,
    Iso15693PollerCallback callback,
    void* context);

// After a clone, the per-block write result: source block count, in-range blocks that failed to
// write (locked/protected), source blocks past the target's capacity (couldn't fit), a bitmap
// (bit N = block N failed), and whether the gen1 fallback set the UID (which overwrites blocks
// 56/57/62/63). Any out param may be NULL. `failed_bitmap` must hold ISO15693_POLLER_BLOCK_BITMAP_SIZE
// bytes.
void iso15693_poller_get_clone_result(
    Iso15693Poller* instance,
    uint16_t* blocks_total,
    uint16_t* failed_count,
    uint16_t* over_capacity,
    uint8_t* failed_bitmap,
    bool* used_gen1);

// True if the source stores real data in a gen1 backdoor block (56/57/62/63) that a gen1 fallback
// would overwrite -- so the write flow can warn before a possible gen1 clone. Source inspection only.
bool iso15693_poller_source_uses_gen1_blocks(const Iso15693_3Data* source);

// Wipe: write zeros to every writable data block on the card (UID left unchanged, like proxmark's
// 'hf 15 wipe'). Reports Success / Partial (some blocks failed) / Fail (nothing writable) /
// CardLost. Per-block detail is available via iso15693_poller_get_clone_result().
void iso15693_poller_start_wipe(Iso15693Poller* instance, Iso15693PollerCallback callback, void* context);

void iso15693_poller_stop(Iso15693Poller* instance);

Iso15693Data* iso15693_poller_get_data(Iso15693Poller* instance);
