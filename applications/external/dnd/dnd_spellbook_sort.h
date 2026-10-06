#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

typedef struct {
    uint32_t records;
    uint32_t source_scans;
    uint32_t merge_passes;
    bool changed;
    bool single_record_reinserted;
} DndSpellbookSortStats;

/* Validate every invocation: there is deliberately no timestamp/size sorted
 * cache to become stale after Grants, restore, import or direct SD edits.
 * Uses 24 run keys plus five bookkeeping keys and bounded byte buffers.
 * Unknown lines stay in their original slots. Record bytes are copied verbatim;
 * each destination slot retains its original LF/CRLF/no-final-newline ending.
 * A failed operation keeps the live file or its recoverable .sort.bak copy.
 * The optional history snapshot is refreshed with the completed sorted file.
 */
bool dnd_spellbook_sort(
    Storage* storage,
    const char* live,
    const char* snapshot,
    DndSpellbookSortStats* stats);
