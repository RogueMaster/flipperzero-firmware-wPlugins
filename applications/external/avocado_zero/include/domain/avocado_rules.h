#pragma once

#include "avocado_state.h"
#include <stdbool.h>
#include <stdint.h>

bool avocado_rules_is_game_over(const AvocadoData *state);

bool avocado_rules_should_show_victory(const AvocadoData *state);

void avocado_rules_apply_elapsed_days(AvocadoData *state, uint32_t now_ts);

void avocado_rules_on_primary_action(AvocadoData *state);

void avocado_rules_acknowledge_victory(AvocadoData *state, uint32_t now_ts);

void avocado_rules_debug_add_simulated_days(AvocadoData *state, uint32_t days);
void avocado_rules_debug_bump_roots(AvocadoData *state);
void avocado_rules_debug_preset_victory_pending(AvocadoData *state);
void avocado_rules_debug_preset_game_over(AvocadoData *state);
