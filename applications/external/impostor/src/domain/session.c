#include "include/domain/session.h"
#include "include/domain/game_rules.h"
#include <string.h>

static void shuffle_indices(uint8_t *indices, uint8_t n,
                            uint32_t (*rng)(void *), void *ctx) {
  for (uint8_t i = n; i > 1u; --i) {
    const uint32_t r = rng(ctx);
    const uint8_t j = (uint8_t)(r % (uint32_t)i);
    const uint8_t tmp = indices[i - 1u];
    indices[i - 1u] = indices[j];
    indices[j] = tmp;
  }
}

bool session_begin(GameSession *session, const GameSetup *setup,
                   uint32_t (*rng)(void *), void *rng_ctx) {
  if (!session || !setup || !rng) {
    return false;
  }
  if (!game_rules_validate_impostor_count(setup->player_count,
                                          setup->impostor_count)) {
    return false;
  }

  memset(session, 0, sizeof(*session));
  session->setup = *setup;

  uint8_t order[IMPOSTOR_MAX_PLAYERS];
  for (uint8_t i = 0; i < setup->player_count; ++i) {
    order[i] = i;
    session->is_impostor[i] = 0;
  }
  shuffle_indices(order, setup->player_count, rng, rng_ctx);

  for (uint8_t k = 0; k < setup->impostor_count; ++k) {
    session->is_impostor[order[k]] = 1;
  }

  for (uint8_t i = 0; i < setup->player_count; ++i) {
    session->reveal_order[i] = i;
  }
  shuffle_indices(session->reveal_order, setup->player_count, rng, rng_ctx);

  session->reveal_player_idx = 0;
  session->starter_picked = 0;
  return true;
}

void session_pick_starter(GameSession *session, uint32_t (*rng)(void *),
                          void *rng_ctx) {
  if (!session || !rng || session->setup.player_count == 0) {
    return;
  }
  const uint32_t r = rng(rng_ctx);
  session->starter_idx = (uint8_t)(r % (uint32_t)session->setup.player_count);
  session->starter_picked = 1;
}

uint8_t session_reveal_current_slot(const GameSession *session) {
  if (!session || session->reveal_player_idx >= session->setup.player_count) {
    return 0;
  }
  return session->reveal_order[session->reveal_player_idx];
}

bool session_reveal_is_impostor(const GameSession *session) {
  const uint8_t slot = session_reveal_current_slot(session);
  return session->is_impostor[slot] != 0;
}

void session_reveal_after_card_ok(GameSession *session) {
  session->reveal_player_idx++;
}

bool session_reveal_done(const GameSession *session) {
  return session->reveal_player_idx >= session->setup.player_count;
}
