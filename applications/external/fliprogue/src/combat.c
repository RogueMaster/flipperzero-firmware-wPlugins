#include "combat.h"

#include "game_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void fr_set_game_over(FrGame* game, uint8_t cause, const char* fmt, ...) {
    if(game->mode == FR_MODE_GAME_OVER || game->mode == FR_MODE_VICTORY) return;
    char previous[FR_LOG_SIZE];
    snprintf(previous, sizeof(previous), "%s", game->log);
    game->mode = FR_MODE_GAME_OVER;
    game->death_cause = cause;
    game->log[0] = '\0';
    game->death_log[0] = '\0';
    if(fmt) {
        char msg[FR_LOG_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);
        if(previous[0] != '\0') {
            char combined[FR_LOG_SIZE];
            const char* keep = fr_last_two_log_phrases(previous);
            size_t keep_len = strlen(keep);
            size_t msg_len = strlen(msg);
            if(keep_len + msg_len + 2u < sizeof(combined)) {
                memcpy(combined, keep, keep_len);
                combined[keep_len] = ' ';
                memcpy(&combined[keep_len + 1u], msg, msg_len + 1u);
            } else {
                if(msg_len >= sizeof(combined)) msg_len = sizeof(combined) - 1u;
                memcpy(combined, msg, msg_len);
                combined[msg_len] = '\0';
            }
            snprintf(game->death_log, sizeof(game->death_log), "%s", fr_last_log_phrases(combined, 3));
        } else {
            snprintf(game->death_log, sizeof(game->death_log), "%s", fr_last_log_phrases(msg, 3));
        }
        snprintf(game->log, sizeof(game->log), "%s", fr_last_log_phrases(msg, 1));
    }
}

void fr_kill_actor(FrGame* game, FrActor* actor) {
    actor->hp = 0;
    actor->active = false;
    game->player.gold += actor->type == FR_MON_GOBLIN ? 3 : 1;
    fr_award_xp(game, actor->type == FR_MON_YONDER_WARDEN ? 20 : (actor->type == FR_MON_DRAGON ? 8 : 1));
}
