#pragma once
#include <furi.h>
#include <engine/game_engine.h>
#include <engine/game_manager_i.h>
#include <genann.h>
#include <storage/storage.h>

#include "game_menu.h"
#include "game_level.h"
#include "tutorial_level.h"

#include "npc_ai_variants.h"

/* Global game defines go here */
//#define DEBUGGING
//#define MINIMAL_DEBUGGING

#define WORLD_BORDER_LEFT_X   6
#define WORLD_BORDER_RIGHT_X  120
#define WORLD_BORDER_TOP_Y    6
#define WORLD_BORDER_BOTTOM_Y 53

#define WORLD_TRANSITION_LEFT_STARTING_POINT 0
#define WORLD_TRANSITION_LEFT_ENDING_POINT   4

#define BULLET_DISTANCE_THRESHOLD 8

#define STARTING_PLAYER_HEALTH 16
#define STARTING_ENEMY_HEALTH  5

#define BACKGROUND_ASSET_ROWS  1
#define BACKGROUND_ASSET_COUNT 1 //Must be divisible by BACKGROUND_ASSET_ROWS

#ifdef __cplusplus
extern "C" {
#endif

#define NPC_ANN_WEIGHT_UNIQUENESS
//#define DEBUGGING
//#define MINIMAL_DEBUGGING

//Configurable variables

//While debugging we increase all lives for longer testing/gameplay.
static const EntityDescription global_desc;
static const EntityDescription player_desc;
static const EntityDescription enemy_desc;

//Enemy structure
typedef struct {
    Entity* instance;
    bool direction;
    bool jumping;
    float targetY;
    int lives;
    uint32_t spawnTime;
    uint32_t mercyTicks;
    uint32_t lastShot;
    genann* ai;
    genann* lastAI;
} Enemy;

#define MAX_ENEMIES 30
extern Enemy enemies[MAX_ENEMIES];

extern int ENEMY_LIVES;

extern uint32_t shootingDelay;
extern uint32_t enemyShootingDelay;
extern float bulletMoveSpeed;
extern float speed;
extern float enemySpeed;
extern float jumpHeight;
extern float jumpSpeed;
extern float enemyJumpHeight;
//Tracking player data
extern bool canRespawn;
extern int health;
extern uint32_t kills;
extern bool jumping;
extern float targetY;
extern float targetX;
extern bool showBackground;
#define TRANSITION_FRAMES 15

extern int16_t transitionLeftTicks;
extern int16_t transitionRightTicks;
//Internals
extern int firstMobSpawnTicks;
extern uint32_t firstKillTick;
extern uint32_t secondKillTick;
extern uint32_t gameBeginningTick;
extern Level* gameLevel;
extern GameManager* globalGameManager;
extern Entity* globalPlayer;

//Functions
float lerp(float y1, float y2, float t);

void renderSceneBackground(Canvas* canvas);
void enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right);
void _enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right,
    int startingLives);
bool damage_enemy(Enemy* enemy);
bool damage_player(Entity* self);
void global_update(Entity* self, GameManager* manager, void* context);
void global_render(Entity* self, GameManager* manager, Canvas* canvas, void* context);
void player_update(Entity* self, GameManager* manager, void* context);
void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context);
void enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context);
void enemy_update(Entity* self, GameManager* manager, void* context);
void hideBackgroundAssets();
void computeBackgroundAssets();
int32_t relaunch_game();

void canvas_printf_blinking(
    Canvas* canvas,
    uint32_t x,
    uint32_t y,
    uint32_t shownTicks,
    uint32_t hiddenTicks,
    const char* format,
    uint32_t* dataHolder);

typedef struct {
    Sprite* sprite;

    Sprite* sprite_left;
    Sprite* sprite_left_shadowed;
    Sprite* sprite_left_recoil;

    Sprite* sprite_right;
    Sprite* sprite_right_shadowed;
    Sprite* sprite_right_recoil;

    Sprite* sprite_jump;
    Sprite* sprite_stand;
    Sprite* sprite_forward;
} PlayerContext;

typedef struct {
    uint32_t score;
    Sprite* sceneBackground;
    Sprite* backgroundAsset1;
    Sprite* backgroundAsset2;
} GameContext;

static const EntityDescription global_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = global_update, // called every frame
    .render = global_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

static const EntityDescription enemy_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = enemy_update, // called every frame
    .render = enemy_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

#ifdef __cplusplus
}
#endif
