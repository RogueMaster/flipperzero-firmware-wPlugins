#pragma once

#include "include/domain/game_limits.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t player_count;
  uint8_t impostor_count;
  uint16_t word_index;
  char names[IMPOSTOR_MAX_PLAYERS][IMPOSTOR_MAX_NAME_LEN];
} GameSetup;

typedef struct {
  GameSetup setup;
  uint8_t is_impostor[IMPOSTOR_MAX_PLAYERS];
  uint8_t reveal_order[IMPOSTOR_MAX_PLAYERS];
  uint8_t reveal_player_idx;
  uint8_t starter_idx;
  uint8_t starter_picked;
} GameSession;

bool session_begin(GameSession *session, const GameSetup *setup,
                   uint32_t (*rng)(void *), void *rng_ctx);

void session_pick_starter(GameSession *session, uint32_t (*rng)(void *),
                          void *rng_ctx);

uint8_t session_reveal_current_slot(const GameSession *session);

bool session_reveal_is_impostor(const GameSession *session);

void session_reveal_after_card_ok(GameSession *session);

bool session_reveal_done(const GameSession *session);
