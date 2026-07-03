#ifndef HOLDEM_STORAGE_H
#define HOLDEM_STORAGE_H

#include "holdem_types.h"

// Returns true when a valid save file exists on external storage.
bool save_exists_on_storage(void);

// Removes the single save slot used by the app.
void delete_save_on_storage(void);

// Writes the complete game state plus persisted gameplay settings.
bool save_progress(
    const HoldemGame* game,
    uint8_t ai_level_pct,
    int32_t configured_small_blind,
    bool progressive_blinds_enabled,
    uint8_t progressive_period_hands,
    int32_t progressive_step_sb,
    int32_t progressive_next_raise_hand_no);

// Loads a complete game state plus persisted gameplay settings; also accepts legacy saves.
bool load_progress(
    HoldemGame* game,
    uint8_t* ai_level_pct,
    int32_t* configured_small_blind,
    bool* progressive_blinds_enabled,
    uint8_t* progressive_period_hands,
    int32_t* progressive_step_sb,
    int32_t* progressive_next_raise_hand_no);

#endif
