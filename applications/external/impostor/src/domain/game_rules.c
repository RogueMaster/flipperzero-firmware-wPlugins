#include "include/domain/game_rules.h"

uint8_t game_rules_suggested_impostors(uint8_t player_count) {
  const uint8_t max_k = game_rules_max_impostors(player_count);
  if (max_k == 0) {
    return 1;
  }
  uint32_t sug = (uint32_t)player_count * 25u / 100u;
  if (sug < 1u) {
    sug = 1u;
  }
  if (sug > (uint32_t)max_k) {
    return max_k;
  }
  return (uint8_t)sug;
}

bool game_rules_validate_player_count(uint8_t n) {
  return n >= IMPOSTOR_MIN_PLAYERS && n <= IMPOSTOR_MAX_PLAYERS;
}

uint8_t game_rules_max_impostors(uint8_t player_count) {
  if (!game_rules_validate_player_count(player_count)) {
    return 0;
  }
  if (player_count <= 2) {
    return 0;
  }
  return (uint8_t)(player_count - 2u);
}

bool game_rules_validate_impostor_count(uint8_t player_count,
                                        uint8_t impostor_count) {
  if (!game_rules_validate_player_count(player_count)) {
    return false;
  }
  if (impostor_count < 1) {
    return false;
  }
  return impostor_count <= game_rules_max_impostors(player_count);
}
