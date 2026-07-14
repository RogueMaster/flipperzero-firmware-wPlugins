#include "game.h"

/****** Entities: Player ******/

// Possible actions (for the ANN)
enum EnemyAction {
    EnemyActionAttack,
    EnemyActionRetreat,
    EnemyActionJump,
    EnemyActionStand
};

Enemy enemies[MAX_ENEMIES];

#define MAX_BULLETS 50
Entity* bullets[MAX_BULLETS];
bool bulletsDirection[MAX_BULLETS];

Entity* enemyBullets[MAX_BULLETS];
bool enemyBulletsDirection[MAX_BULLETS];

// Forward declaration of player_desc, because it's used in player_spawn function.

/* Begin of variables from header */

//Configurable values
//TODO Reloading with faster shooting.

#define SIZE_OF_WEIGHTS          362
#define NPC_ANN_BEHAVIOR_LATENCY 0
#ifdef DEBUGGING
#define NPC_ANN_WEIGHT_UNIQUENESS
#endif

#define TRAINING_RESET_VALUE 100
#ifdef DEBUGGING
uint32_t shootingDelay = 600;
#else
uint32_t shootingDelay = 450;
#endif
#ifdef DEBUGGING
uint32_t enemyShootingDelay = 600;
#else
uint32_t enemyShootingDelay = 600;
#endif
float bulletMoveSpeed = 0.7f;
float speed = 0.6f;
float enemySpeed = 0.19f;
float jumpHeight = 22.0F;
float jumpSpeed = 0.08f;
float enemyJumpHeight = 22.0F + 10.0F;
//Tracking player data
uint32_t kills = 0;
bool jumping = false;
float targetY = WORLD_BORDER_BOTTOM_Y;
float targetX = 10.0F;
bool showBackground = true;
int16_t transitionLeftTicks = 0;
int16_t transitionRightTicks = 0;
//Internal vars
int firstMobSpawnTicks = 0;
//While debugging we increase all lives for longer testing/gameplay.
#define LEARNING_RATE (double)0.0001

#ifdef DEBUGGING
bool canRespawn = true;
#else
bool canRespawn = false;
#endif
#ifdef DEBUGGING
int ENEMY_LIVES = 100;
int health = 200;
#else
int ENEMY_LIVES = STARTING_ENEMY_HEALTH;
int health = STARTING_PLAYER_HEALTH;
#endif
uint32_t firstKillTick = 0;
uint32_t secondKillTick = 0;
uint32_t gameBeginningTick = 0;
Level* gameLevel = NULL;
GameManager* globalGameManager = NULL;
Entity* globalPlayer = NULL;

/* End of header variables */

//Enemy, and bullet storage

//Internal variables
static const EntityDescription target_desc;
static const EntityDescription target_enemy_desc;

//More internal variables
uint32_t lastShotTick;
uint32_t lastBehaviorTick;

enum EnemyAction lastAction = EnemyActionStand;

struct BackgroundAsset {
    uint32_t posX;
    uint32_t posY;
    int type;
};

struct BackgroundAsset backgroundAssets[BACKGROUND_ASSET_COUNT];

float lerp(float y1, float y2, float t) {
    return y1 + t * (y2 - y1);
}

void nn_train(genann* ai, double* inputs, enum EnemyAction* desired_outputs, double learningRate) {
    //With the second output, we subtract 2, to keep the inputs in range of 0-1
    double outputs[2] = {desired_outputs[0], desired_outputs[1] - 2};
    genann_train(ai, inputs, outputs, learningRate);
}

static void frame_cb(GameEngine* engine, Canvas* canvas, InputState input, void* context) {
    UNUSED(engine);
    GameManager* game_manager = context;
    game_manager_input_set(game_manager, input);
    game_manager_update(game_manager);
    game_manager_render(game_manager, canvas);
}
/*
static Vector random_pos(void) {
    return (Vector){rand() % 120 + 4, rand() % 58 + 4};
}
*/

void featureCalculation(Entity* enemy, double* features) {
    //Pre processing
    Vector enemyPos = entity_pos_get(enemy);
    Vector closestBullet = (Vector){-1.0F, -1.0F};
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);
        float distXSq = (bulletPos.x - enemyPos.x) * (bulletPos.x - enemyPos.x);
        float distYSq = (bulletPos.y - enemyPos.y) * (bulletPos.y - enemyPos.y);
        float distance = sqrtf(distXSq + distYSq);

        float closestBulletDistX = (closestBullet.x - enemyPos.x) * (closestBullet.x - enemyPos.x);
        float closestBulletDistY = (closestBullet.y - enemyPos.y) * (closestBullet.y - enemyPos.y);
        float closestBulletDist = sqrtf(closestBulletDistX + closestBulletDistY);
        if(distance <= closestBulletDist) {
            closestBullet = bulletPos;
        }
    }

    float distToBullet = fabs(enemyPos.x - closestBullet.x);
    UNUSED(distToBullet);
    float distToPlayer;
    if(health > 0) {
        distToPlayer = fabs(entity_pos_get(globalPlayer).x - enemyPos.x);
    } else {
        distToPlayer = fabs((float)backgroundAssets[0].posX - enemyPos.x);
    }
    /*return (Vector){
        distToBullet < 25 && closestBullet.x != -1 ? 0.5 : -0.5,
        distToPlayer > 90 ? 0.5 :
        distToPlayer < 65 ? -0.5 :
                            0};*/
    features[0] = closestBullet.x;
    features[1] = closestBullet.y;
    features[2] = enemyPos.x;
    features[3] = enemyPos.y;
    features[4] = distToPlayer;
}

void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);
    globalPlayer = player;

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, (Vector){10, 32});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 16x16
    entity_collider_add_rect(player, 16, 16);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite_right = game_manager_sprite_load(manager, "player_right.fxbm");
    player_context->sprite_right_shadowed =
        game_manager_sprite_load(manager, "player_right_shadow.fxbm");
    player_context->sprite_right_recoil =
        game_manager_sprite_load(manager, "player_right_recoil.fxbm");

    player_context->sprite_left = game_manager_sprite_load(manager, "player_left.fxbm");
    player_context->sprite_left_shadowed =
        game_manager_sprite_load(manager, "player_left_shadow.fxbm");
    player_context->sprite_left_recoil =
        game_manager_sprite_load(manager, "player_left_recoil.fxbm");
    player_context->sprite_jump = game_manager_sprite_load(manager, "player_jump.fxbm");
    player_context->sprite_stand = game_manager_sprite_load(manager, "player_stand.fxbm");
    player_context->sprite_forward = game_manager_sprite_load(manager, "player_forward.fxbm");

    player_context->sprite = player_context->sprite_right;

    gameBeginningTick = furi_get_tick();
}

int npcAIModelIndex = -1;

void _enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right,
    int startingLives) {
    int enemyIndex = -1;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != NULL) continue;
        enemyIndex = i;
        enemies[i].instance = level_add_entity(level, &enemy_desc);
        enemies[i].direction = right;
        enemies[i].jumping = false;
        enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        enemies[i].lives = startingLives;
        enemies[i].spawnTime = furi_get_tick();
        enemies[i].mercyTicks = mercyTicks;
        enemies[i].lastShot = furi_get_tick() + 2000;

        enemies[i].ai = genann_init(5, 2, 15, 2);

#ifdef NPC_ANN_WEIGHT_UNIQUENESS
        double randValue = (double)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;
        //double randValue2 = (double)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;

        if(npcAIModelIndex == -1) {
            //Start off with random model, then always change, so that two following NPCs are never the same.
            npcAIModelIndex = (randValue > (double)0.7) ? 0 : 2;
        } else {
            npcAIModelIndex++;
        }

        for(int j = 0; j < enemies[i].ai->total_weights; j++) {
            if(npcAIModelIndex % 3 == 0) {
                static double bestWeights[SIZE_OF_WEIGHTS] = WEIGHTS_FOR_BEST_NPC;
                enemies[i].ai->weight[j] = bestWeights[j];
            } else if(npcAIModelIndex % 3 == 1) {
                static double closeUpNPCWeights[SIZE_OF_WEIGHTS] = WEIGHTS_FOR_CLOSEUP_NPC;
                enemies[i].ai->weight[j] = closeUpNPCWeights[j];
            } else {
                static double nonJumpingNPCWeights[SIZE_OF_WEIGHTS] = WEIGHTS_FOR_NONJUMPING_NPC;
                enemies[i].ai->weight[j] = nonJumpingNPCWeights[j];
            }
        }
#else
        double weights[SIZE_OF_WEIGHTS] = WEIGHTS_FOR_BEST_NPC;

        for(int j = 0; j < enemies[i].ai->total_weights; j++) {
            enemies[i].ai->weight[j] = weights[j];
        }

#endif

        FURI_LOG_I("DEADZONE", "Loaded the weights!");
        break;
    }

    // Too many enemies
    if(enemyIndex == -1) return;

    // Set enemy position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(enemies[enemyIndex].instance, spawnPosition);

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 16x16
    entity_collider_add_rect(enemies[enemyIndex].instance, 16, 16);

    // Get enemy context
    PlayerContext* player_context = entity_context_get(enemies[enemyIndex].instance);

    // Load enemy sprite
    player_context->sprite_left = game_manager_sprite_load(manager, "enemy_left.fxbm");
    player_context->sprite_right = game_manager_sprite_load(manager, "enemy_right.fxbm");
    if(right) {
        player_context->sprite = player_context->sprite_right;
    } else {
        player_context->sprite = player_context->sprite_left;
    }
}

void enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right) {
    _enemy_spawn(level, manager, spawnPosition, mercyTicks, right, ENEMY_LIVES);
}

void player_jump_handler(PlayerContext* playerCtx, Vector* pos, InputState* input) {
    Sprite* jumpingSprite = playerCtx->sprite_jump;
    Sprite* standingSprite = playerCtx->sprite_stand;

    //Initiate jump process (first jump) if we are on ground (and are not jumping)
    if(input->held & GameKeyUp && !jumping && roundf(pos->y) == WORLD_BORDER_BOTTOM_Y) {
        //Start jumping
        jumping = true;

        //Set target Y (based on maximum jump height)
        targetY = CLAMP(pos->y - jumpHeight, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

        //Set player sprite to jumping
        //playerCtx->sprite = jumpingSprite;
        return;
    }

    //Interpolate to the target jump position
    pos->y = lerp(pos->y, targetY, jumpSpeed);

    //Continuation of jump (until threshold is reached)
    if(jumping) {
        //We've reached our threshold
        if(roundf(pos->y) == roundf(targetY)) {
            //Reset threshold and jumping state
            targetY = WORLD_BORDER_BOTTOM_Y;
            jumping = false;
        }
    }

    if(roundf(pos->y) == WORLD_BORDER_BOTTOM_Y && !jumping && playerCtx->sprite == jumpingSprite) {
        playerCtx->sprite = standingSprite;
    }
}

void player_shoot_handler(PlayerContext* playerCtx, InputState* input, Vector* pos) {
    UNUSED(input);
    //If we are not facing right or left, we cannot shoot
    bool canShoot = playerCtx->sprite != playerCtx->sprite_jump &&
                    playerCtx->sprite != playerCtx->sprite_stand &&
                    playerCtx->sprite != playerCtx->sprite_left_recoil &&
                    playerCtx->sprite != playerCtx->sprite_right_recoil &&
                    playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                    playerCtx->sprite != playerCtx->sprite_right_shadowed &&
                    playerCtx->sprite !=
                        playerCtx->sprite_forward; //TODO For now, can't shoot forward

    uint32_t currentTick = furi_get_tick();
    //Shooting action
    if((input->held & GameKeyOk) && (currentTick - lastShotTick >= shootingDelay) && canShoot) {
        lastShotTick = currentTick;
        //Spawn bullet
        int bulletIndex = -1;
        //Find first empty bullet
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(bullets[i] != NULL) continue;
            bulletIndex = i;
            break;
        }

        //Too many bullets in the scene.
        if(bulletIndex == -1) return;

        bullets[bulletIndex] = level_add_entity(gameLevel, &target_desc);
        bulletsDirection[bulletIndex] = playerCtx->sprite == playerCtx->sprite_right ||
                                        playerCtx->sprite == playerCtx->sprite_right_recoil ||
                                        playerCtx->sprite == playerCtx->sprite_right_shadowed;
        float deltaX = bulletsDirection[bulletIndex] ? 10 : -10;
        // Set target position
        Vector bulletPos = (Vector){pos->x + deltaX, pos->y};
        entity_pos_set(bullets[bulletIndex], bulletPos);

        //Handle recoil
        float recoil = 0.0f;
        if(playerCtx->sprite == playerCtx->sprite_right) {
            playerCtx->sprite = playerCtx->sprite_right_recoil;
            recoil -= 7;
        } else if(playerCtx->sprite == playerCtx->sprite_left) {
            playerCtx->sprite = playerCtx->sprite_left_recoil;
            recoil += 7;
        }
        pos->x += recoil;
        targetX += recoil;
    }

    if(furi_get_tick() - lastShotTick > 300) {
        if(playerCtx->sprite == playerCtx->sprite_right_recoil) {
            playerCtx->sprite = playerCtx->sprite_right;
        } else if(playerCtx->sprite == playerCtx->sprite_left_recoil) {
            playerCtx->sprite = playerCtx->sprite_left;
        }
    }
}

void enemy_shoot_handler(Enemy* enemy, Vector* pos, uint32_t* enemyLastShotTick, void* context) {
    PlayerContext* enemyCtx = context;
    uint32_t currentTick = furi_get_tick();

    bool gracePeriod = furi_get_tick() < (enemy->spawnTime + enemy->mercyTicks);
    float shootingRateFactor = gracePeriod ? 45.0F : 1;
    //Shooting action
    if(currentTick - *enemyLastShotTick >= (enemyShootingDelay * (shootingRateFactor)) &&
       health != 0) {
        *enemyLastShotTick = currentTick;
        //Spawn bullet
        int bulletIndex = -1;
        //Find first empty bullet
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(enemyBullets[i] != NULL) continue;
            bulletIndex = i;
            break;
        }

        //Too many bullets in the scene.
        if(bulletIndex == -1) return;

        enemyBullets[bulletIndex] = level_add_entity(gameLevel, &target_enemy_desc);
        // Set target position
        Vector bulletPos;
        enemyBulletsDirection[bulletIndex] = enemyCtx->sprite == enemyCtx->sprite_right;
        if(enemyBulletsDirection[bulletIndex]) {
            bulletPos = (Vector){pos->x + 10, pos->y};
        } else {
            bulletPos = (Vector){pos->x - 10, pos->y};
        }
        entity_pos_set(enemyBullets[bulletIndex], bulletPos);
    }
}

uint32_t lastSwitchRight = 0;

bool damage_enemy(Enemy* enemy) {
    const NotificationSequence* damageSound = &sequence_semi_success;

    if(--enemy->lives == 0) {
        //Ran out of lives
        level_remove_entity(gameLevel, enemy->instance);

#ifdef MINIMAL_DEBUGGING
        //Print weights
        FURI_LOG_I("DEADZONE", "Finished randomness tweaking of weights:");
        for(int j = 0; j < enemy->ai->total_weights; j++) {
            FURI_LOG_I("DEADZONE", "Weights: %f", enemy->ai->weight[j]);
        }
#endif

        genann_free(enemy->lastAI);
        genann_free(enemy->ai);
        //Replace damage sound with death sound
        damageSound = &sequence_success;

        enemy->instance = NULL;
        kills++;
        if(kills == 1) {
            firstKillTick = furi_get_tick();
        }
    }

    //Play sound of getting hit
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, damageSound);
    return enemy->lives == 0;
}

bool damage_player(Entity* self) {
    const NotificationSequence* damageSound = &sequence_single_vibro;

    if(--health == 0) {
        backgroundAssets[0] =
            (struct BackgroundAsset){entity_pos_get(self).x, WORLD_BORDER_BOTTOM_Y - 6, 1};

        //Ran out of lives
        level_remove_entity(gameLevel, self);

        //Replace damage sound with death sound
        damageSound = &sequence_double_vibro;

        /*
                //Destroy all associated bullets
                for(int i = 0; i < MAX_BULLETS; i++) {
                    if(bullets[i] == NULL) continue;
                    level_remove_entity(gameLevel, bullets[i]);
                    bullets[i] = NULL;
                }*/
    }

    //Play sound of getting hit
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, damageSound);
    notification_message(notifications, &sequence_blink_red_100);
    return health == 0;
}

#ifdef DEBUGGING
int trainCount = 0;
#else
int trainCount = 1000;
#endif

#define IS_TRAINING               trainCount < TRAINING_RESET_VALUE
#define DONE_TRAINING             trainCount == TRAINING_RESET_VALUE
#define RECENTLY_STARTED_TRAINING trainCount == 1

double err = 100;
double recentErr = 0;

void player_update(Entity* self, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);

    PlayerContext* playerCtx = context;

    //Player bullet collision
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(enemyBullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(enemyBullets[i]);
        float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
        float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
        float distance = sqrtf(distXSq + distYSq);
        if(distance < BULLET_DISTANCE_THRESHOLD ||
           (playerCtx->sprite == playerCtx->sprite_jump && sqrtf(distXSq) < 6 &&
            (distYSq > 0 ? distYSq < 8 : sqrtf(distYSq) < 7))) {
            //Self destruction of bullet and potentially player
            level_remove_entity(gameLevel, enemyBullets[i]);
            enemyBullets[i] = NULL;
            damage_player(self);
        }
    }

    game_level_player_update(self, manager, playerCtx, &pos);
    tutorial_level_player_update(self, manager, playerCtx, &pos);

    //Movement - Game position starts at TOP LEFT.
    //The higher the Y, the lower we move on the screen
    //The higher the X, the more we move toward the right side.
    //Jump Mechanics
    player_jump_handler(playerCtx, &pos, &input);

    //Player Shooting Mechanics
    player_shoot_handler(playerCtx, &input, &pos);

    if(input.held & GameKeyLeft && playerCtx->sprite != playerCtx->sprite_left_recoil) {
        if(playerCtx->sprite == playerCtx->sprite_right_shadowed) {
            transitionLeftTicks = TRANSITION_FRAMES;
        }
        //Switch sprite to left direction
        if((playerCtx->sprite != playerCtx->sprite_left_shadowed &&
            playerCtx->sprite != playerCtx->sprite_right_shadowed &&
            playerCtx->sprite != playerCtx->sprite_left)) {
            if(playerCtx->sprite == playerCtx->sprite_stand) {
                //transitionLeftTicks++;
            }
            if(transitionLeftTicks < TRANSITION_FRAMES) {
                transitionLeftTicks++;
                if(playerCtx->sprite != playerCtx->sprite_stand &&
                   playerCtx->sprite != playerCtx->sprite_jump) {
                    playerCtx->sprite = playerCtx->sprite_forward;
                }
            } else if(
                playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                transitionLeftTicks >= TRANSITION_FRAMES) {
                transitionLeftTicks = 0;
                if(playerCtx->sprite == playerCtx->sprite_right_shadowed) {
                    playerCtx->sprite = playerCtx->sprite_left_shadowed;
                } else {
                    playerCtx->sprite = playerCtx->sprite_left;
                }
            }
        }

        if(playerCtx->sprite == playerCtx->sprite_left_shadowed ||
           playerCtx->sprite == playerCtx->sprite_left) {
            targetX -= speed;
            targetX = CLAMP(targetX, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
            pos.x = CLAMP(pos.x - speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
        }
    }

    if(input.held & GameKeyRight && playerCtx->sprite != playerCtx->sprite_right_recoil) {
        //Switch sprite to left direction

        if(playerCtx->sprite == playerCtx->sprite_left_shadowed) {
            transitionRightTicks = TRANSITION_FRAMES;
        }
        if(playerCtx->sprite != playerCtx->sprite_right_shadowed &&
           playerCtx->sprite != playerCtx->sprite_right) {
            if(playerCtx->sprite == playerCtx->sprite_forward) {
                //transitionRightTicks++;
            }
            if(transitionRightTicks < TRANSITION_FRAMES) {
                transitionRightTicks++;
                if(playerCtx->sprite != playerCtx->sprite_forward &&
                   playerCtx->sprite != playerCtx->sprite_jump) {
                    playerCtx->sprite = playerCtx->sprite_stand;
                }
            } else if(
                playerCtx->sprite != playerCtx->sprite_right_shadowed &&
                transitionRightTicks >= TRANSITION_FRAMES) {
                transitionRightTicks = 0;
                if(playerCtx->sprite == playerCtx->sprite_left_shadowed) {
                    playerCtx->sprite = playerCtx->sprite_right_shadowed;
                } else {
                    playerCtx->sprite = playerCtx->sprite_right;
                }
            }
        }

        //Switch to right direction
        if(playerCtx->sprite == playerCtx->sprite_right_shadowed ||
           playerCtx->sprite == playerCtx->sprite_right) {
            targetX += speed;
            targetX = CLAMP(targetX, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
            pos.x = CLAMP(pos.x + speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
        }
    }

    bool canPressDown = playerCtx->sprite != playerCtx->sprite_left_recoil &&
                        playerCtx->sprite != playerCtx->sprite_right_recoil;

    if(input.pressed & GameKeyDown && canPressDown) {
#ifdef DEBUGGING
        if(IS_TRAINING) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;
                double features[5];
                featureCalculation(enemies[i].instance, features);
                enum EnemyAction outputs[2] = {EnemyActionRetreat, EnemyActionStand};
                nn_train(enemies[i].ai, features, outputs, LEARNING_RATE);
                trainCount++;
                break;
            }
        }
#else
        playerCtx->sprite = playerCtx->sprite_forward;
#endif
    }

    // Clamp player position to screen bounds
    pos.x = CLAMP(lerp(pos.x, targetX, jumpSpeed), WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
    pos.y = CLAMP(pos.y, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

    //Screen space begins at top left corner.
    //Y increases as we go down.
    //X increases as we go right

    // Set new player position
    entity_pos_set(self, pos);

    // Control game exit

    //When the Back key is pressed
    if(input.pressed & GameKeyBack) {
#ifdef DEBUGGING
        if(IS_TRAINING) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;
                double features[5];
                featureCalculation(enemies[i].instance, features);
                enum EnemyAction outputs[2] = {EnemyActionRetreat, EnemyActionJump};
                nn_train(enemies[i].ai, features, outputs, LEARNING_RATE);
                break;
            }
        }
#else
        //Only reaches if player is alive. Send them to the main menu
        game_manager_game_stop(manager);
#endif
    }
}

const char* int_to_string(int value) {
    static char buffer[12]; // Enough to hold any 32-bit int
    int i = 0, is_negative = 0;

    if(value < 0) {
        is_negative = 1;
        value = -value;
    }

    // Convert digits to string (in reverse order)
    do {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    } while(value > 0);

    if(is_negative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    // Reverse the string
    for(int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buffer[j];
        buffer[j] = buffer[k];
        buffer[k] = temp;
    }

    return buffer;
}

int successfulJumpCycles = 0;

void global_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(context);

    //Check player input when they're dead, since enemy is alive, processing must proceed here.
    InputState input = game_manager_input_get(manager);
    if(input.pressed & GameKeyBack) {
        if(health <= 0) {
            if(game_menu_tutorial_selected) {
                //If dead, restart game during tutorial
                hideBackgroundAssets();
                health = STARTING_PLAYER_HEALTH;
                player_spawn(gameLevel, manager);
                entity_pos_set(globalPlayer, (Vector){WORLD_BORDER_LEFT_X, WORLD_BORDER_BOTTOM_Y});
            } else {
                //End game
                game_manager_game_stop(manager);
            }
        }
    }

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance == NULL || globalPlayer == NULL) continue;
        Vector playerPos = entity_pos_get(globalPlayer);
        Vector enemyPos = entity_pos_get(enemies[i].instance);

        if(playerPos.x - enemyPos.x > 0) {
            enemies[i].direction = true;
        } else {
            enemies[i].direction = false;
        }
    }

    //Player bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);

        float deltaX = bulletsDirection[i] ? bulletMoveSpeed : -bulletMoveSpeed;
        bulletPos.x =
            CLAMP(bulletPos.x + deltaX, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        //bulletPos.y += 0.07;
        entity_pos_set(bullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, bullets[i]);
            bullets[i] = NULL;
#ifdef DEBUGGING
            if(successfulJumpCycles > 7 && trainCount != 1000) {
                //STOP training
                trainCount = 1000;
                FURI_LOG_I("DEADZONE", "Finished training! Weights:");
                for(int i = 0; i < enemies[0].ai->total_weights; i++) {
                    FURI_LOG_I("DEADZONE", "Weights: %f", enemies[0].ai->weight[i]);
                }
            } else {
                recentErr -= 20;
                successfulJumpCycles++;
            }
#endif
        }
    }

    //Enemy bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(enemyBullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(enemyBullets[i]);
        //TODO Currently movement is dependent on enemy's trajectory, no metadata of bullet.
        float deltaX = enemyBulletsDirection[i] ? bulletMoveSpeed : -bulletMoveSpeed;
        bulletPos.x =
            CLAMP(bulletPos.x + deltaX, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        entity_pos_set(enemyBullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, enemyBullets[i]);
            enemyBullets[i] = NULL;
        }
    }
}

void hideBackgroundAssets() {
    for(uint32_t i = 0; i < BACKGROUND_ASSET_COUNT; i++) {
        backgroundAssets[i].type = -1;
    }
}

void computeBackgroundAssets() {
    int counter = 0;
    uint32_t y = WORLD_BORDER_TOP_Y + 5;
    for(uint32_t k = 0; k < BACKGROUND_ASSET_ROWS; k++) {
        double randValue = (double)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;
        uint32_t x = 10;
        for(int i = 0; i < (BACKGROUND_ASSET_COUNT / BACKGROUND_ASSET_ROWS); i++) {
            randValue = (double)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;
            randValue = (double)furi_hal_random_get() / FURI_HAL_RANDOM_MAX;
            backgroundAssets[counter++] =
                (struct BackgroundAsset){x, y, randValue > (double)0.8 ? 0 : 1};
            x += 30 + randValue * 40;
        }
        y += 15 + randValue * 20;
        y = MIN(y, (uint32_t)WORLD_BORDER_BOTTOM_Y - 20);
    }
}

void canvas_printf_blinking(
    Canvas* canvas,
    uint32_t x,
    uint32_t y,
    uint32_t shownTicks,
    uint32_t hiddenTicks,
    const char* format,
    uint32_t* dataHolder) {
    if(furi_get_tick() - *dataHolder < shownTicks) {
        canvas_printf(canvas, x, y, format);
    } else if(furi_get_tick() - *dataHolder > (shownTicks + hiddenTicks)) {
        *dataHolder = furi_get_tick();
    }
}

void renderSceneBackground(Canvas* canvas) {
    if(!showBackground) return;
    static int pixelated[996][2] = {
        {76, 10},  {77, 10},  {78, 10},  {79, 10},  {95, 10},  {96, 10},  {97, 10},  {109, 10},
        {35, 11},  {36, 11},  {37, 11},  {38, 11},  {39, 11},  {40, 11},  {41, 11},  {42, 11},
        {74, 11},  {75, 11},  {76, 11},  {79, 11},  {97, 11},  {98, 11},  {108, 11}, {109, 11},
        {33, 12},  {34, 12},  {35, 12},  {42, 12},  {43, 12},  {64, 12},  {65, 12},  {66, 12},
        {67, 12},  {68, 12},  {69, 12},  {70, 12},  {72, 12},  {73, 12},  {74, 12},  {78, 12},
        {79, 12},  {98, 12},  {99, 12},  {102, 12}, {103, 12}, {104, 12}, {105, 12}, {108, 12},
        {33, 13},  {36, 13},  {37, 13},  {38, 13},  {40, 13},  {41, 13},  {43, 13},  {44, 13},
        {63, 13},  {64, 13},  {70, 13},  {71, 13},  {72, 13},  {78, 13},  {83, 13},  {84, 13},
        {87, 13},  {88, 13},  {101, 13}, {102, 13}, {103, 13}, {105, 13}, {106, 13}, {107, 13},
        {112, 13}, {33, 14},  {36, 14},  {37, 14},  {38, 14},  {40, 14},  {41, 14},  {44, 14},
        {63, 14},  {68, 14},  {71, 14},  {78, 14},  {100, 14}, {101, 14}, {107, 14}, {108, 14},
        {112, 14}, {33, 15},  {35, 15},  {36, 15},  {37, 15},  {41, 15},  {42, 15},  {43, 15},
        {44, 15},  {63, 15},  {64, 15},  {68, 15},  {69, 15},  {71, 15},  {72, 15},  {74, 15},
        {75, 15},  {78, 15},  {82, 15},  {84, 15},  {85, 15},  {86, 15},  {96, 15},  {97, 15},
        {99, 15},  {100, 15}, {108, 15}, {109, 15}, {112, 15}, {33, 16},  {34, 16},  {38, 16},
        {39, 16},  {42, 16},  {43, 16},  {63, 16},  {64, 16},  {65, 16},  {66, 16},  {67, 16},
        {68, 16},  {69, 16},  {70, 16},  {71, 16},  {72, 16},  {73, 16},  {74, 16},  {75, 16},
        {76, 16},  {77, 16},  {78, 16},  {93, 16},  {94, 16},  {95, 16},  {96, 16},  {99, 16},
        {109, 16}, {112, 16}, {34, 17},  {35, 17},  {37, 17},  {38, 17},  {41, 17},  {42, 17},
        {69, 17},  {70, 17},  {72, 17},  {93, 17},  {94, 17},  {99, 17},  {109, 17}, {35, 18},
        {40, 18},  {41, 18},  {70, 18},  {71, 18},  {72, 18},  {99, 18},  {109, 18}, {35, 19},
        {39, 19},  {40, 19},  {71, 19},  {72, 19},  {99, 19},  {108, 19}, {109, 19}, {35, 20},
        {36, 20},  {39, 20},  {99, 20},  {100, 20}, {108, 20}, {110, 20}, {111, 20}, {112, 20},
        {113, 20}, {114, 20}, {126, 20}, {127, 20}, {35, 21},  {36, 21},  {37, 21},  {38, 21},
        {39, 21},  {54, 21},  {55, 21},  {56, 21},  {100, 21}, {101, 21}, {102, 21}, {103, 21},
        {106, 21}, {107, 21}, {125, 21}, {126, 21}, {20, 22},  {22, 22},  {35, 22},  {37, 22},
        {39, 22},  {40, 22},  {53, 22},  {54, 22},  {56, 22},  {57, 22},  {100, 22}, {103, 22},
        {104, 22}, {105, 22}, {106, 22}, {124, 22}, {125, 22}, {18, 23},  {34, 23},  {35, 23},
        {36, 23},  {37, 23},  {40, 23},  {41, 23},  {42, 23},  {52, 23},  {53, 23},  {57, 23},
        {58, 23},  {100, 23}, {112, 23}, {113, 23}, {114, 23}, {115, 23}, {123, 23}, {124, 23},
        {33, 24},  {34, 24},  {36, 24},  {39, 24},  {40, 24},  {42, 24},  {52, 24},  {58, 24},
        {59, 24},  {99, 24},  {100, 24}, {112, 24}, {113, 24}, {114, 24}, {115, 24}, {116, 24},
        {17, 25},  {30, 25},  {31, 25},  {33, 25},  {36, 25},  {38, 25},  {39, 25},  {42, 25},
        {51, 25},  {52, 25},  {59, 25},  {60, 25},  {99, 25},  {105, 25}, {106, 25}, {112, 25},
        {113, 25}, {114, 25}, {115, 25}, {116, 25}, {117, 25}, {125, 25}, {126, 25}, {127, 25},
        {28, 26},  {29, 26},  {30, 26},  {31, 26},  {32, 26},  {33, 26},  {36, 26},  {37, 26},
        {38, 26},  {42, 26},  {50, 26},  {51, 26},  {60, 26},  {61, 26},  {99, 26},  {106, 26},
        {112, 26}, {115, 26}, {116, 26}, {117, 26}, {125, 26}, {126, 26}, {16, 27},  {17, 27},
        {28, 27},  {29, 27},  {30, 27},  {32, 27},  {33, 27},  {36, 27},  {37, 27},  {42, 27},
        {50, 27},  {61, 27},  {112, 27}, {117, 27}, {16, 28},  {17, 28},  {28, 28},  {33, 28},
        {34, 28},  {35, 28},  {36, 28},  {39, 28},  {40, 28},  {41, 28},  {42, 28},  {50, 28},
        {52, 28},  {53, 28},  {54, 28},  {58, 28},  {59, 28},  {61, 28},  {112, 28}, {117, 28},
        {120, 28}, {121, 28}, {123, 28}, {124, 28}, {125, 28}, {126, 28}, {127, 28}, {16, 29},
        {17, 29},  {28, 29},  {34, 29},  {35, 29},  {37, 29},  {38, 29},  {39, 29},  {50, 29},
        {52, 29},  {53, 29},  {54, 29},  {58, 29},  {59, 29},  {61, 29},  {93, 29},  {94, 29},
        {95, 29},  {96, 29},  {97, 29},  {112, 29}, {117, 29}, {118, 29}, {119, 29}, {120, 29},
        {121, 29}, {122, 29}, {13, 30},  {14, 30},  {15, 30},  {16, 30},  {17, 30},  {28, 30},
        {29, 30},  {30, 30},  {31, 30},  {35, 30},  {36, 30},  {37, 30},  {50, 30},  {52, 30},
        {53, 30},  {54, 30},  {58, 30},  {59, 30},  {61, 30},  {92, 30},  {97, 30},  {98, 30},
        {99, 30},  {112, 30}, {117, 30}, {121, 30}, {122, 30}, {11, 31},  {12, 31},  {18, 31},
        {19, 31},  {31, 31},  {32, 31},  {35, 31},  {36, 31},  {37, 31},  {50, 31},  {61, 31},
        {91, 31},  {92, 31},  {94, 31},  {95, 31},  {96, 31},  {99, 31},  {100, 31}, {111, 31},
        {112, 31}, {117, 31}, {121, 31}, {122, 31}, {10, 32},  {11, 32},  {19, 32},  {20, 32},
        {31, 32},  {32, 32},  {33, 32},  {37, 32},  {38, 32},  {39, 32},  {40, 32},  {50, 32},
        {61, 32},  {75, 32},  {76, 32},  {77, 32},  {91, 32},  {100, 32}, {101, 32}, {102, 32},
        {103, 32}, {110, 32}, {111, 32}, {112, 32}, {117, 32}, {119, 32}, {120, 32}, {121, 32},
        {9, 33},   {21, 33},  {29, 33},  {30, 33},  {31, 33},  {33, 33},  {34, 33},  {35, 33},
        {40, 33},  {41, 33},  {50, 33},  {61, 33},  {74, 33},  {76, 33},  {78, 33},  {90, 33},
        {91, 33},  {93, 33},  {95, 33},  {96, 33},  {101, 33}, {102, 33}, {103, 33}, {104, 33},
        {105, 33}, {106, 33}, {107, 33}, {108, 33}, {109, 33}, {110, 33}, {111, 33}, {112, 33},
        {113, 33}, {114, 33}, {115, 33}, {116, 33}, {117, 33}, {118, 33}, {119, 33}, {120, 33},
        {121, 33}, {122, 33}, {9, 34},   {21, 34},  {28, 34},  {29, 34},  {33, 34},  {34, 34},
        {35, 34},  {41, 34},  {50, 34},  {61, 34},  {75, 34},  {76, 34},  {77, 34},  {90, 34},
        {93, 34},  {94, 34},  {95, 34},  {96, 34},  {101, 34}, {102, 34}, {103, 34}, {106, 34},
        {111, 34}, {112, 34}, {117, 34}, {9, 35},   {13, 35},  {14, 35},  {15, 35},  {16, 35},
        {21, 35},  {28, 35},  {32, 35},  {33, 35},  {35, 35},  {36, 35},  {40, 35},  {41, 35},
        {50, 35},  {61, 35},  {76, 35},  {89, 35},  {90, 35},  {93, 35},  {94, 35},  {95, 35},
        {96, 35},  {106, 35}, {111, 35}, {117, 35}, {9, 36},   {13, 36},  {14, 36},  {15, 36},
        {16, 36},  {21, 36},  {27, 36},  {28, 36},  {32, 36},  {35, 36},  {36, 36},  {37, 36},
        {40, 36},  {50, 36},  {55, 36},  {56, 36},  {57, 36},  {61, 36},  {75, 36},  {76, 36},
        {77, 36},  {89, 36},  {97, 36},  {98, 36},  {99, 36},  {100, 36}, {101, 36}, {102, 36},
        {103, 36}, {104, 36}, {105, 36}, {106, 36}, {107, 36}, {9, 37},   {13, 37},  {16, 37},
        {21, 37},  {27, 37},  {32, 37},  {34, 37},  {35, 37},  {36, 37},  {37, 37},  {38, 37},
        {39, 37},  {40, 37},  {50, 37},  {55, 37},  {56, 37},  {57, 37},  {61, 37},  {66, 37},
        {67, 37},  {68, 37},  {74, 37},  {76, 37},  {78, 37},  {89, 37},  {90, 37},  {95, 37},
        {96, 37},  {97, 37},  {106, 37}, {107, 37}, {9, 38},   {10, 38},  {11, 38},  {12, 38},
        {13, 38},  {14, 38},  {15, 38},  {16, 38},  {17, 38},  {18, 38},  {19, 38},  {20, 38},
        {21, 38},  {27, 38},  {28, 38},  {31, 38},  {32, 38},  {33, 38},  {34, 38},  {35, 38},
        {36, 38},  {37, 38},  {50, 38},  {55, 38},  {56, 38},  {57, 38},  {61, 38},  {64, 38},
        {65, 38},  {66, 38},  {67, 38},  {68, 38},  {69, 38},  {76, 38},  {90, 38},  {91, 38},
        {92, 38},  {93, 38},  {94, 38},  {95, 38},  {107, 38}, {108, 38}, {109, 38}, {110, 38},
        {111, 38}, {112, 38}, {113, 38}, {114, 38}, {115, 38}, {116, 38}, {117, 38}, {118, 38},
        {119, 38}, {120, 38}, {121, 38}, {122, 38}, {28, 39},  {31, 39},  {32, 39},  {35, 39},
        {37, 39},  {38, 39},  {50, 39},  {51, 39},  {52, 39},  {53, 39},  {54, 39},  {55, 39},
        {56, 39},  {57, 39},  {58, 39},  {59, 39},  {60, 39},  {61, 39},  {64, 39},  {65, 39},
        {66, 39},  {67, 39},  {68, 39},  {69, 39},  {75, 39},  {77, 39},  {28, 40},  {29, 40},
        {30, 40},  {31, 40},  {35, 40},  {38, 40},  {50, 40},  {51, 40},  {52, 40},  {53, 40},
        {54, 40},  {55, 40},  {56, 40},  {57, 40},  {58, 40},  {59, 40},  {60, 40},  {61, 40},
        {65, 40},  {68, 40},  {29, 41},  {30, 41},  {31, 41},  {34, 41},  {35, 41},  {36, 41},
        {37, 41},  {38, 41},  {39, 41},  {29, 42},  {33, 42},  {34, 42},  {35, 42},  {39, 42},
        {29, 43},  {32, 43},  {33, 43},  {35, 43},  {36, 43},  {39, 43},  {40, 43},  {29, 44},
        {30, 44},  {31, 44},  {32, 44},  {36, 44},  {40, 44},  {30, 45},  {31, 45},  {36, 45},
        {37, 45},  {38, 45},  {39, 45},  {40, 45},  {0, 62},   {1, 62},   {2, 62},   {3, 62},
        {4, 62},   {5, 62},   {6, 62},   {7, 62},   {8, 62},   {9, 62},   {10, 62},  {11, 62},
        {12, 62},  {13, 62},  {14, 62},  {15, 62},  {16, 62},  {17, 62},  {18, 62},  {19, 62},
        {20, 62},  {21, 62},  {22, 62},  {23, 62},  {24, 62},  {25, 62},  {26, 62},  {27, 62},
        {28, 62},  {29, 62},  {30, 62},  {31, 62},  {32, 62},  {33, 62},  {34, 62},  {35, 62},
        {36, 62},  {37, 62},  {38, 62},  {39, 62},  {40, 62},  {41, 62},  {42, 62},  {43, 62},
        {44, 62},  {45, 62},  {46, 62},  {47, 62},  {48, 62},  {49, 62},  {50, 62},  {51, 62},
        {52, 62},  {53, 62},  {54, 62},  {55, 62},  {56, 62},  {57, 62},  {58, 62},  {59, 62},
        {60, 62},  {61, 62},  {62, 62},  {63, 62},  {64, 62},  {65, 62},  {66, 62},  {67, 62},
        {68, 62},  {69, 62},  {70, 62},  {71, 62},  {72, 62},  {73, 62},  {74, 62},  {75, 62},
        {76, 62},  {77, 62},  {78, 62},  {79, 62},  {80, 62},  {81, 62},  {82, 62},  {83, 62},
        {84, 62},  {85, 62},  {86, 62},  {87, 62},  {88, 62},  {89, 62},  {90, 62},  {91, 62},
        {92, 62},  {93, 62},  {94, 62},  {95, 62},  {96, 62},  {97, 62},  {98, 62},  {99, 62},
        {100, 62}, {101, 62}, {102, 62}, {103, 62}, {104, 62}, {105, 62}, {106, 62}, {107, 62},
        {108, 62}, {109, 62}, {110, 62}, {111, 62}, {112, 62}, {113, 62}, {114, 62}, {115, 62},
        {116, 62}, {117, 62}, {118, 62}, {119, 62}, {120, 62}, {121, 62}, {122, 62}, {123, 62},
        {124, 62}, {125, 62}, {126, 62}, {127, 62}, {0, 63},   {1, 63},   {2, 63},   {3, 63},
        {4, 63},   {5, 63},   {6, 63},   {7, 63},   {8, 63},   {9, 63},   {10, 63},  {11, 63},
        {12, 63},  {13, 63},  {14, 63},  {15, 63},  {16, 63},  {17, 63},  {18, 63},  {19, 63},
        {20, 63},  {21, 63},  {22, 63},  {23, 63},  {24, 63},  {25, 63},  {26, 63},  {27, 63},
        {28, 63},  {29, 63},  {30, 63},  {31, 63},  {32, 63},  {33, 63},  {34, 63},  {35, 63},
        {36, 63},  {37, 63},  {38, 63},  {39, 63},  {40, 63},  {41, 63},  {42, 63},  {43, 63},
        {44, 63},  {45, 63},  {46, 63},  {47, 63},  {48, 63},  {49, 63},  {50, 63},  {51, 63},
        {52, 63},  {53, 63},  {54, 63},  {55, 63},  {56, 63},  {57, 63},  {58, 63},  {59, 63},
        {60, 63},  {61, 63},  {62, 63},  {63, 63},  {64, 63},  {65, 63},  {66, 63},  {67, 63},
        {68, 63},  {69, 63},  {70, 63},  {71, 63},  {72, 63},  {73, 63},  {74, 63},  {75, 63},
        {76, 63},  {77, 63},  {78, 63},  {79, 63},  {80, 63},  {81, 63},  {82, 63},  {83, 63},
        {84, 63},  {85, 63},  {86, 63},  {87, 63},  {88, 63},  {89, 63},  {90, 63},  {91, 63},
        {92, 63},  {93, 63},  {94, 63},  {95, 63},  {96, 63},  {97, 63},  {98, 63},  {99, 63},
        {100, 63}, {101, 63}, {102, 63}, {103, 63}, {104, 63}, {105, 63}, {106, 63}, {107, 63},
        {108, 63}, {109, 63}, {110, 63}, {111, 63}, {112, 63}, {113, 63}, {114, 63}, {115, 63},
        {116, 63}, {117, 63}, {118, 63}, {119, 63}, {120, 63}, {121, 63}, {122, 63}, {123, 63},
        {124, 63}, {125, 63}, {126, 63}, {127, 63},
    };

    static bool positionedPixels;
    if(!positionedPixels) {
        for(int i = 0; i < 996; i++) {
            pixelated[i][1] -= 16;
        }
        positionedPixels = true;
    }

    static int lastSceneShift = 0;
    static uint32_t shiftCounter = 0;
    static int shiftDirection = 1;
    static int pixelsToRemove = 996 - 1;
    if(health <= 0) {
        shiftDirection = 0;
        for(int i = 0; i < 6; i++) {
            if(pixelsToRemove >= 0) {
                pixelated[pixelsToRemove][0] = 0;
                pixelated[pixelsToRemove--][0] = 0;
            } else {
                showBackground = false;
            }
        }
    }

    for(int i = 0; i < 996; i++) {
        if(pixelsToRemove >= 0) {
            canvas_draw_dot(canvas, pixelated[i][0], pixelated[i][1]);
        } else {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;

                //Enemy is in the way of the grave
                if(fabs(backgroundAssets[0].posX - entity_pos_get(enemies[i].instance).x) < 18) {
                    level_remove_entity(gameLevel, enemies[i].instance);

                    genann_free(enemies[i].lastAI);
                    genann_free(enemies[i].ai);
                    enemies[i].instance = NULL;
                }

                for(int j = 0; j < MAX_OBSTACLES; j++) {
                    if(obstacles[j].visible) obstacles[j].visible = false;
                }
            }
        }
    }

    uint32_t speedOfShift = playerLevel > 5 ? 20 : 530;
    uint32_t shiftVariance = playerLevel > 5 ? 2 : 0;

    if(furi_get_tick() - lastSceneShift >= speedOfShift) {
        //Shift
        lastSceneShift = furi_get_tick();
        for(int i = 0; i < 996; i++) {
            pixelated[i][0] += shiftVariance > 0 ? shiftDirection : 0;
        }
        if(shiftCounter++ >= shiftVariance) {
            shiftCounter = 0;
            shiftDirection *= -1;
        }
    }
}

void global_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    GameContext* game_context = game_manager_game_context_get(manager);
    UNUSED(game_context);

    static uint32_t counter;
    if(counter < 200) {
        counter++;
        renderSceneBackground(canvas);
    } else {
        showBackground = false;
        canvas_draw_frame(canvas, 2, -4, 124, 61);
        canvas_draw_line(canvas, 1, 57, 0, 60);
        canvas_draw_line(canvas, 126, 57, 128, 61);

        if(health <= 0) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;

                //Enemy is in the way of the grave
                if(fabs(backgroundAssets[0].posX - entity_pos_get(enemies[i].instance).x) < 18) {
                    level_remove_entity(gameLevel, enemies[i].instance);

                    genann_free(enemies[i].lastAI);
                    genann_free(enemies[i].ai);
                    enemies[i].instance = NULL;
                }

                for(int j = 0; j < MAX_OBSTACLES; j++) {
                    if(obstacles[j].visible) obstacles[j].visible = false;
                }
            }
        }
    }
    //Draw background assets
    for(uint32_t i = 0; i < BACKGROUND_ASSET_COUNT; i++) {
        struct BackgroundAsset asset = backgroundAssets[i];
        if(asset.type == -1) continue;
        canvas_draw_sprite(
            canvas,
            asset.type == 0 ? game_context->backgroundAsset1 : game_context->backgroundAsset2,
            asset.posX,
            asset.posY);
    }

    if(health <= 0) {
        static uint32_t lastBlink = 0;
        canvas_printf_blinking(
            canvas,
            CLAMP(
                backgroundAssets[0].posX,
                (uint32_t)WORLD_BORDER_RIGHT_X - 10,
                (uint32_t)WORLD_BORDER_LEFT_X + 5),
            backgroundAssets[0].posY - 5,
            1000,
            500,
            "R.I.P",
            &lastBlink);
        if(canRespawn || game_menu_tutorial_selected) {
            canvas_printf(canvas, 20, 20, "Press Back to respawn.");
        } else if(!showBackground) {
            canvas_printf(canvas, 28, 30, "Current Level: %d", playerLevel + 1);
            canvas_printf(canvas, 46, 10, "Total Kills: %d", kills);
            static int highestLevel;
            static bool queriedHighestLevel = false;
            if(!queriedHighestLevel) {
                Storage* storage = furi_record_open(RECORD_STORAGE);
                File* file = storage_file_alloc(storage);
                char buffer[12];
                if(storage_file_open(
                       file, APP_DATA_PATH("game_data.deadzone"), FSAM_READ, FSOM_OPEN_ALWAYS)) {
                    storage_file_read(file, buffer, 12);
                    highestLevel = atoi(buffer);
                    storage_file_close(file);
                }
                FURI_LOG_I(
                    "DEADZONE",
                    "The highest level was: %d, the current level is: %d, will overwrite(1=yes): %d",
                    highestLevel,
                    playerLevel,
                    highestLevel < playerLevel ? 1 : 0);
                if(highestLevel < playerLevel) {
                    highestLevel = playerLevel;
                    //Update highest level
                    const char* newBuffer = int_to_string(highestLevel);
                    if(storage_file_open(
                           file,
                           APP_DATA_PATH("game_data.deadzone"),
                           FSAM_WRITE,
                           FSOM_OPEN_ALWAYS)) {
                        storage_file_write(file, newBuffer, 12);
                    }
                }

                storage_file_close(file);

                // Deallocate file
                storage_file_free(file);

                // Close storage
                furi_record_close(RECORD_STORAGE);
                queriedHighestLevel = true;
            }
            canvas_printf(canvas, 31, 20, "Record Level: %d", highestLevel + 1);
        }
    }
}

void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;

    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    // We subtract 5 from x and y, because collision box is 10x10, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);
    //canvas_draw_frame(canvas, 0, -4, 135, 64);

    // Get game context

    // Draw score
    //canvas_printf(canvas, 60, 7, "Enemy Lives: %d", enemies[0].lives);
    if(game_menu_tutorial_selected) {
        canvas_printf(canvas, 80, 7, "Health: %d", health);
    } else {
        canvas_printf(canvas, pos.x + 1, pos.y - 10, "%d", health);
    }

    if(kills > 0) {
        //canvas_printf(canvas, 10, 7, "Kills: %d", kills);
    }

    game_level_player_render(manager, canvas, player);
    tutorial_level_player_render(manager, canvas, player);

#ifdef DEBUGGING
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance == NULL) continue;
        for(int j = 0; j < enemies[i].ai->total_weights; j++) {
            //canvas_printf(canvas, 20 + j * 5, 50, "Weights: %.1f", enemies[i].ai->weight[j]);
        }
        break;
    }
#endif
}

void enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(manager);
    // Get enemy context
    PlayerContext* player = context;

    // Get enemy position
    Vector pos = entity_pos_get(self);

    //Draw enemy health above their head
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        if(enemies[i].direction) {
            player->sprite = player->sprite_right;
        } else {
            player->sprite = player->sprite_left;
        }
        canvas_printf(canvas, pos.x + 1, pos.y - 10, "%d", enemies[i].lives);
        break;
    }

    // Draw enemy sprite
    // We subtract 5 from x and y, because collision box is 16x16, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);

    //This must be processed here, as the player object will be deinstantiated
    game_level_enemy_render(manager, canvas, context);
    game_level_player_render(manager, canvas, context);
}

void enemy_update(Entity* self, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    PlayerContext* enemyCtx = context;
    UNUSED(manager);

    Vector pos = entity_pos_get(self);

    //Bullet collision
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);
        float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
        float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
        float distance = sqrtf(distXSq + distYSq);
        if(distance < BULLET_DISTANCE_THRESHOLD) {
            //Self destruction of bullet and entity
            level_remove_entity(gameLevel, bullets[i]);
            bullets[i] = NULL;

            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance != self) continue;

                damage_enemy(&enemies[i]);
                break;
            }
            break;
        }
    }

    //Enemy NPC behavior

    uint32_t currentTick = furi_get_tick();
    if(currentTick - lastBehaviorTick > NPC_ANN_BEHAVIOR_LATENCY) {
        lastBehaviorTick = currentTick;
        Vector playerPos = entity_pos_get(globalPlayer);
        float distXSqToPlayer = (playerPos.x - pos.x) * (playerPos.x - pos.x);
        float distYSqToPlayer = (playerPos.y - pos.y) * (playerPos.y - pos.y);
        float distanceToPlayer = sqrtf(distXSqToPlayer + distYSqToPlayer);
        UNUSED(distanceToPlayer);

        Vector closestBullet = (Vector){200.0F, 200.0F};
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(bullets[i] == NULL) continue;
            Vector bulletPos = entity_pos_get(bullets[i]);
            float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
            float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
            float distance = sqrtf(distXSq + distYSq);

            float closestBulletDistX = (closestBullet.x - pos.x) * (closestBullet.x - pos.x);
            float closestBulletDistY = (closestBullet.y - pos.y) * (closestBullet.y - pos.y);
            float closestBulletDist = sqrtf(closestBulletDistX + closestBulletDistY);
            if(distance <= closestBulletDist) {
                closestBullet = bulletPos;
            }
        }

        Enemy* enemy = NULL;
        for(int i = 0; i < MAX_ENEMIES; i++) {
            if(enemies[i].instance != self) continue;
            enemy = &enemies[i];
            break;
        }
        if(enemy != NULL) {
            double features[5];
            featureCalculation(self, features);
            double closestBulletX = features[0];
            double closestBulletY = features[1];
            UNUSED(closestBulletX);
            UNUSED(closestBulletY);
            double enemyX = features[2];
            double enemyY = features[3];
            UNUSED(enemyX);
            UNUSED(enemyY);
            double xDistToPlayer = features[4];
            UNUSED(xDistToPlayer);

#ifdef DEBUGGING
            if(IS_TRAINING) {
                bool shouldJump = fabs(enemyX - closestBulletX) < 17 &&
                                  features[0] != -1; //Is there even a bullet?
                bool attack = xDistToPlayer > 85; //Should we approach the player?
                enum EnemyAction outputs[2] = {
                    attack ? EnemyActionAttack : EnemyActionRetreat,
                    shouldJump ? EnemyActionJump : EnemyActionStand};
                nn_train(enemy->ai, features, outputs, LEARNING_RATE);
                trainCount++;
            }
#endif
            const double* outputs = genann_run(enemy->ai, features);

            int actionValue = (int)round(outputs[0]);
            enum EnemyAction firstAction;
            switch(actionValue) {
            case EnemyActionAttack:
                firstAction = EnemyActionAttack;
                break;
            case EnemyActionRetreat:
                firstAction = EnemyActionRetreat;
                break;
            default:
                firstAction = EnemyActionRetreat;
                break;
            }
            actionValue = (int)round(outputs[1]);
            enum EnemyAction secondAction;
            switch(actionValue) {
            case EnemyActionAttack:
                secondAction = EnemyActionJump;
                break;
            case EnemyActionRetreat:
                secondAction = EnemyActionStand;
                break;
            default:
                secondAction = EnemyActionJump;
                break;
            }

            enum EnemyAction action = secondAction != EnemyActionStand ? secondAction :
                                                                         firstAction;

#ifdef DEBUGGING
            if(IS_TRAINING) {
                if(fabs(features[0] - features[2]) < 15 && action != EnemyActionJump) {
                    //Correct jumping state
                    recentErr += 2;
                } else if(distanceToPlayer > 85 && action != EnemyActionAttack) {
                    recentErr += (double)0.5;
                } else if(distanceToPlayer < 55 && action != EnemyActionRetreat) {
                    recentErr += (double)0.5;
                }
            }

            if(RECENTLY_STARTED_TRAINING) {
                enemy->lastAI = genann_copy(enemy->ai);
            }

            if(DONE_TRAINING) {
                if(recentErr < err) {
                    //Continue using this model, it has improved
                    err = recentErr;
                    FURI_LOG_I(
                        "DEADZONE",
                        "The errors in the ANN model have reduced. We will continue with the current model. (Errors: %f)",
                        recentErr);

                } else {
                    //More errors, revert.
                    genann_free(enemy->ai);
                    enemy->ai = enemy->lastAI;
                }
                trainCount = 0;
                recentErr = 0;
            }
#endif

            lastAction = action;
            switch(action) {
            case EnemyActionAttack:
                Enemy* enemy = NULL;
                for(int i = 0; i < MAX_ENEMIES; i++) {
                    if(enemies[i].instance != self) continue;
                    enemy = &enemies[i];
                    break;
                }

                bool gracePeriod = furi_get_tick() < (enemy->spawnTime + enemy->mercyTicks);

                if(enemyCtx->sprite == enemyCtx->sprite_left) {
                    pos.x = CLAMP(
                        lerp(
                            pos.x,
                            playerPos.x + ((pos.x - playerPos.x > 0) ? 30 : -30),
                            enemySpeed),
                        WORLD_BORDER_RIGHT_X,
                        //Prevent from approaching too much during player spawning / grace period
                        gracePeriod ? (WORLD_BORDER_RIGHT_X - 14) : WORLD_BORDER_LEFT_X);
                } else {
                    pos.x = CLAMP(
                        lerp(pos.x, pos.x + (pos.x - playerPos.x < 0) ? 30 : -30, enemySpeed),
                        WORLD_BORDER_RIGHT_X,
                        WORLD_BORDER_LEFT_X);
                    break;
                }
                break;
            case EnemyActionRetreat:
                pos.x = CLAMP(
                    lerp(
                        pos.x,
                        pos.x + (enemyCtx->sprite == enemyCtx->sprite_left ?
                                     (pos.x - playerPos.x) :
                                     (playerPos.x - pos.x)),
                        enemySpeed),
                    WORLD_BORDER_RIGHT_X,
                    WORLD_BORDER_LEFT_X);
                break;
            case EnemyActionStand:
                break;
            case EnemyActionJump:
                //Prevent from jumping during start of first level (player spawning)
                if(furi_get_tick() - firstMobSpawnTicks < 3000) {
                    break;
                }
                for(int i = 0; i < MAX_ENEMIES; i++) {
                    if(enemies[i].instance != self) continue;
                    //Is on ground and can jump
                    if(!enemies[i].jumping) {
                        enemies[i].jumping = true;
                        //Enemies have a higher jump height
                        enemies[i].targetY = CLAMP(
                            WORLD_BORDER_BOTTOM_Y - (enemyJumpHeight),
                            WORLD_BORDER_BOTTOM_Y,
                            WORLD_BORDER_TOP_Y);
                        return;
                    }
                    break;
                }
                break;
            }
        }
    }
    //Gravity & movement interpolation
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        pos.y = lerp(pos.y, enemies[i].targetY, jumpSpeed);

        if(enemies[i].jumping && roundf(pos.y) == roundf(enemies[i].targetY)) {
            enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        }

        if(enemies[i].jumping && roundf(pos.y) == WORLD_BORDER_BOTTOM_Y) {
            enemies[i].jumping = false;
        }
        break;
    }

    //Enemy shooting
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        enemy_shoot_handler(&enemies[i], &pos, &enemies[i].lastShot, context);
        break;
    }

    //Update position of npc
    entity_pos_set(self, pos);
}

/****** Entities: Target ******/

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    //canvas_draw_box(canvas, pos.x, pos.y, 8, 5);
    canvas_draw_disc(canvas, pos.x, pos.y, 2);
}

static const EntityDescription target_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static const EntityDescription target_enemy_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static void tutorial_level_alloc(Level* level, GameManager* manager) {
    if(!game_menu_tutorial_selected) return;
    showBackground = false;
    //Add enemy to level
    if(firstKillTick == 0) enemy_spawn(level, manager, (Vector){110, 49}, 3000, false);
}

/****** Level ******/

static void level_alloc(Level* level, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    showBackground = true;
    UNUSED(manager);
    UNUSED(context);
    level_add_entity(level, &global_desc);

    // Add player entity to the level
    player_spawn(level, manager);

    tutorial_level_alloc(level, manager);

    // Add target entity to the level
    //level_add_entity(level, &target_desc);

    //level_add_entity(level, &target_enemy_desc);
    gameLevel = level;
    hideBackgroundAssets();
}

/*
    Alloc/free is called once, when level is added/removed from the game. 
    It useful if you have small amount of levels and entities, that can be allocated at once.

    Start/stop is called when level is changed to/from this level.
    It will save memory, because you can allocate entities only for current level.
*/
static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL, // called once, when level freed
    .start = NULL, // called when level is changed to this level
    .stop = NULL, // called when level is changed from this level
    .context_size = 0, // size of level context, will be automatically allocated and freed
};

/****** Game ******/

static void game_start_post_menu(GameManager* game_manager, void* ctx) {
    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext* game_context = ctx;
    game_context->score = 0;
    game_context->sceneBackground =
        game_manager_sprite_load(game_manager, "scene_background.fxbm");

    game_context->backgroundAsset1 =
        game_manager_sprite_load(game_manager, "background_asset_1.fxbm");
    game_context->backgroundAsset2 =
        game_manager_sprite_load(game_manager, "background_asset_2.fxbm");

    globalGameManager = game_manager;

    // Add level to the game
    game_manager_add_level(game_manager, &level);

    //Instantiate all bullets

    for(int i = 0; i < MAX_BULLETS; i++) {
        bullets[i] = NULL;
        enemyBullets[i] = NULL;
    }

    //TODO ANIMATIONS FOR GAME, such as particles going up like fire on player
    //TODO walk animation, optional shooting animation
}

/*
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager* game_manager, void* ctx) {
    UNUSED(game_manager);
    if(game_menu_quit_selected) return;

    //Instantiate the lock
    game_menu_open(game_manager, false);

    game_start_post_menu((game_manager), ctx);

    if(game_menu_quit_selected) {
        game_manager_game_stop(game_manager);
    }
}

void game_stop(void* ctx) {
    //Leave immediately if they want to quit.
    if(game_menu_quit_selected || health <= 0) {
        return;
    }
    // Do some deinitialization here, for example you can save score to storage.
    // For simplicity, we will just print it.

    game_menu_exit_lock = api_lock_alloc_locked();
    Gui* gui = furi_record_open(RECORD_GUI);
    Submenu* submenu = submenu_alloc();
    /*submenu_add_item(
        submenu,
        game_menu_game_selected ? "RESUME GAME" : "START GAME",
        3,
        game_menu_button_callback,
        ctx);
    submenu_add_item(
        submenu,
        game_menu_tutorial_selected ? "RESUME TUTORIAL" : "TUTORIAL",
        4,
        game_menu_button_callback,
        ctx);*/
    submenu_add_item(submenu, "QUIT", 2, game_menu_button_callback, ctx);
    ViewHolder* view_holder = view_holder_alloc();
    view_holder_set_view(view_holder, submenu_get_view(submenu));
    view_holder_attach_to_gui(view_holder, gui);
    api_lock_wait_unlock_and_free(game_menu_exit_lock);

    //Do they want to quit?
    if(game_menu_quit_selected) {
        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        // End access to the GUI API.
        furi_record_close(RECORD_GUI);
    } else { //Or relaunch
        //Clear previous menu
        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        furi_record_close(RECORD_GUI);
        relaunch_game();
    }
}
/*
    Yor game configuration, do not rename this variable, but you can change it's content here.  
*/
const Game game = {
    .target_fps = 110, // target fps, game will try to keep this value
    .show_fps = false, // show fps counter on the screen
    .always_backlight = false, // keep display backlight always on
    .start = game_start, // will be called once, when game starts
    .stop = game_stop, // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};

int32_t relaunch_game() {
    GameManager* game_manager = game_manager_alloc();

    GameEngineSettings settings = game_engine_settings_init();
    settings.target_fps = game.target_fps;
    settings.show_fps = game.show_fps;
    settings.always_backlight = game.always_backlight;
    settings.frame_callback = frame_cb;
    settings.context = game_manager;

    GameEngine* engine = game_engine_alloc(settings);
    game_manager_engine_set(game_manager, engine);

    void* game_context = NULL;
    if(game.context_size > 0) {
        game_context = malloc(game.context_size);
        game_manager_game_context_set(game_manager, game_context);
    }
    game_start_post_menu(game_manager, game_context);
    game_engine_run(engine);
    game_engine_free(engine);

    game_manager_free(game_manager);

    game_stop(game_context);
    if(game_context) {
        free(game_context);
    }
    return 0;
}
