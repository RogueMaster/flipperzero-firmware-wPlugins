#include "include/domain/session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_rng = 1;
static uint32_t test_rng(void *ctx) {
  (void)ctx;
  g_rng = g_rng * 1664525u + 1013904223u;
  return g_rng;
}

int main(void) {
  GameSetup setup = {0};
  setup.player_count = 4;
  setup.impostor_count = 1;
  setup.word_index = 3;
  snprintf(setup.names[0], IMPOSTOR_MAX_NAME_LEN, "%s", "A");
  snprintf(setup.names[1], IMPOSTOR_MAX_NAME_LEN, "%s", "B");
  snprintf(setup.names[2], IMPOSTOR_MAX_NAME_LEN, "%s", "C");
  snprintf(setup.names[3], IMPOSTOR_MAX_NAME_LEN, "%s", "D");

  GameSession s;
  const bool began = session_begin(&s, &setup, test_rng, NULL);
  assert(began);

  uint8_t impostors = 0;
  for (uint8_t i = 0; i < setup.player_count; ++i) {
    impostors += s.is_impostor[i];
  }
  assert(impostors == 1);

  while (!session_reveal_done(&s)) {
    (void)session_reveal_current_slot(&s);
    session_reveal_after_card_ok(&s);
  }

  session_pick_starter(&s, test_rng, NULL);
  assert(s.starter_idx < setup.player_count);

  return 0;
}
