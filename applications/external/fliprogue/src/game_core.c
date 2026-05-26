#include "game_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Core helpers, logging, RNG, visibility predicates, and player combat stats. */

bool fr_log_ends_sentence(char c) {
    return c == '.' || c == '!' || c == '?';
}

uint8_t fr_log_sentence_count(const char* text) {
    uint8_t count = 0;
    for(uint8_t i = 0; text[i] != '\0'; i++) {
        if(fr_log_ends_sentence(text[i])) count++;
    }
    return count;
}

const char* fr_second_log_phrase(const char* text) {
    for(uint8_t i = 0; text[i] != '\0'; i++) {
        if(fr_log_ends_sentence(text[i])) {
            i++;
            while(text[i] == ' ') i++;
            return &text[i];
        }
    }
    return text;
}

const char* fr_last_log_phrases(const char* text, uint8_t max_phrases) {
    while(fr_log_sentence_count(text) > max_phrases) text = fr_second_log_phrase(text);
    return text;
}

const char* fr_last_two_log_phrases(const char* text) {
    return fr_last_log_phrases(text, 2);
}

void fr_log(FrGame* game, const char* fmt, ...) {
    char next[FR_LOG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(next, sizeof(next), fmt, args);
    va_end(args);
    if(game->log[0] == '\0') {
        snprintf(game->log, sizeof(game->log), "%s", fr_last_two_log_phrases(next));
    } else {
        const char* keep = fr_last_two_log_phrases(game->log);
        char combined[FR_LOG_SIZE];
        size_t keep_len = strlen(keep);
        size_t next_len = strlen(next);
        if(keep_len + next_len + 2u < sizeof(combined)) {
            memcpy(combined, keep, keep_len);
            combined[keep_len] = ' ';
            memcpy(&combined[keep_len + 1u], next, next_len + 1u);
        } else {
            snprintf(combined, sizeof(combined), "%s", next);
        }
        snprintf(game->log, sizeof(game->log), "%s", fr_last_two_log_phrases(combined));
    }
}

void fr_event_secret(FrGame* game, uint8_t x, uint8_t y) {
    game->last_event = FR_EVENT_SECRET;
    game->event_x = x;
    game->event_y = y;
    game->event_tx = x;
    game->event_ty = y;
    game->event_glyph = '+';
}

void fr_event_projectile(FrGame* game, uint8_t sx, uint8_t sy, uint8_t tx, uint8_t ty, char glyph) {
    game->last_event = FR_EVENT_MON_PROJECTILE;
    game->event_x = sx;
    game->event_y = sy;
    game->event_tx = tx;
    game->event_ty = ty;
    game->event_glyph = glyph;
}

uint32_t fr_rand(FrGame* game) {
    game->seed = game->seed * 1664525u + 1013904223u;
    return game->seed;
}

uint8_t fr_rand_u8(FrGame* game, uint8_t limit) {
    if(limit == 0) return 0;
    return (uint8_t)(fr_rand(game) % limit);
}

bool fr_in_bounds(uint8_t x, uint8_t y) {
    return x < FR_MAP_W && y < FR_MAP_H;
}

bool fr_is_walkable(uint8_t terrain) {
    return terrain == FR_TERR_FLOOR || terrain == FR_TERR_DOOR_OPEN ||
           terrain == FR_TERR_DOOR_CLOSED || terrain == FR_TERR_GRASS ||
           terrain == FR_TERR_GRASS_TRAMPLED || terrain == FR_TERR_PUDDLE ||
           terrain == FR_TERR_SAND || terrain == FR_TERR_WATER || terrain == FR_TERR_STAIRS_DOWN ||
           terrain == FR_TERR_STAIRS_UP || terrain == FR_TERR_BUTTON || terrain == FR_TERR_ICE;
}

bool fr_blocks_sight(uint8_t terrain) {
    return terrain == FR_TERR_VOID || terrain == FR_TERR_WALL || terrain == FR_TERR_DOOR_CLOSED ||
           terrain == FR_TERR_GRASS;
}

uint8_t fr_abs_i8(int8_t value) {
    return (uint8_t)(value < 0 ? -value : value);
}

int8_t fr_sign_i8(int16_t value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

void fr_mark_known_potion(FrGame* game, uint8_t potion) {
    if(potion < 16) game->player.known_potions |= (uint16_t)(1u << potion);
}

void fr_mark_known_scroll(FrGame* game, uint8_t scroll) {
    if(scroll < 16) game->player.known_scrolls |= (uint16_t)(1u << scroll);
}

void fr_mark_known_trinket(FrGame* game, uint8_t trinket) {
    if(trinket < 16) game->player.known_trinkets |= (uint16_t)(1u << trinket);
}

bool fr_knows_potion(const FrGame* game, uint8_t potion) {
    return potion < 16 && (game->player.known_potions & (uint16_t)(1u << potion)) != 0;
}

bool fr_knows_scroll(const FrGame* game, uint8_t scroll) {
    return scroll < 16 && (game->player.known_scrolls & (uint16_t)(1u << scroll)) != 0;
}

bool fr_knows_trinket(const FrGame* game, uint8_t trinket) {
    return trinket < 16 && (game->player.known_trinkets & (uint16_t)(1u << trinket)) != 0;
}

uint8_t fr_player_damage(const FrGame* game) {
    uint8_t str = game->player.str;
    if(fr_hunger_state(game) != FR_HUNGER_OK && str > 0) str--;
    if(game->player.class_id == FR_CLASS_RANGER || game->player.class_id == FR_CLASS_MAGE) {
        return (uint8_t)(1 + game->player.dagger_lvl + str / 4);
    }
    return (uint8_t)(2 + game->player.sword_lvl + str / 3);
}

uint8_t fr_player_ranged_damage(const FrGame* game) {
    if(game->player.class_id == FR_CLASS_RANGER) return (uint8_t)(2 + game->player.bow_lvl + game->player.dex / 3);
    if(game->player.class_id == FR_CLASS_MAGE) return (uint8_t)(2 + game->player.staff_lvl + game->player.wil / 3);
    return 0;
}

bool fr_player_can_ranged(const FrGame* game) {
    if(game->player.class_id == FR_CLASS_RANGER) return game->player.arrows > 0;
    if(game->player.class_id == FR_CLASS_MAGE) return game->player.charges > 0;
    return false;
}

bool fr_actor_visible_to_player(const FrGame* game, const FrActor* actor) {
    if(!actor || !actor->active) return false;
    if((game->tiles[actor->y][actor->x] & FR_TILE_VISIBLE) == 0) return false;
    return (actor->flags & FR_ACTOR_HIDDEN) == 0;
}

uint8_t fr_player_block(const FrGame* game) {
    uint8_t block = (uint8_t)(game->player.shield_lvl + game->player.body_lvl);
    if(game->player.class_id == FR_CLASS_WARRIOR && (game->player.perks & FR_PERK_5) != 0) {
        uint8_t adjacent = 0;
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            const FrActor* actor = &game->actors[i];
            if(!actor->active) continue;
            int8_t dx = (int8_t)actor->x - (int8_t)game->player.x;
            int8_t dy = (int8_t)actor->y - (int8_t)game->player.y;
            if((fr_abs_i8(dx) + fr_abs_i8(dy)) == 1) adjacent++;
        }
        if(adjacent >= 2) block++;
    }
    return block;
}
