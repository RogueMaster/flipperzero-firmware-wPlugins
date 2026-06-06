#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TUTU_PROGRESS_BYTES 13 // 100 bits, one per level

typedef struct {
    uint16_t highest_unlocked; // index of highest playable level (0-based)
    uint8_t completed[TUTU_PROGRESS_BYTES];
} TutuProgress;

void tutu_progress_default(TutuProgress *p);
bool tutu_progress_is_completed(const TutuProgress *p, uint16_t n);
void tutu_progress_mark_completed(TutuProgress *p, uint16_t n);
// Mark level n done and unlock n+1 (clamped to count-1). Returns the new highest_unlocked.
uint16_t tutu_progress_complete_and_unlock(TutuProgress *p, uint16_t n, uint16_t count);
bool tutu_progress_is_unlocked(const TutuProgress *p, uint16_t n);
