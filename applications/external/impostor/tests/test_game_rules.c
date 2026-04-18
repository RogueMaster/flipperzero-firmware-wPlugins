#include "include/domain/game_rules.h"
#include <assert.h>

int main(void) {
  assert(game_rules_validate_player_count(2) == false);
  assert(game_rules_validate_player_count(3) == true);
  assert(game_rules_validate_player_count(64) == true);
  assert(game_rules_validate_player_count(65) == false);

  assert(game_rules_max_impostors(3) == 1);
  assert(game_rules_max_impostors(8) == 6);
  assert(game_rules_max_impostors(24) == 22);
  assert(game_rules_max_impostors(50) == 48);

  assert(game_rules_suggested_impostors(24) == 6);
  assert(game_rules_suggested_impostors(50) == 12);
  assert(game_rules_suggested_impostors(4) == 1);

  assert(game_rules_validate_impostor_count(5, 0) == false);
  assert(game_rules_validate_impostor_count(5, 1) == true);
  assert(game_rules_validate_impostor_count(5, 3) == true);
  assert(game_rules_validate_impostor_count(5, 4) == false);
  assert(game_rules_validate_impostor_count(50, 1) == true);
  assert(game_rules_validate_impostor_count(50, 48) == true);
  assert(game_rules_validate_impostor_count(50, 49) == false);

  return 0;
}
