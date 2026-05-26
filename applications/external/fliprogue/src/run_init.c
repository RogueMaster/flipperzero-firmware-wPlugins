#include "run_init.h"

#include "game_core.h"
#include "progression.h"

#include <stdio.h>
#include <string.h>

static void fr_generate_labels(FrGame* game) {
    static const char* colors[] = {"Red", "Green", "Black", "Milky", "Amber", "Blue", "Grey", "Violet"};
    static const char* runes[] = {
        "VEX", "MUR", "OOL", "LUMA", "DREG", "ZUN", "PAX", "KETH", "ZOL", "YON",
        "ASH", "VEIL", "NIX", "ROOK", "VALE", "OM", "IX", "DUN", "HAL", "FER",
        "NOX", "BRI", "THUL", "KOR", "SAR", "ULM", "QIR", "BAZ", "EKO", "JIN",
        "ORR", "TYR", "MAK", "SIV",
    };
    enum { RuneCount = sizeof(runes) / sizeof(runes[0]) };
    for(uint8_t i = 1; i < FR_POTION_MAX; i++) {
        uint8_t pick = (uint8_t)((i - 1 + game->seed) % 8);
        snprintf(game->potion_labels[i], FR_LABEL_SIZE, "%s Potion", colors[pick]);
    }
    for(uint8_t i = 1; i < FR_SCROLL_MAX; i++) {
        uint8_t a = (uint8_t)((i + game->seed) % RuneCount);
        uint8_t b = (uint8_t)((i * 7 + game->seed) % RuneCount);
        if(a == b) b = (uint8_t)((b + 1) % RuneCount);
        for(uint8_t tries = 0; tries < RuneCount; tries++) {
            snprintf(game->scroll_labels[i], FR_LABEL_SIZE, "%s %s", runes[a], runes[b]);
            bool unique = true;
            for(uint8_t prev = 1; prev < i; prev++) {
                if(strcmp(game->scroll_labels[prev], game->scroll_labels[i]) == 0) unique = false;
            }
            if(unique) break;
            b = (uint8_t)((b + 1) % RuneCount);
            if(a == b) b = (uint8_t)((b + 1) % RuneCount);
        }
    }
    for(uint8_t i = 1; i < FR_TRINKET_MAX; i++) {
        uint8_t pick = (uint8_t)((i * 5 + game->seed) % RuneCount);
        for(uint8_t tries = 0; tries < RuneCount; tries++) {
            snprintf(game->trinket_labels[i], FR_LABEL_SIZE, "Charm %s", runes[pick]);
            bool unique = true;
            for(uint8_t prev = 1; prev < i; prev++) {
                if(strcmp(game->trinket_labels[prev], game->trinket_labels[i]) == 0) unique = false;
            }
            if(unique) break;
            pick = (uint8_t)((pick + 1) % RuneCount);
        }
    }
}

void fr_game_init(FrGame* game, uint32_t seed) {
    fr_game_init_class(game, seed, FR_CLASS_WARRIOR);
}

void fr_game_init_class(FrGame* game, uint32_t seed, uint8_t class_id) {
    memset(game, 0, sizeof(*game));
    game->seed = seed != 0 ? seed : 1u;
    game->run_seed = game->seed;
    game->mode = FR_MODE_PLAYING;
    game->player.class_id = class_id;
    game->player.level = 1;
    game->player.hunger = 240;
    game->player.food = 3;

    if(class_id == FR_CLASS_RANGER) {
        game->player.hp = game->player.max_hp = 20;
        game->player.str = 3;
        game->player.dex = 6;
        game->player.wil = 3;
        game->player.dagger_lvl = 0;
        game->player.bow_lvl = 1;
        game->player.arrows = 12;
    } else if(class_id == FR_CLASS_MAGE) {
        game->player.hp = game->player.max_hp = 18;
        game->player.str = 3;
        game->player.dex = 3;
        game->player.wil = 6;
        game->player.dagger_lvl = 0;
        game->player.staff_lvl = 1;
        game->player.charges = 7;
        game->player.known_scrolls = 0xFFFFu;
    } else {
        game->player.hp = game->player.max_hp = 24;
        game->player.str = 6;
        game->player.dex = 3;
        game->player.wil = 3;
        game->player.sword_lvl = 0;
        game->player.shield_lvl = 1;
    }

    fr_generate_labels(game);
    fr_generate_floor(game, 1);
    fr_log(game, "%s enters.", fr_player_class_name(class_id));
}
