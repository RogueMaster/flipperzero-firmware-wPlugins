#include "game_level.h"

bool started = false;
bool startedLevelTwo = false;
int welcomeTicks = 0;
#define MAX_DOORS 5
struct game_door doors[MAX_DOORS];
struct game_obstacle obstacles[MAX_OBSTACLES];
int totalObstacles = 0;
bool canSpawnDoubleObstacles = true;
int maxObstaclesFirst = 10;
int maxObstaclesSecond = 25;
int playerLevel = 0;
bool startedNextContinuousTask = false;

void emptyFunction() {
}

void postObstacleDestructionTask() {
    //Range is 30-100
    int x = rand() % (120 - 10 + 1) + 10;
    obstacles[totalObstacles % MAX_OBSTACLES] = (struct game_obstacle){
        x,
        8,
        OBSTACLE_WIDTH,
        0,
        x < 65,
        true,
        totalObstacles < maxObstaclesFirst ? postObstacleDestructionTask : emptyFunction};

    totalObstacles++;

    if(canSpawnDoubleObstacles) {
        if(totalObstacles > maxObstaclesFirst) {
            x = rand() % (120 - 10 + 1) + 10;
            obstacles[totalObstacles % MAX_OBSTACLES] = (struct game_obstacle){
                x,
                8,
                OBSTACLE_WIDTH,
                0,
                x < 65,
                true,
                totalObstacles < (maxObstaclesFirst + maxObstaclesSecond) ?
                    postObstacleDestructionTask :
                    emptyFunction};
            totalObstacles++;
        }
    }
}

uint32_t enemySpawnCount = 1;

void postDoorEntryTask4() {
    playerLevel++;

    for(uint32_t i = 0; i < enemySpawnCount; i++) {
        if(enemySpawnCount == 1) {
            enemyShootingDelay = 800;
        } else {
            enemyShootingDelay -= 50;
            shootingDelay -= 50;
        }
        enemy_spawn(
            gameLevel,
            globalGameManager,
            (Vector){
                MIN(10 + 80 * i, (uint32_t)WORLD_BORDER_RIGHT_X),
                30,
            },
            800,
            true);
    }
    //Increase enemy shooting delay
}

void postDoorEntryTask3() {
    playerLevel++;

    _enemy_spawn(gameLevel, globalGameManager, (Vector){120, 30}, 500, false, 2 * ENEMY_LIVES);
    //Increase enemy shooting delay
    enemyShootingDelay = 2200;

    totalObstacles = 0;
    maxObstaclesFirst = 30;
    canSpawnDoubleObstacles = false;

    obstacles[0] = (struct game_obstacle){
        60, 16, OBSTACLE_WIDTH, 0, false, true, postObstacleDestructionTask};
}

void postDoorEntryTask2() {
    playerLevel++;
    enemy_spawn(gameLevel, globalGameManager, (Vector){10, 30}, 500, true);
    //Increase enemy shooting delay
    enemyShootingDelay = 600;
}

void postDoorEntryTask() {
    obstacles[0] = (struct game_obstacle){
        60, 16, OBSTACLE_WIDTH, 0, false, true, postObstacleDestructionTask};
    playerLevel++;
}

void game_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos) {
    if(!game_menu_game_selected) return;
    PlayerContext* playerCtx = context;
    UNUSED(self);

    static bool hasProgressed = false;
    if(kills == 1) {
        if(!hasProgressed) {
            playerLevel++;
            hasProgressed = true;
            enemy_spawn(gameLevel, manager, (Vector){120, 0}, 3000, false);
        }
        static uint16_t spawnedThirdMob = 0;
        const uint16_t tickDuration = 100;
        if(spawnedThirdMob < (tickDuration + 1) && spawnedThirdMob++ == tickDuration) {
            enemy_spawn(gameLevel, manager, (Vector){10, 0}, 3000, true);
        }
    }

    static bool completedFirstTask = false;
    static bool thirdTask = false;

    static uint32_t nextKillCount = 0;

    if(kills == 3) {
        if(!completedFirstTask) {
            playerLevel++;
            completedFirstTask = true;
            doors[0] = (struct game_door){
                58, 24, 25, 32, true, 0, "Avoid the obstacles!", 34000, postDoorEntryTask};
        }
    } else if(kills == 4) {
        static uint16_t spawnedFifthMob = 0;
        const uint16_t tickDuration = 100;
        if(spawnedFifthMob < (tickDuration + 1) && spawnedFifthMob++ == tickDuration) {
            enemy_spawn(gameLevel, globalGameManager, (Vector){30, 30}, 500, true);
        }
    } else if(kills == 5) {
        //Next door!
        if(!thirdTask) {
            thirdTask = true;
            doors[0] = (struct game_door){
                30, 24, 25, 32, true, 0, "Enemies and Obstacles!", 34000, postDoorEntryTask3};
        }
    } else if(kills > 5) {
        if(!startedNextContinuousTask) {
            bool allObstaclesGone = true;
            for(int i = 0; i < MAX_OBSTACLES; i++) {
                struct game_obstacle obstacle = obstacles[i];
                if(obstacle.visible) {
                    allObstaclesGone = false;
                    break;
                }
            }
            if(allObstaclesGone) {
                if(nextKillCount == 0) {
                    nextKillCount = 6 + enemySpawnCount;
                    doors[0] = (struct game_door){
                        30,
                        24,
                        25,
                        32,
                        true,
                        0,
                        "Enemies and Obstacles!",
                        34000,
                        postDoorEntryTask4};
                }
                startedNextContinuousTask = true;
            }
        } else if(nextKillCount == kills) {
            enemySpawnCount++;
            nextKillCount += enemySpawnCount;
            doors[0] = (struct game_door){
                30, 24, 25, 32, true, 0, "Enemies and Obstacles!", 34000, postDoorEntryTask4};
        }
    }

    //Door processing
    InputState input = game_manager_input_get(manager);

    int enteredDoorIndex = -1;
    for(int i = 0; i < MAX_DOORS; i++) {
        struct game_door door = doors[i];

        if(!door.visible) continue;
        if(input.pressed & GameKeyDown &&
           fabsf(pos->x - (door.x + door.width / 2.0f)) < (door.width / 2.0f) &&
           fabsf(pos->y - (door.y + door.height / 2.0f)) < (door.height / 2.0f)) {
            enteredDoorIndex = i;
            door.transitionTicks = furi_get_tick();
            void (*task)(void) = doors[i].postTask;
            if(task != NULL) {
                task();
            }

            playerCtx->sprite = playerCtx->sprite_forward;
            break;
            //Went in door!
        }
    }

    if(enteredDoorIndex != -1) {
        doors[enteredDoorIndex].visible = false;
    }

    for(int i = 0; i < MAX_OBSTACLES; i++) {
        struct game_obstacle* obstacle = &obstacles[i];
        if(obstacle == NULL || !obstacle->visible) continue;
        if(obstacle->direction) {
            obstacle->x += OBSTACLE_SPEED / 4.0f;
        } else {
            obstacle->x -= OBSTACLE_SPEED / 4.0f;
        }
        obstacle->y += OBSTACLE_SPEED;

        if(obstacle->x < WORLD_BORDER_LEFT_X || obstacle->x > WORLD_BORDER_RIGHT_X ||
           obstacle->y > WORLD_BORDER_BOTTOM_Y || obstacle->y < WORLD_BORDER_TOP_Y) {
            //Destroy
            obstacle->visible = false;

            //Destruction post task
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_success);
            void (*task)(void) = obstacle->destructionTask;
            if(task != NULL) {
                task();
            }
        }

        //Player Collision
        if(fabsf(pos->x - obstacle->x) < (OBSTACLE_WIDTH + 5) &&
           fabsf(pos->y - obstacle->y) < OBSTACLE_WIDTH) {
            //Make invisible
            obstacle->visible = false;

            //Damage player
            damage_player(self);

            //Destruction post task
            void (*task)(void) = obstacle->destructionTask;
            if(task != NULL) {
                task();
            }
        } else {
            //Enemy collision: TODO Make this in an enemy update function
            for(int k = 0; k < MAX_ENEMIES; k++) {
                if(enemies[k].instance == NULL) continue;
                Vector enemyPos = entity_pos_get(enemies[k].instance);
                if(fabsf(enemyPos.x - obstacle->x) < (OBSTACLE_WIDTH + 5) &&
                   fabsf(enemyPos.y - obstacle->y) < OBSTACLE_WIDTH) {
                    //Make invisible
                    obstacle->visible = false;
                    //Damage enemy
                    damage_enemy(&enemies[k]);

                    //Destruction post task
                    void (*task)(void) = obstacle->destructionTask;
                    if(task != NULL) {
                        task();
                    }
                }
                break;
            }
        }
    }
}

void game_level_player_render(GameManager* manager, Canvas* canvas, void* context) {
    if(!game_menu_game_selected) return;
    UNUSED(manager);
    PlayerContext* playerCtx = context;
    UNUSED(playerCtx);

#ifdef MINIMAL_DEBUGGING
    canvas_printf(canvas, 30, 30, "Current Level: %d", playerLevel);
#endif

    if(!started) {
        started = true;
        welcomeTicks = furi_get_tick();
        enemy_spawn(gameLevel, manager, (Vector){120, 0}, 2000, false);
    }

    static uint32_t titleScreenHolder = 0;
    if(started && (furi_get_tick() - welcomeTicks) < 4000) {
        canvas_printf_blinking(
            canvas, 15, 37, 1500, 350, "This is DEADZONE...", &titleScreenHolder);
        //canvas_printf(canvas, 15, 37, "This is DEADZONE...");
    }

    bool renderedDoor = false;

    for(int i = 0; i < MAX_DOORS; i++) {
        struct game_door door = doors[i];
        if(!door.visible) {
            if((furi_get_tick() - door.transitionTicks) < door.transitionTime &&
               door.transitionText != NULL) {
                canvas_draw_str_aligned(
                    canvas, door.x, door.y, AlignCenter, AlignCenter, door.transitionText);
                playerCtx->sprite = playerCtx->sprite_forward;
            }
            continue;
        };
        renderedDoor = true;
        showBackground = false;
        canvas_draw_frame(canvas, door.x, door.y, door.width, door.height);
        canvas_draw_circle(canvas, door.x + 4, door.y + (door.height / 2.0f), 2);
        canvas_draw_line(
            canvas,
            door.x + 7,
            door.y + (door.height / 2.0f) + 1,
            door.x + (door.width / 2.0f),
            door.y + (door.height / 2.0f));
        canvas_draw_line(
            canvas,
            door.x + 7,
            door.y + (door.height / 2.0f) - 1,
            door.x + (door.width / 2.0f),
            door.y + (door.height / 2.0f));
    }

    if(!renderedDoor) {
        showBackground = true;
    }

    struct game_door* first_door = &doors[0];
    if(playerLevel == 2) {
        canvas_draw_str_aligned(
            canvas, first_door->x, first_door->y - 15, AlignCenter, AlignTop, "Next level...");
    }

    for(int i = 0; i < MAX_OBSTACLES; i++) {
        struct game_obstacle* obstacle = &obstacles[i];
        if(obstacle == NULL || !obstacle->visible) continue;
        canvas_draw_disc(canvas, obstacle->x, obstacle->y, obstacle->width);
        //canvas_draw_box(canvas, obstacle->x, obstacle->y, obstacle->width, obstacle->width);
    }

    if(totalObstacles >= 35 && playerLevel == 3) {
        bool allObstaclesGone = true;
        for(int i = 0; i < MAX_OBSTACLES; i++) {
            struct game_obstacle obstacle = obstacles[i];
            if(obstacle.visible) {
                allObstaclesGone = false;
                break;
            }
        }
        if(allObstaclesGone) {
            canvas_draw_str_aligned(
                canvas, 58, 8, AlignCenter, AlignTop, "Move onto the next level...");
            static bool secondLevelGateway = false;
            if(!secondLevelGateway) {
                doors[0] = (struct game_door){
                    80, 24, 25, 32, true, 0, "Level two!", 2000, postDoorEntryTask2};
                secondLevelGateway = true;
            }
        }
    }

    if(playerLevel == 2 && !startedLevelTwo) {
        startedLevelTwo = true;
    }
}

void game_level_enemy_render(GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(canvas);
    UNUSED(context);
}
