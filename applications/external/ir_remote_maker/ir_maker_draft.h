#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <infrared_signal.h>
#include <infrared_error_code.h>

/* Hard caps on a draft remote. IR_MAKER_RAW_BUDGET bounds the total number of
 * raw timings across ALL buttons: a single raw capture can be up to
 * MAX_TIMINGS_AMOUNT (1024) uint32 = 4 KiB, so an unbounded pile of raw
 * buttons would exhaust the heap. Parsed buttons cost no raw budget. */
#define IR_MAKER_NAME_MAX    24U
#define IR_MAKER_MAX_BUTTONS 32U
#define IR_MAKER_RAW_BUDGET  4096U

/* One button in the draft: a display name plus an owned InfraredSignal
 * (parsed or raw). Both are heap-allocated and freed by the draft. */
typedef struct {
    FuriString* name;
    InfraredSignal* signal;
} IrMakerButton;

/* The whole in-memory remote being composed. Fixed-capacity array keeps the
 * memory footprint predictable on the Flipper's small heap. */
typedef struct {
    IrMakerButton items[IR_MAKER_MAX_BUTTONS];
    size_t count;
    size_t raw_timings_used;
    FuriString* remote_name;
} IrMakerDraft;

/** Allocate an empty draft. */
IrMakerDraft* ir_maker_draft_alloc(void);

/** Free the draft and every button signal/name it owns. */
void ir_maker_draft_free(IrMakerDraft* draft);

/** Drop all buttons and reset the raw budget (keeps the allocation). */
void ir_maker_draft_reset(IrMakerDraft* draft);

/** Append a copy of `signal` under `name`.
 * Returns false if the draft is full or adding a raw signal would exceed
 * IR_MAKER_RAW_BUDGET; nothing is added in that case. */
bool ir_maker_draft_add(IrMakerDraft* draft, const char* name, const InfraredSignal* signal);

/** Replace button idx's display name. */
void ir_maker_draft_set_name(IrMakerDraft* draft, size_t idx, const char* name);

/** Replace button idx's signal with a copy of `signal`, keeping the raw budget
 * accounting consistent. Returns false if the swap would exceed the budget. */
bool ir_maker_draft_set_signal(IrMakerDraft* draft, size_t idx, const InfraredSignal* signal);

/** Delete button idx, shifting the tail down. */
void ir_maker_draft_delete(IrMakerDraft* draft, size_t idx);

/** Move the button at idx to new_idx, shifting the others to make room. */
void ir_maker_draft_move(IrMakerDraft* draft, size_t idx, size_t new_idx);

/** Write the whole draft to `path` in one pass as a standard .ir file. */
InfraredErrorCode ir_maker_draft_save(IrMakerDraft* draft, Storage* storage, const char* path);

/** Load an existing .ir file into the (already reset) draft. */
InfraredErrorCode ir_maker_draft_load(IrMakerDraft* draft, Storage* storage, const char* path);
