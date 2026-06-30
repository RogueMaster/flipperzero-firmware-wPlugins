#pragma once
#include "engine/engine.h"
#include <flip_world.h>
#include <game/game.h>
#include "engine/sensors/imu.h"

#define MAX_ENEMIES 12
#define MAX_LEVELS 5
#define MAX_NPCS 1

typedef enum
{
    SPRITE_ID_AXE,
    SPRITE_ID_BOW,
    SPRITE_ID_NAKED,
    SPRITE_ID_SWORD,
    SPRITE_ID_CYCLOPS,
    SPRITE_ID_GHOST,
    SPRITE_ID_OGRE,
    SPRITE_ID_FUNNY
} SpriteID;

// EntityContext definition
typedef struct
{
    SpriteID id;                // Unique ID for the entity type
    uint8_t index;              // Index for the specific entity instance
    Vector size;                // Size of the entity
    Sprite *sprite_right;       // Entity sprite when looking right
    Sprite *sprite_left;        // Entity sprite when looking left
    EntityDirection direction;  // Direction the entity is facing
    EntityState state;          // Current state of the entity
    Vector start_position;      // Start position of the entity
    Vector end_position;        // End position of the entity
    float move_timer;           // Timer for the entity movement
    float elapsed_move_timer;   // Elapsed time for the entity movement
    float radius;               // Collision radius for the entity
    float speed;                // Speed of the entity
    float attack_timer;         // Cooldown duration between attacks
    float elapsed_attack_timer; // Time elapsed since the last attack
    uint32_t strength;          // Damage the entity deals
    float health;               // Health of the entity
    char message[64];           // Message to display when interacting with the entity
    bool is_user;               // Flag to indicate if the entity is a live player or not
    char username[32];          // entity username
    uint32_t xp;                // experience points
    uint32_t level;             // entity level
} EntityContext;

typedef struct
{
    Vector old_position;        // previous position of the player
    EntityDirection direction;  // direction the player is facing
    EntityState state;          // current state of the player
    Vector start_position;      // starting position of the player
    Sprite *sprite_right;       // player sprite looking right
    Sprite *sprite_left;        // player sprite looking left
    int8_t dx;                  // x direction
    int8_t dy;                  // y direction
    uint32_t xp;                // experience points
    uint32_t level;             // player level
    uint32_t strength;          // player strength
    uint32_t health;            // player health
    uint32_t max_health;        // player maximum health
    uint8_t health_regen;       // player health regeneration rate per second/frame
    float elapsed_health_regen; // time elapsed since last health regeneration
    float attack_timer;         // Cooldown duration between attacks
    float elapsed_attack_timer; // Time elapsed since the last attack
    char username[32];          // player username
    bool left;                  // track player sprite direction
} PlayerContext;

// two screens for the game menu
typedef enum
{
    GAME_MENU_INFO, // level, health, xp, etc.
    GAME_MENU_MORE, // more settings
    GAME_MENU_NPC,  // NPC dialog
} GameMenuScreen;

// game modes
typedef enum
{
    GAME_MODE_PVE = 0,   // player(s) vs everyone
    GAME_MODE_PVP = 1,   // player vs player
    GAME_MODE_STORY = 2, // story mode
} GameMode;

// game ending reasons
typedef enum
{
    GAME_END_MEMORY = 0,              // ran out of memory
    GAME_END_TUTORIAL_INCOMPLETE = 1, // tutorial incomplete
    GAME_END_PVP_REQUIREMENT = 2,     // player level too low for pvp
    GAME_END_PVP_ENEMY_DEAD = 3,      // enemy dead
    GAME_END_PVP_PLAYER_DEAD = 4,     // player dead
    GAME_END_NETWORK = 5,             // network error
    GAME_END_APP = 6,                 // app issue
} GameEndReason;

typedef struct
{
    Level *levels[MAX_LEVELS];
    Entity *enemies[MAX_ENEMIES];
    Entity *npcs[MAX_NPCS];
    Entity *player;
    //
    float fps;
    int8_t level_count;
    int8_t enemy_count;
    int8_t npc_count;
    int8_t current_level;
    bool ended_early;
    Imu *imu;
    bool imu_present;
    //
    bool is_switching_level;
    bool is_menu_open;
    //
    uint16_t elapsed_button_timer;
    uint8_t last_button;
    //
    GameMenuScreen menu_screen;
    uint8_t menu_selection;
    //
    GameMode game_mode;
    GameEndReason end_reason;
    //
    uint32_t icon_count;
    uint16_t icon_offset;
    //
    char message[64];
    //
    uint32_t story_step;
    //
    FlipperHTTP *fhttp;
    //
} GameContext;

typedef struct
{
    SpriteID id;
    char left_file_name[33];
    char right_file_name[33];
    uint8_t width;
    uint8_t height;
} SpriteContext;

extern const EntityDescription player_desc;
void player_spawn(Level *level, GameManager *manager);
SpriteContext *sprite_context_get(const char *name);
int player_level_iterative_get(uint32_t xp);
