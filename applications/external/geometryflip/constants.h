#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <m-array.h>
#include <math.h>

#define TAG                     "GeometryDash"
#define GD_APP_DATA_PATH        EXT_PATH("apps_data/geometry_flip")
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64
#define CUBE_SIZE               8
#define CUBE_X_POS              20
#define CUBE_JUMP_VEL           -120.0f
#define CUBE_GRAVITY            250.0f
#define GROUND_Y                (SCREEN_HEIGHT - 8)
#define SHIP_SIZE               8
#define SHIP_FLY_FORCE          -130.0f
#define SHIP_GRAVITY            130.0f
#define SHIP_MAX_FALLSPEED      220.0f
#define SHIP_MAX_RISESPEED      -220.0f
#define SHIP_ACCELERATION       0.8f
#define BALL_SIZE               8
#define BALL_GRAVITY            180.0f
#define BALL_FLIP_IMPULSE       100.0f
#define BALL_FLIP_COOLDOWN      150
#define UFO_SIZE                8
#define UFO_JUMP_VEL            -130.0f
#define UFO_GRAVITY             220.0f
#define SCROLL_SPEED            60.0f
#define OBSTACLE_WIDTH          CUBE_SIZE
#define OBSTACLE_HEIGHT         CUBE_SIZE
#define OBSTACLE_GAP            20
#define COYOTE_TIME_MS          120
#define MIN_COLLISION_THRESHOLD 1.0f
#define BALL_JUMP_VEL           -100.0f

typedef enum {
    GameStateMenu,
    GameStatePlaying,
    GameStateGameOver,
    GameStateWin
} GameState;

typedef enum {
    PlayerModeCube,
    PlayerModeShip,
    PlayerModeBall,
    PlayerModeUfo
} PlayerMode;

typedef enum {
    GravityDirectionDown,
    GravityDirectionUp
} GravityDirection;

typedef enum {
    ObstacleTypeNone = 0,
    ObstacleTypeBlock,
    ObstacleTypeSpikeUp,
    ObstacleTypeSpikeDown,
    ObstacleTypeSpikeLeft,
    ObstacleTypeSpikeRight,
    ObstacleTypePortalShip,
    ObstacleTypePortalCube,
    ObstacleTypePortalBall,
    ObstacleTypePortalUfo,
    ObstacleTypeGravityUp,
    ObstacleTypeGravityDown,
    ObstacleTypeCount
} ObstacleType;

#define MAX_LEVEL_OBSTACLES 2000

typedef struct {
    ObstacleType type;
    uint8_t grid_x;
    uint8_t grid_y;
    uint8_t length;
} Obstacle;

typedef struct {
    Obstacle obstacles[MAX_LEVEL_OBSTACLES];
    int length;
    char name[32];
    int max_grid_x;
} LevelData;

// Forward declaration for circular dependency
typedef struct {
    PlayerMode player_mode;
    GravityDirection gravity_dir;
    float cube_y;
    float cube_velocity_y;
    bool is_on_ground;
    uint32_t ground_lost_time;
    uint32_t ball_flip_timer;
} PlayerState;

ARRAY_DEF(LevelFileArray, char*, M_POD_OPLIST)

typedef struct {
    GameState state;
    PlayerState player;
    float scroll_offset;
    uint32_t last_tick;
    bool input_pressed;
    int selected_menu_item;
    LevelData current_level;
    LevelFileArray_t level_files;
} GeometryDashApp;
