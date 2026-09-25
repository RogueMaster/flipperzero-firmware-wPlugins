#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TUTU_PROGRESS_BYTES 13 // 100 bits, one per level

typedef struct {
    uint16_t highest_unlocked; // index of highest playable level (0-based)
    uint8_t completed[TUTU_PROGRESS_BYTES];
} TutuProgress;

void tutu_progress_default(TutuProgress* p);
bool tutu_progress_is_completed(const TutuProgress* p, uint16_t n);
void tutu_progress_mark_completed(TutuProgress* p, uint16_t n);
// Mark level n done and unlock n+1 (clamped to count-1). Returns the new highest_unlocked.
uint16_t tutu_progress_complete_and_unlock(TutuProgress* p, uint16_t n, uint16_t count);
bool tutu_progress_is_unlocked(const TutuProgress* p, uint16_t n);

// Number of quarantine slots for an unreadable save file: "<file>.bad", then
// "<file>.bad1".."<file>.bad9".
#define TUTU_PROGRESS_BACKUP_SLOTS 10

// First free backup slot index for an unreadable save file, so a later corruption never
// overwrites an earlier kept copy. taken[i] is true if slot i's file already exists.
// Returns -1 if every slot is taken.
int tutu_progress_backup_slot(const bool taken[TUTU_PROGRESS_BACKUP_SLOTS]);
