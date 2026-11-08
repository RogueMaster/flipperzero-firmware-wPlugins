#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cards_data.h"
#include "game.h"

typedef enum {
    LoteriaAutoManual = 0,
    LoteriaAuto5s,
    LoteriaAuto10s,
    LoteriaAuto15s,
    LoteriaAutoCount,
} LoteriaAutoMode;

typedef struct {
    uint32_t games_played;
    uint32_t games_completed;
    uint32_t games_interrupted;
    uint32_t cards_sung;
    uint32_t last_played_epoch;
} LoteriaStats;

typedef struct {
    bool skip_welcome;
    uint8_t auto_mode;
    LoteriaStats stats;
    LoteriaGame saved_game;
    bool has_saved_game;
} LoteriaPersist;

void loteria_persist_init_defaults(LoteriaPersist* persist);
bool loteria_persist_load(LoteriaPersist* persist);
bool loteria_persist_save(const LoteriaPersist* persist);

uint32_t loteria_auto_mode_ms(uint8_t mode);
const char* loteria_auto_mode_label(uint8_t mode);
