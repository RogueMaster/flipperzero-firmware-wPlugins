#include "tutorial_level.h"

bool firstLevelCompleted = false;
bool startedGame = false;
bool hasSpawnedFirstMob = false;
void tutorial_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos) {
    if(!game_menu_tutorial_selected) return;

    PlayerContext* playerCtx = context;

    //Completed first level and reached end (willing to transition)
    if(firstLevelCompleted && roundf(pos->x) == WORLD_BORDER_RIGHT_X) {
        entity_pos_set(self, (Vector){WORLD_TRANSITION_LEFT_STARTING_POINT, pos->y});
        pos->x = WORLD_TRANSITION_LEFT_STARTING_POINT;
        targetX = WORLD_BORDER_LEFT_X;
        startedGame = true;
        playerCtx->sprite = playerCtx->sprite_right;
    }

    //They transitioned to the second level (from first level)
    if(firstLevelCompleted && pos->x < WORLD_TRANSITION_LEFT_ENDING_POINT) {
        //Smooth level transitioning code, hence border is extended in this snippet.
        //(0 -> right border) instead of (left border -> right border)
        pos->x = CLAMP(
            lerp(pos->x, targetX, jumpSpeed),
            WORLD_BORDER_RIGHT_X,
            WORLD_TRANSITION_LEFT_STARTING_POINT);
        pos->y = CLAMP(pos->y, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);
        entity_pos_set(self, *pos);

        return;
    }

    //Finally spawn the next mob after completing the first level.
    if(firstLevelCompleted && startedGame && roundf(pos->x) > WORLD_BORDER_LEFT_X) {
        if(!hasSpawnedFirstMob) {
            //Spawn new mob
            enemy_spawn(gameLevel, manager, (Vector){110, 49}, 8000, false);
            hasSpawnedFirstMob = true;
            firstMobSpawnTicks = furi_get_tick();
        }
    }
}

bool renderUsername = true;

void tutorial_level_player_render(GameManager* manager, Canvas* canvas, void* context) {
    if(!game_menu_tutorial_selected) return;
    PlayerContext* player = context;

    //Render player username
    if(renderUsername && globalPlayer != NULL) {
        Vector pos = entity_pos_get(globalPlayer);
        canvas_printf(canvas, pos.x - 7, pos.y - 10, "%s", username);
    }
    if(kills == 0) {
        static uint32_t dataHolder;
        static uint32_t dataHolder2;
        canvas_printf_blinking(canvas, 13, 10, 400, 200, "DEADZONE", &dataHolder);
        canvas_printf_blinking(canvas, 15, 19, 400, 200, "developed by retrooper", &dataHolder2);

    } else if(kills == 1) {
        if(furi_get_tick() - firstKillTick < 2000) {
            canvas_printf(canvas, 30, 30, "Great job!");
            static bool firstKillMsg;
            if(!firstKillMsg) {
                firstKillMsg = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_single_vibro);
                furi_delay_ms(2000);
            }
        } else if(furi_get_tick() - firstKillTick < 7500) {
            renderUsername = false;
            canvas_printf(canvas, 20, 40, "You got your first kill!");
            static bool nextMsg;
            if(!nextMsg) {
                nextMsg = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_single_vibro);
                furi_delay_ms(2000);
            }
        } else if(!startedGame) {
            if(player->sprite == player->sprite_right) {
                player->sprite = player->sprite_right_shadowed;
            } else if(player->sprite == player->sprite_left) {
                player->sprite = player->sprite_left_shadowed;
            }
            canvas_printf(canvas, 47, 40, "Continue!");
            canvas_printf(canvas, 100, 53, "-->");
            canvas_draw_box(canvas, 126, 44, 2, 16);
            static bool continued;
            if(!continued) {
                continued = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_success);
                firstLevelCompleted = true;
            }
        }
    } else if(kills == 2) {
        static bool secondKill;
        if(!secondKill) {
            secondKill = true;
            secondKillTick = furi_get_tick();
        }

        if(furi_get_tick() - secondKillTick < 5000) {
            canvas_printf(
                canvas,
                20,
                30,
                health > ((int)roundf(STARTING_PLAYER_HEALTH / 2.0f)) ? "Was that easy?..." :
                                                                        "You seem to struggle?");
            for(int i = 5; i > 0; i--) {
                furi_delay_ms(i);
            }

        } else if(furi_get_tick() - secondKillTick > 7000 && furi_get_tick() - secondKillTick < 9000) {
            if(health > ((int)roundf(STARTING_PLAYER_HEALTH / 2.0f))) {
                canvas_printf(canvas, 5, 30, "Now the final challenge!");
            } else {
                canvas_printf(canvas, 5, 30, "Well, here's the real");
                canvas_printf(canvas, 5, 40, "challenge!");
            }
            for(int i = 5; i > 0; i--) {
                furi_delay_ms(i);
            }
        } else if(furi_get_tick() - secondKillTick > 10000) {
            static bool hasSpawnedFinalBoss1;
            if(!hasSpawnedFinalBoss1) {
                hasSpawnedFinalBoss1 = true;
                enemy_spawn(
                    gameLevel,
                    manager,
                    (Vector){WORLD_BORDER_RIGHT_X - 30, WORLD_BORDER_TOP_Y},
                    0,
                    false);
            }
            static bool hasSpawnedFinalBoss2;
            if(furi_get_tick() - secondKillTick > 12000 && !hasSpawnedFinalBoss2) {
                hasSpawnedFinalBoss2 = true;
                enemy_spawn(
                    gameLevel,
                    manager,
                    (Vector){WORLD_BORDER_RIGHT_X, WORLD_BORDER_TOP_Y},
                    0,
                    false);
            }
        }
        if(furi_get_tick() - secondKillTick > 8000) {
            int tickThousands = (int)roundf((furi_get_tick() - secondKill) / 1000.0F);
            furi_delay_ms(tickThousands % 3 == 0 ? 10 : 0);
        }
    } else if(kills == 4) {
        canvas_draw_box(canvas, 126, 44, 2, 16);
        //Text will be blinking/flashing
        if((int)roundf((furi_get_tick() - secondKillTick) / 1000.0f) % 2 == 0) {
            canvas_printf(canvas, 20, 30, "You've completed the");
            canvas_printf(canvas, 20, 40, "tutorial!");
        }
    }

    //Draw the right door after completing the first level.
    if(firstLevelCompleted && startedGame) {
        canvas_draw_box(canvas, 0, 44, 2, 16);
    }

    if(hasSpawnedFirstMob) {
        if(furi_get_tick() - firstMobSpawnTicks < 3000) {
            canvas_printf(canvas, 0, 40, "Welcome to the next level!");
            furi_delay_ms(10);
        } else if(furi_get_tick() - firstMobSpawnTicks < 7000) {
            canvas_printf(canvas, 50, 40, "Fight!");
            for(int i = 5; i > 0; i--) {
                for(int j = 0; j < 5; j++) {
                    furi_delay_ms(i);
                }
            }
            renderUsername = true;
        }
    }
}

void tutorial_level_enemy_render(GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(canvas);
    UNUSED(context);
    if(health == STARTING_PLAYER_HEALTH && furi_get_tick() - gameBeginningTick < 5000) {
        canvas_printf(canvas, 30, 20, "Welcome to the");
        canvas_printf(canvas, 30, 36, "DEADZONE tutorial!");
    }
}
