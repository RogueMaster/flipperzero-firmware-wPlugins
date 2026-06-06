#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

bool fr_log_ends_sentence(char c);
uint8_t fr_log_sentence_count(const char* text);
const char* fr_second_log_phrase(const char* text);
const char* fr_last_log_phrases(const char* text, uint8_t max_phrases);
const char* fr_last_two_log_phrases(const char* text);
void fr_log(FrGame* game, const char* fmt, ...);
void fr_event_secret(FrGame* game, uint8_t x, uint8_t y);
void fr_event_projectile(FrGame* game, uint8_t sx, uint8_t sy, uint8_t tx, uint8_t ty, char glyph);
uint32_t fr_rand(FrGame* game);
uint8_t fr_rand_u8(FrGame* game, uint8_t limit);
bool fr_in_bounds(uint8_t x, uint8_t y);
bool fr_is_walkable(uint8_t terrain);
bool fr_blocks_sight(uint8_t terrain);
uint8_t fr_abs_i8(int8_t value);
int8_t fr_sign_i8(int16_t value);
void fr_mark_known_potion(FrGame* game, uint8_t potion);
void fr_mark_known_scroll(FrGame* game, uint8_t scroll);
void fr_mark_known_trinket(FrGame* game, uint8_t trinket);
bool fr_knows_potion(const FrGame* game, uint8_t potion);
bool fr_knows_scroll(const FrGame* game, uint8_t scroll);
bool fr_knows_trinket(const FrGame* game, uint8_t trinket);
uint8_t fr_player_damage(const FrGame* game);
uint8_t fr_player_ranged_damage(const FrGame* game);
uint8_t fr_player_block(const FrGame* game);
