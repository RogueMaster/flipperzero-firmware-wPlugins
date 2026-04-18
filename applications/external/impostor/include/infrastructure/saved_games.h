#pragma once

#include "include/domain/game_limits.h"
#include "include/domain/session.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t created_ts;
  uint8_t player_count;
  uint8_t impostor_count;
  uint16_t word_index;
  char title[IMPOSTOR_MAX_SESSION_TITLE_LEN];
  char names[IMPOSTOR_MAX_PLAYERS][IMPOSTOR_MAX_NAME_LEN];
} SavedGameRecord;

void saved_games_record_from_setup(SavedGameRecord *out, const GameSetup *setup,
                                   uint32_t created_ts, const char *title);

void saved_games_record_refresh_kept_meta(SavedGameRecord *out,
                                          const SavedGameRecord *meta_from,
                                          const GameSetup *setup);

bool saved_games_load(SavedGameRecord *out, uint8_t *count_out);

bool saved_games_append(SavedGameRecord *list, uint8_t *count_io,
                        const SavedGameRecord *record);

bool saved_games_replace_at(SavedGameRecord *list, uint8_t *count_io,
                            uint8_t index, const SavedGameRecord *record);

bool saved_games_delete_at(SavedGameRecord *list, uint8_t *count_io,
                           uint8_t index);
