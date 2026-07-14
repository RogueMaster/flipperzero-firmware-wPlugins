#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_vibro.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <math.h>

// Screen dimensions for Flipper Zero
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define WORLD_WIDTH   SCREEN_WIDTH
#define PLAY_HEIGHT   (SCREEN_HEIGHT - 7)
#define ARRAY_SIZE    (WORLD_WIDTH * PLAY_HEIGHT + 1)
#define LOG_FILE      STORAGE_PATH "grokadven3_loggy.txt"

// Game states
typedef enum {
    STATE_TITLE,
    STATE_GAME,
    STATE_PAUSED,
    STATE_DEATH,
    STATE_EXIT
} GameState;

// Entity types
typedef enum {
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS,
    ENTITY_FINAL_BOSS
} EntityType;

// Entity structure
typedef struct {
    EntityType type;
    int x, y;
    int width, height;
    int health_bars;
    int power_points;
    int power_level;
    bool facing_right;
    uint64_t double_power_timer;
    uint64_t pickup_cooldown;
} Entity;

// Platform structure
typedef struct {
    int x, y;
    int width, height;
} Platform;

// Pickup types
typedef enum {
    PICKUP_ALMOND_WATER,
    PICKUP_CHEST,
    PICKUP_DOOR,
    PICKUP_FURNITURE
} PickupType;

typedef struct {
    PickupType type;
    int x, y;
    char symbol; // For furniture variety
    bool falling;
} Pickup;

// Projectile structure
typedef struct {
    int x, y;
    int dx, dy;
    bool active;
    bool from_player;
} Projectile;

typedef struct {
    bool dark_mode;
    bool fps_display;
} GameSettings;

// Game context
typedef struct {
    GameState state;
    Canvas* canvas;
    InputEvent input;
    uint32_t frame_count;
    float fps;
    uint64_t last_frame_time;
    uint64_t last_fps_check;
    uint64_t last_input_time;
    uint64_t day_night_timer;
    uint64_t weather_timer;
    uint64_t weather_duration;
    uint64_t final_boss_timer;
    uint64_t iframe_timer;
    uint64_t special_timer;
    uint64_t hold_back_timer;
    uint64_t hold_up_timer;
    uint64_t hold_down_timer;
    uint64_t hold_left_timer;
    uint64_t hold_right_timer;
    uint64_t door_timer;
    uint64_t door_cooldown;
    bool is_night;
    bool weather_active;
    int weather_type; // 0: none, 1: snow, 2: rain, 3: constellations
    int sun_moon_x;
    GameSettings settings;
    Entity player;
    Entity entities[5];
    int entity_count;
    int almond_water_count;
    bool in_iframes;
    bool special_active;
    char world_map[ARRAY_SIZE];
    char world_objects[ARRAY_SIZE];
    char oset[5];
    Platform platforms[20];
    int platform_count;
    Pickup pickups[50];
    int pickup_count;
    Projectile player_projectile;
    Projectile enemy_projectiles[2];
    int enemy_projectile_count;
    uint32_t world_seed;
    bool bypass_collision;
} GameContext;

// File paths
#define STORAGE_PATH       "/ext/grokadven_03/"
#define WMA_FILE           STORAGE_PATH "WMA_arr.txt"
#define WSO_FILE           STORAGE_PATH "WSO_arr.txt"
#define SETTINGS_FILE      STORAGE_PATH "settings.bin"
#define PLAYER_DATA_FILE   STORAGE_PATH "player_data.bin"
#define PLAYER_DATA_BACKUP STORAGE_PATH "player_data_backup.bin"

// Log events to file
static void log_event(const char* event) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return;
    }
    if(storage_file_open(file, LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char timestamp[20];
        snprintf(timestamp, 20, "[%u] ", (unsigned int)furi_get_tick());
        storage_file_write(file, timestamp, strlen(timestamp));
        storage_file_write(file, event, strlen(event));
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Initialize arrays
static void init_arrays(GameContext* ctx) {
    if(!ctx) return;
    memset(ctx->world_map, 0, ARRAY_SIZE);
    memset(ctx->world_objects, 0, ARRAY_SIZE);
    ctx->world_map[(ARRAY_SIZE - 1) / 2] = '~'; // Respawn point
    strcpy(ctx->oset, "000Z");
    ctx->world_seed = furi_get_tick(); // Faux-random seed
}

// Save array to file
static bool save_array(const char* filename, const char* data, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for save");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for save");
        return false;
    }
    bool success = storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        storage_file_write(file, data, size);
    } else {
        log_event("Failed to open file for save");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load array from file
static bool load_array(const char* filename, char* data, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for load");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for load");
        return false;
    }
    bool success = storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, data, size);
    } else {
        log_event("Failed to open file for load");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Save settings
static bool save_settings(GameContext* ctx) {
    if(!ctx) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for settings save");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for settings save");
        return false;
    }
    bool success = storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        storage_file_write(file, &ctx->settings, sizeof(GameSettings));
    } else {
        log_event("Failed to open settings file for save");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load settings
static bool load_settings(GameContext* ctx) {
    if(!ctx) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for settings load");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for settings load");
        return false;
    }
    bool success = storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, &ctx->settings, sizeof(GameSettings));
    } else {
        log_event("Failed to open settings file for load");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Save player data
static bool save_player_data(GameContext* ctx) {
    if(!ctx) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for player data save");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for player data save");
        return false;
    }
    bool success = false;
    for(int i = 0; i < 2; i++) {
        if(storage_file_open(file, PLAYER_DATA_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, &ctx->player, sizeof(Entity));
            storage_file_write(file, ctx->oset, sizeof(ctx->oset));
            storage_file_write(file, &ctx->world_seed, sizeof(uint32_t));
            storage_file_write(file, &ctx->almond_water_count, sizeof(int));
            success = true;
            break;
        }
    }
    if(!success) {
        log_event("Failed to save player data, creating backup");
        if(storage_file_open(file, PLAYER_DATA_BACKUP, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, &ctx->player, sizeof(Entity));
            storage_file_write(file, ctx->oset, sizeof(ctx->oset));
            storage_file_write(file, &ctx->world_seed, sizeof(uint32_t));
            storage_file_write(file, &ctx->almond_water_count, sizeof(int));
        } else {
            log_event("Failed to create player data backup");
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load player data
static bool load_player_data(GameContext* ctx) {
    if(!ctx) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        log_event("Failed to open storage for player data load");
        return false;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        log_event("Failed to allocate file for player data load");
        return false;
    }
    bool success = storage_file_open(file, PLAYER_DATA_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, &ctx->player, sizeof(Entity));
        storage_file_read(file, ctx->oset, sizeof(ctx->oset));
        storage_file_read(file, &ctx->world_seed, sizeof(uint32_t));
        storage_file_read(file, &ctx->almond_water_count, sizeof(int));
    } else {
        log_event("Failed to load player data, trying backup");
        success = storage_file_open(file, PLAYER_DATA_BACKUP, FSAM_READ, FSOM_OPEN_EXISTING);
        if(success) {
            storage_file_read(file, &ctx->player, sizeof(Entity));
            storage_file_read(file, ctx->oset, sizeof(ctx->oset));
            storage_file_read(file, &ctx->world_seed, sizeof(uint32_t));
            storage_file_read(file, &ctx->almond_water_count, sizeof(int));
        } else {
            log_event("Failed to load player data backup");
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Check if player can level up
static void check_level_up(GameContext* ctx) {
    if(!ctx) return;
    while(ctx->player.power_points >= 100 && (ctx->player.width < 9 || ctx->player.height < 12)) {
        ctx->player.power_points -= 100;
        ctx->player.power_level++;
        if(ctx->player.width < 9) ctx->player.width++;
        if(ctx->player.height < 12) ctx->player.height++;
    }
}

// Generate world elements
static void generate_world(GameContext* ctx, bool preserve_player_data) {
    if(!ctx) return;
    srand(ctx->world_seed);
    ctx->platform_count = 0;
    ctx->pickup_count = 0;
    ctx->entity_count = 2;
    // Ground platform
    ctx->platforms[ctx->platform_count].x = 0;
    ctx->platforms[ctx->platform_count].y = PLAY_HEIGHT - 6;
    ctx->platforms[ctx->platform_count].width = WORLD_WIDTH;
    ctx->platforms[ctx->platform_count].height = 6;
    ctx->platform_count++;
    // Random platforms (min 3, max 6)
    int min_platforms = 3;
    while(ctx->platform_count < min_platforms + 1) {
        int px = rand() % (WORLD_WIDTH - 50);
        int py = rand() % (PLAY_HEIGHT - 15) + 9; // Avoid top 9 pixels
        int pw = rand() % 40 + 20;
        ctx->platforms[ctx->platform_count].x = px;
        ctx->platforms[ctx->platform_count].y = py;
        ctx->platforms[ctx->platform_count].width = pw;
        ctx->platforms[ctx->platform_count].height = 3;
        ctx->platform_count++;
    }
    for(int i = 0; i < 19 && ctx->platform_count < 6 + 1; i++) {
        if(rand() % 100 < 10) {
            int px = rand() % (WORLD_WIDTH - 50);
            int py = rand() % (PLAY_HEIGHT - 15) + 9;
            int pw = rand() % 40 + 20;
            ctx->platforms[ctx->platform_count].x = px;
            ctx->platforms[ctx->platform_count].y = py;
            ctx->platforms[ctx->platform_count].width = pw;
            ctx->platforms[ctx->platform_count].height = 3;
            ctx->platform_count++;
        }
    }
    // Clear arrays
    memset(ctx->world_map, 0, ARRAY_SIZE);
    memset(ctx->world_objects, 0, ARRAY_SIZE);
    ctx->world_map[(ARRAY_SIZE - 1) / 2] = '~';
    // Generate walls and pickups
    for(int i = 0; i < ARRAY_SIZE - 1; i++) {
        int x = i % WORLD_WIDTH;
        int y = i / WORLD_WIDTH;
        if(y < 9) continue; // Avoid top 9 pixels
        if(ctx->world_map[i] == 0) {
            int roll = rand() % 100;
            if(roll < 5)
                ctx->world_map[i] = 'W';
            else if(roll < 15) {
                ctx->pickups[ctx->pickup_count].type = PICKUP_ALMOND_WATER;
                ctx->pickups[ctx->pickup_count].x = x;
                ctx->pickups[ctx->pickup_count].y = y;
                ctx->pickups[ctx->pickup_count].falling = true;
                ctx->pickup_count++;
            } else if(roll < 25) {
                ctx->pickups[ctx->pickup_count].type = PICKUP_CHEST;
                ctx->pickups[ctx->pickup_count].x = x;
                ctx->pickups[ctx->pickup_count].y = y;
                ctx->pickups[ctx->pickup_count].falling = true;
                ctx->pickup_count++;
            }
        }
        if(ctx->world_objects[i] == 0 && rand() % 100 < 10) {
            ctx->pickups[ctx->pickup_count].type = PICKUP_FURNITURE;
            ctx->pickups[ctx->pickup_count].x = x;
            ctx->pickups[ctx->pickup_count].y = y;
            ctx->pickups[ctx->pickup_count].symbol = (rand() % 2 == 0) ? 'C' :
                                                                         'H'; // Chair or hat rack
            ctx->pickups[ctx->pickup_count].falling = false;
            ctx->pickup_count++;
        }
    }
    // Ensure minimum 2 enemies
    for(int i = 0; i < 2; i++) {
        ctx->entities[i].type = ENTITY_ENEMY;
        ctx->entities[i].x = rand() % (WORLD_WIDTH - 4);
        ctx->entities[i].y = rand() % (PLAY_HEIGHT - 15) + 9;
        ctx->entities[i].width = 4;
        ctx->entities[i].height = 4;
        ctx->entities[i].health_bars = 5 + ctx->player.power_level;
        ctx->entities[i].power_points = 0;
        ctx->entities[i].power_level = 0;
        ctx->entities[i].facing_right = true;
        ctx->entities[i].double_power_timer = 0;
        ctx->entities[i].pickup_cooldown = 0;
        // Move to platform if below ground
        if(ctx->entities[i].y + ctx->entities[i].height > PLAY_HEIGHT - 6) {
            for(int j = 0; j < ctx->platform_count; j++) {
                if(ctx->entities[i].x + ctx->entities[i].width > ctx->platforms[j].x + 1 &&
                   ctx->entities[i].x < ctx->platforms[j].x + ctx->platforms[j].width - 1) {
                    ctx->entities[i].y = ctx->platforms[j].y - ctx->entities[i].height;
                    break;
                }
            }
        }
    }
    ctx->entity_count = 2;
    // Add boss
    if(ctx->entity_count < 3) {
        ctx->entities[2].type = ENTITY_BOSS;
        ctx->entities[2].x = WORLD_WIDTH / 2 + 20;
        ctx->entities[2].y = PLAY_HEIGHT - 7;
        ctx->entities[2].width = 3;
        ctx->entities[2].height = 6;
        ctx->entities[2].health_bars = 10 + ctx->player.power_level;
        ctx->entities[2].power_points = 0;
        ctx->entities[2].power_level = 0;
        ctx->entities[2].facing_right = true;
        ctx->entities[2].double_power_timer = 0;
        ctx->entities[2].pickup_cooldown = 0;
        ctx->entity_count = 3;
    }
    // Preserve player data if specified
    if(!preserve_player_data) {
        ctx->player.x = SCREEN_WIDTH / 2;
        ctx->player.y = PLAY_HEIGHT - 7;
        ctx->player.health_bars = 2;
        ctx->player.power_points = 0;
        ctx->player.power_level = 0;
        ctx->almond_water_count = 0;
    }
    ctx->world_seed++;
}

// Initialize game context
static void game_init(GameContext* ctx) {
    if(!ctx) return;
    ctx->frame_count = 0;
    ctx->fps = 0.0f;
    ctx->last_frame_time = furi_get_tick();
    ctx->last_fps_check = ctx->last_frame_time;
    ctx->last_input_time = ctx->last_frame_time;
    ctx->day_night_timer = ctx->last_frame_time;
    ctx->weather_timer = ctx->last_frame_time;
    ctx->weather_duration = 0;
    ctx->final_boss_timer = 0;
    ctx->iframe_timer = 0;
    ctx->special_timer = 0;
    ctx->hold_back_timer = 0;
    ctx->hold_up_timer = 0;
    ctx->hold_down_timer = 0;
    ctx->hold_left_timer = 0;
    ctx->hold_right_timer = 0;
    ctx->door_timer = ctx->last_frame_time;
    ctx->door_cooldown = 0;
    ctx->weather_active = false;
    ctx->weather_type = 0;
    ctx->is_night = true;
    ctx->sun_moon_x = 0;
    ctx->settings.dark_mode = true;
    ctx->settings.fps_display = true;
    ctx->player.type = ENTITY_PLAYER;
    ctx->player.x = SCREEN_WIDTH / 2;
    ctx->player.y = PLAY_HEIGHT - 7;
    ctx->player.width = 3;
    ctx->player.height = 4;
    ctx->player.health_bars = 2;
    ctx->player.power_points = 0;
    ctx->player.power_level = 0;
    ctx->player.facing_right = false;
    ctx->player.double_power_timer = 0;
    ctx->player.pickup_cooldown = 0;
    ctx->in_iframes = false;
    ctx->special_active = false;
    ctx->almond_water_count = 0;
    ctx->player_projectile.active = false;
    ctx->enemy_projectile_count = 0;
    ctx->entity_count = 0;
    ctx->bypass_collision = false;
    init_arrays(ctx);
    if(load_settings(ctx)) {
        ctx->settings.dark_mode = true;
    }
    if(load_player_data(ctx)) {
        if(ctx->player.x < 0 || ctx->player.x >= WORLD_WIDTH - ctx->player.width)
            ctx->player.x = SCREEN_WIDTH / 2;
        if(ctx->player.y < 0 || ctx->player.y >= PLAY_HEIGHT - ctx->player.height)
            ctx->player.y = PLAY_HEIGHT - 7;
        if(ctx->almond_water_count < 0 || ctx->almond_water_count > 3) ctx->almond_water_count = 0;
    }
    load_array(WMA_FILE, ctx->world_map, ARRAY_SIZE);
    load_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE);
    check_level_up(ctx);
    ctx->state = STATE_TITLE;
}

// Collision check for entity vs platform
static bool collides_with_platform(int ex, int ey, int ew, int eh, Platform* p) {
    return ex + ew > p->x + 1 && ex < p->x + p->width - 1 && ey + eh >= p->y && ey < p->y + 1;
}

static bool can_move_down(int ex, int ey, int ew, int eh, GameContext* ctx) {
    if(!ctx) return true;
    for(int i = 0; i < ctx->platform_count; i++) {
        if(collides_with_platform(ex, ey + 1, ew, eh, &ctx->platforms[i]) &&
           ey + eh <= ctx->platforms[i].y + 1) {
            return false;
        }
    }
    return true;
}

static bool collides_with_wall(int ex, int ey, int ew, int eh, GameContext* ctx) {
    if(!ctx) return false;
    for(int y = ey; y < ey + eh; y++) {
        for(int x = ex; x < ex + ew; x++) {
            int idx = y * WORLD_WIDTH + x;
            if(idx >= 0 && idx < ARRAY_SIZE && ctx->world_map[idx] == 'W') {
                return true;
            }
        }
    }
    return false;
}

static bool collides_with_furniture(int ex, int ey, int ew, int eh, GameContext* ctx) {
    if(!ctx) return false;
    for(int i = 0; i < ctx->pickup_count; i++) {
        if(ctx->pickups[i].type == PICKUP_FURNITURE) {
            int px = ctx->pickups[i].x;
            int py = ctx->pickups[i].y;
            if(ex + ew > px && ex < px + 1 && ey + eh > py && ey < py + 1) {
                return true;
            }
        }
    }
    return false;
}

// Draw title screen
static void draw_title(GameContext* ctx, Canvas* canvas) {
    if(!ctx || !canvas) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    uint64_t elapsed = furi_get_tick() - ctx->last_frame_time;
    if(elapsed < 1500) {
        // Display title
        canvas_draw_str_aligned(
            canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3 - 8, AlignCenter, AlignCenter, "Grok's");
        canvas_draw_str_aligned(
            canvas,
            SCREEN_WIDTH / 2,
            SCREEN_HEIGHT / 3 + 18,
            AlignCenter,
            AlignCenter,
            "Adventure");
        canvas_draw_str_aligned(
            canvas, (SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 3 + 33, AlignCenter, AlignCenter, "v");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(
            canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3 + 30, AlignCenter, AlignCenter, "3");
    } else if(elapsed < 2000) {
        // Fade out effect
        int fade_level = (int)((2000 - elapsed) / 500.0f * 10); // 0 to 10
        if(fade_level > 0) {
            canvas_set_color(canvas, ColorWhite);
            for(int x = 0; x < SCREEN_WIDTH; x += 10 - fade_level + 1) {
                for(int y = 0; y < SCREEN_HEIGHT; y += 10 - fade_level + 1) {
                    canvas_draw_dot(canvas, x, y);
                }
            }
        }
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

// Draw bottle sprite (4x4)
static void draw_bottle(Canvas* canvas, int x, int y, Color fg, Color bg) {
    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x, y + 1, 4, 3); // Body
    canvas_draw_box(canvas, x + 1, y, 2, 1); // Neck
    canvas_set_color(canvas, bg);
    canvas_draw_frame(canvas, x, y + 1, 4, 3); // Border
}

// Draw skull sprite (4x4)
static void draw_skull(Canvas* canvas, int x, int y, Color fg, Color bg) {
    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x, y, 4, 4);
    canvas_set_color(canvas, bg);
    canvas_draw_dot(canvas, x + 1, y + 1); // Left eye
    canvas_draw_dot(canvas, x + 2, y + 1); // Right eye
    canvas_draw_line(canvas, x + 1, y + 3, x + 2, y + 3); // Mouth
}

// Draw chair sprite (4x4)
static void draw_chair(Canvas* canvas, int x, int y, Color fg, Color bg) {
    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x, y + 1, 4, 3); // Seat
    canvas_draw_line(canvas, x + 3, y, x + 3, y + 3); // Backrest
    canvas_set_color(canvas, bg);
    canvas_draw_frame(canvas, x, y + 1, 4, 3);
}

// Draw hat rack sprite (4x4)
static void draw_hat_rack(Canvas* canvas, int x, int y, Color fg, Color bg) {
    canvas_set_color(canvas, fg);
    canvas_draw_line(canvas, x + 2, y, x + 2, y + 3); // Pole
    canvas_draw_line(canvas, x + 1, y, x + 3, y); // Crossbar
    canvas_set_color(canvas, bg);
    canvas_draw_dot(canvas, x + 2, y + 3); // Base outline
}

// Draw banana sprite (5x5)
static void
    draw_banana(GameContext* ctx, Canvas* canvas, int x, int y, Color fg, Color bg, int health) {
    if(!ctx || !canvas) return;
    canvas_set_color(canvas, fg);
    int size = health > 0 ? 5 : (furi_get_tick() - ctx->last_frame_time) / 200 + 1;
    if(size > 5) size = 1;
    canvas_draw_box(canvas, x, y, size, size);
    if(size == 5) {
        canvas_draw_line(canvas, x + 2, y, x + 2, y - 1); // Stem
        canvas_set_color(canvas, bg);
        canvas_draw_frame(canvas, x, y, 5, 5);
    }
    if(health <= 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x, y + 10, "T2YL");
    }
}

// Draw game world
static void draw_game(GameContext* ctx, Canvas* canvas) {
    if(!ctx || !canvas) return;
    Color bg = ctx->is_night ? ColorBlack : ColorWhite;
    Color fg = ctx->is_night ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, fg);

    // Draw sky
    if(ctx->is_night) {
        canvas_draw_circle(canvas, ctx->sun_moon_x % SCREEN_WIDTH, 10, 2); // Moon
        canvas_draw_dot(canvas, 20, 10); // Fixed stars
        canvas_draw_dot(canvas, 25, 12);
        canvas_draw_dot(canvas, 30, 8);
        if(ctx->weather_active && ctx->weather_type == 3) {
            int stars[2][2] = {{20, 10}, {25, 12}};
            for(int j = 0; j < 2; j++) {
                int sx = (stars[j][0] + ctx->sun_moon_x / 2) % SCREEN_WIDTH;
                canvas_draw_dot(canvas, sx, stars[j][1]);
            }
        }
    } else {
        canvas_draw_circle(canvas, ctx->sun_moon_x % SCREEN_WIDTH, 10, 4); // Sun
        // Add dust-like dots near ground
        srand(furi_get_tick());
        for(int x = 0; x < SCREEN_WIDTH; x += 4) {
            for(int y = PLAY_HEIGHT - 10; y < PLAY_HEIGHT - 1; y += 2) {
                if(rand() % 100 < 5) {
                    canvas_draw_dot(canvas, x, y);
                }
            }
        }
    }

    // Draw weather
    if(ctx->weather_active && ctx->weather_type != 3) {
        int wx = ctx->sun_moon_x % SCREEN_WIDTH;
        int wy = 10;
        if(ctx->weather_type == 1 && ctx->is_night) {
            canvas_draw_line(canvas, wx, wy, wx + 1, wy + 1); // Snow
        } else if(ctx->weather_type == 2 && !ctx->is_night) {
            canvas_draw_dot(canvas, wx, wy); // Rain
        }
    }

    // Draw walls
    for(int y = 0; y < PLAY_HEIGHT; y++) {
        for(int x = 0; x < WORLD_WIDTH; x++) {
            int idx = y * WORLD_WIDTH + x;
            if(idx >= 0 && idx < ARRAY_SIZE && ctx->world_map[idx] == 'W') {
                canvas_draw_box(canvas, x, y, 1, 1);
            }
        }
    }

    // Draw platforms
    for(int i = 0; i < ctx->platform_count; i++) {
        Platform* p = &ctx->platforms[i];
        int px = p->x;
        if(px + p->width < 0 || px > SCREEN_WIDTH) continue;
        canvas_draw_line(
            canvas, px + 1, p->y + p->height - 1, px + p->width - 2, p->y + p->height - 1);
        canvas_draw_line(canvas, px + 3, p->y, px + p->width - 4, p->y);
        canvas_draw_line(canvas, px + 1, p->y + p->height - 1, px + 3, p->y + 1);
        canvas_draw_line(
            canvas, px + p->width - 2, p->y + p->height - 1, px + p->width - 4, p->y + 1);
        for(int py = p->y + 1; py < p->y + p->height - 1; py += 2) {
            canvas_draw_line(canvas, px + 1, py, px + p->width - 2, py); // Shading
        }
    }

    // Draw pickups
    for(int i = 0; i < ctx->pickup_count; i++) {
        Pickup* pk = &ctx->pickups[i];
        int px = pk->x;
        int py = pk->y;
        if(px < 0 || px > SCREEN_WIDTH || py < 9) continue;
        if(pk->type == PICKUP_ALMOND_WATER) {
            draw_bottle(canvas, px, py, fg, bg);
        } else if(pk->type == PICKUP_CHEST) {
            canvas_draw_rbox(canvas, px, py, 4, 4, 2);
            canvas_draw_circle(canvas, px + 2, py + 2, 1);
        } else if(pk->type == PICKUP_DOOR) {
            canvas_set_color(canvas, fg);
            canvas_draw_rframe(canvas, px, py, 6, 7, 1);
            canvas_set_color(canvas, !ctx->is_night ? ColorWhite : ColorBlack);
            canvas_draw_frame(canvas, px, py, 6, 7); // Border
            if(furi_get_tick() / 500 % 2 == 0 &&
               abs(ctx->player.x + ctx->player.width / 2 - (px + 3)) < 3) {
                canvas_draw_frame(canvas, px + 1, py + 1, 4, 5); // Flash
            }
            canvas_set_color(canvas, fg);
            canvas_draw_line(canvas, px + 2, py + 4, px + 3, py + 2); // Up arrow
            canvas_draw_line(canvas, px + 3, py + 2, px + 4, py + 4);
            canvas_draw_line(canvas, px + 2, py + 4, px + 4, py + 4);
        } else if(pk->type == PICKUP_FURNITURE) {
            if(pk->symbol == 'C') {
                draw_chair(canvas, px, py, fg, bg);
            } else {
                draw_hat_rack(canvas, px, py, fg, bg);
            }
        }
    }

    // Draw entities
    for(int i = -1; i < ctx->entity_count; i++) {
        Entity* e = (i == -1) ? &ctx->player : &ctx->entities[i];
        int ex = e->x;
        int ey = e->y;
        if(ex + e->width < 0 || ex > SCREEN_WIDTH || ey < 9) continue;
        Color entity_color = fg;
        Color border_color = bg;
        if(e == &ctx->player && ctx->special_active && (furi_get_tick() / 100 % 2 == 0)) {
            entity_color = bg;
            border_color = fg;
        }
        if(e->type == ENTITY_PLAYER) {
            canvas_set_color(canvas, entity_color);
            canvas_draw_box(canvas, ex, ey, e->width, e->height);
            canvas_set_color(canvas, border_color);
            canvas_draw_line(canvas, ex, ey, ex + e->width - 1, ey);
            canvas_draw_line(canvas, ex, ey, ex, ey + e->height - 1);
            canvas_draw_line(canvas, ex + e->width - 1, ey, ex + e->width - 1, ey + e->height - 1);
        } else if(e->type == ENTITY_ENEMY) {
            draw_skull(canvas, ex, ey, entity_color, border_color);
        } else if(e->type == ENTITY_BOSS) {
            canvas_set_color(canvas, entity_color);
            canvas_draw_box(canvas, ex, ey, 2, 2);
            int curve_dir = e->facing_right ? 1 : -1;
            canvas_draw_line(
                canvas,
                ex + (e->facing_right ? 0 : 1),
                ey + 2,
                ex + (e->facing_right ? 1 : 0),
                ey + 4);
            canvas_draw_line(
                canvas,
                ex + (e->facing_right ? 1 : 0),
                ey + 2,
                ex + (e->facing_right ? 2 : -1),
                ey + 4);
            canvas_draw_box(canvas, ex + curve_dir, ey + 5, 2, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_dot(canvas, ex + 1, ey + 1);
            canvas_draw_dot(canvas, ex + 1 + curve_dir, ey + 1);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, ex + 1, ey + 1, 1, 1);
            canvas_draw_frame(canvas, ex + 1 + curve_dir, ey + 1, 1, 1);
            canvas_set_color(canvas, fg);
        } else {
            draw_banana(ctx, canvas, ex, ey, entity_color, border_color, e->health_bars);
        }
        int bar_x = e->facing_right ? ex - 2 : ex + e->width + 1;
        for(int j = 0; j < e->health_bars; j++) {
            canvas_draw_line(canvas, bar_x, ey + j * 2, bar_x + 2, ey + j * 2);
        }
    }

    // Draw projectiles
    if(ctx->player_projectile.active) {
        int px = ctx->player_projectile.x;
        int py = ctx->player_projectile.y;
        if(px >= 0 && px < SCREEN_WIDTH && py >= 9 && py < PLAY_HEIGHT) {
            canvas_set_color(canvas, fg);
            canvas_draw_box(canvas, px, py, 2, 2);
        }
    }
    for(int i = 0; i < ctx->enemy_projectile_count; i++) {
        if(ctx->enemy_projectiles[i].active) {
            int px = ctx->enemy_projectiles[i].x;
            int py = ctx->enemy_projectiles[i].y;
            if(px >= 0 && px < SCREEN_WIDTH && py >= 9 && py < PLAY_HEIGHT) {
                canvas_set_color(canvas, fg);
                canvas_draw_box(canvas, px, py, 2, 2);
            }
        }
    }

    // UI
    canvas_set_color(canvas, fg);
    if(ctx->settings.fps_display) {
        char fps_str[10];
        snprintf(fps_str, 10, "%.0f", (double)ctx->fps);
        canvas_draw_str(canvas, SCREEN_WIDTH - 21, 11, fps_str);
    }
    char pp_str[20];
    snprintf(pp_str, 20, "PP: %d", ctx->player.power_points);
    canvas_draw_str(canvas, 2, 11, pp_str);
    char aw_str[5];
    snprintf(aw_str, 5, "A: %d", ctx->almond_water_count);
    canvas_draw_str(canvas, 3, SCREEN_HEIGHT / 2, aw_str);
}

// Draw pause
static void draw_pause(GameContext* ctx, Canvas* canvas) {
    if(!ctx || !canvas) return;
    draw_game(ctx, canvas);
    canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    for(int ox = -5; ox <= 5; ox++) {
        for(int oy = -5; oy <= 5; oy++) {
            if(abs(ox) + abs(oy) <= 5) {
                canvas_draw_str(
                    canvas, SCREEN_WIDTH / 2 - 20 + ox, SCREEN_HEIGHT / 2 + oy, "Pause");
            }
        }
    }
    canvas_set_color(canvas, ctx->is_night ? ColorBlack : ColorWhite);
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2, "Pause");
}

// Draw death
static void draw_death(GameContext* ctx, Canvas* canvas) {
    if(!ctx || !canvas) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2, "U DED");
}

// Handle input
static void handle_input(GameContext* ctx, InputEvent* input) {
    if(!ctx || !input) return;
    uint64_t time = furi_get_tick();
    ctx->last_input_time = time;

    if(ctx->state == STATE_TITLE) {
        if(input->type == InputTypePress && input->key == InputKeyOk) {
            ctx->state = STATE_GAME;
            generate_world(ctx, false);
        }
    } else if(ctx->state == STATE_GAME) {
        if(input->type == InputTypePress || input->type == InputTypeRepeat) {
            int dx = 0, dy = 0;
            bool is_press = (input->type == InputTypePress);
            if(input->key == InputKeyUp) {
                dy = is_press ? -2 : -4;
                if(is_press) {
                    ctx->hold_up_timer = time;
                    ctx->bypass_collision = true;
                }
            } else if(input->key == InputKeyDown) {
                dy = is_press ? 2 : 4;
                if(is_press) {
                    ctx->hold_down_timer = time;
                    ctx->bypass_collision = true;
                }
            } else if(input->key == InputKeyLeft) {
                dx = is_press ? -2 : -4;
                if(is_press) {
                    ctx->hold_left_timer = time;
                    ctx->bypass_collision = true;
                }
            } else if(input->key == InputKeyRight) {
                dx = is_press ? 2 : 4;
                if(is_press) {
                    ctx->hold_right_timer = time;
                    ctx->bypass_collision = true;
                }
            } else if(input->key == InputKeyOk && !ctx->player_projectile.active) {
                bool hit = false;
                for(int i = 0; i < ctx->entity_count; i++) {
                    Entity* e = &ctx->entities[i];
                    if(abs(ctx->player.x - e->x) <= 3 && abs(ctx->player.y - e->y) <= 3) {
                        int damage = (ctx->player.double_power_timer > time) ? 2 : 1;
                        e->health_bars -= damage;
                        furi_hal_vibro_on(true);
                        furi_delay_ms(20);
                        furi_hal_vibro_on(false);
                        hit = true;
                        if(e->health_bars <= 0) {
                            ctx->player.power_points +=
                                (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) ? 25 : 10;
                            check_level_up(ctx);
                            // Drop chance
                            if(rand() % 100 < 50) {
                                ctx->pickups[ctx->pickup_count].type =
                                    (rand() % 4 == 0) ? PICKUP_ALMOND_WATER : PICKUP_CHEST;
                                ctx->pickups[ctx->pickup_count].x = e->x;
                                ctx->pickups[ctx->pickup_count].y = e->y;
                                ctx->pickups[ctx->pickup_count].falling = true;
                                ctx->pickup_count++;
                            } else if(
                                (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) &&
                                rand() % 100 < 10) {
                                ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
                                ctx->pickups[ctx->pickup_count].x = e->x;
                                ctx->pickups[ctx->pickup_count].y =
                                    PLAY_HEIGHT - 7; // Place on ground
                                ctx->pickups[ctx->pickup_count].falling = false;
                                ctx->pickup_count++;
                            }
                            if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                                ctx->final_boss_timer = time;
                                if(e->type == ENTITY_FINAL_BOSS) {
                                    log_event("Final boss defeated");
                                }
                            }
                            // Remove entity
                            for(int j = i; j < ctx->entity_count - 1; j++) {
                                ctx->entities[j] = ctx->entities[j + 1];
                            }
                            ctx->entity_count--;
                            i--;
                        }
                    }
                }
                if(!hit) {
                    ctx->player_projectile.active = true;
                    ctx->player_projectile.x =
                        ctx->player.x + (ctx->player.facing_right ? ctx->player.width : -2);
                    ctx->player_projectile.y = ctx->player.y + ctx->player.height / 2;
                    ctx->player_projectile.dx = ctx->player.facing_right ? 2 : -2;
                    ctx->player_projectile.dy = 0;
                    ctx->player_projectile.from_player = true;
                    furi_hal_vibro_on(true);
                    furi_delay_ms(20);
                    furi_hal_vibro_on(false);
                }
            } else if(input->key == InputKeyBack) {
                if(ctx->hold_back_timer == 0) ctx->hold_back_timer = time;
            }
            if(dx != 0 || dy != 0) {
                if(ctx->player_projectile.active) {
                    ctx->player_projectile.dx = (dx != 0) ? dx * 2 : ctx->player_projectile.dx;
                    ctx->player_projectile.dy = (dy != 0) ? dy * 2 : ctx->player_projectile.dy;
                } else {
                    int new_x = ctx->player.x + dx;
                    int new_y = ctx->player.y + dy;
                    if(ctx->bypass_collision) {
                        if(new_x >= 0 && new_x + ctx->player.width <= WORLD_WIDTH)
                            ctx->player.x = new_x;
                        if(new_y >= 9 && new_y + ctx->player.height <= PLAY_HEIGHT)
                            ctx->player.y = new_y;
                        if(dx != 0) ctx->player.facing_right = (dx > 0);
                    } else {
                        if(new_x >= 0 && new_x + ctx->player.width <= WORLD_WIDTH && new_y >= 9 &&
                           new_y + ctx->player.height <= PLAY_HEIGHT &&
                           !collides_with_wall(
                               new_x, new_y, ctx->player.width, ctx->player.height, ctx) &&
                           !collides_with_furniture(
                               new_x, new_y, ctx->player.width, ctx->player.height, ctx) &&
                           (dy <= 0 ||
                            can_move_down(
                                new_x, new_y, ctx->player.width, ctx->player.height, ctx))) {
                            ctx->player.x = new_x;
                            ctx->player.y = new_y;
                            if(dx != 0) ctx->player.facing_right = (dx > 0);
                        }
                    }
                }
            }
            // Door interaction
            if(input->key == InputKeyUp) {
                for(int i = 0; i < ctx->pickup_count; i++) {
                    if(ctx->pickups[i].type == PICKUP_DOOR) {
                        int dx =
                            abs(ctx->player.x + ctx->player.width / 2 - (ctx->pickups[i].x + 3));
                        if(dx < 3 &&
                           abs(ctx->player.y + ctx->player.height - (ctx->pickups[i].y + 7)) < 1) {
                            canvas_set_color(ctx->canvas, ColorBlack);
                            canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            view_port_update(furi_record_open(RECORD_GUI));
                            log_event("Door used, regenerating world");
                            generate_world(ctx, true);
                            break;
                        }
                    }
                }
            }
        } else if(input->type == InputTypeRelease) {
            if(input->key == InputKeyUp) {
                ctx->hold_up_timer = 0;
                ctx->bypass_collision = false;
            } else if(input->key == InputKeyDown) {
                ctx->hold_down_timer = 0;
                ctx->bypass_collision = false;
            } else if(input->key == InputKeyLeft) {
                ctx->hold_left_timer = 0;
                ctx->bypass_collision = false;
            } else if(input->key == InputKeyRight) {
                ctx->hold_right_timer = 0;
                ctx->bypass_collision = false;
            } else if(input->key == InputKeyBack) {
                if(time - ctx->hold_back_timer >= 3000) {
                    log_event("Entering STATE_EXIT");
                    if(save_array(WMA_FILE, ctx->world_map, ARRAY_SIZE) ||
                       save_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE) ||
                       save_settings(ctx) || save_player_data(ctx)) {
                        // Save succeeded
                    }
                    ctx->state = STATE_EXIT;
                } else {
                    ctx->state = STATE_PAUSED;
                    ctx->final_boss_timer = time; // Reset boss timer
                }
                ctx->hold_back_timer = 0;
            }
        }
    } else if(ctx->state == STATE_PAUSED) {
        if(input->type == InputTypePress &&
           (input->key == InputKeyOk || input->key == InputKeyBack)) {
            ctx->state = STATE_GAME;
        }
    } else if(ctx->state == STATE_DEATH) {
        if(time - ctx->last_frame_time > 2000) {
            log_event("Exiting STATE_DEATH");
            ctx->player.health_bars = 2;
            ctx->player.x = rand() % (WORLD_WIDTH - ctx->player.width);
            ctx->player.y = rand() % (PLAY_HEIGHT - 15) + 9;
            ctx->in_iframes = true;
            ctx->iframe_timer = time + 2000;
            ctx->state = STATE_GAME;
        }
    }
}
// Update game state
static void update_game(GameContext* ctx) {
    if(!ctx) return;
    static int final_boss_health = 10;
    uint64_t time = furi_get_tick();
    if(time - ctx->last_fps_check > 1000) {
        ctx->fps = (float)ctx->frame_count / ((time - ctx->last_fps_check) / 1000.0f);
        ctx->last_fps_check = time;
        ctx->frame_count = 0;
    }
    if(ctx->state == STATE_TITLE) {
        if(time - ctx->last_frame_time > 2000) {
            ctx->state = STATE_GAME;
            generate_world(ctx, false);
            view_port_update(furi_record_open(RECORD_GUI));
        }
    } else if(ctx->state == STATE_GAME) {
        if(time - ctx->last_input_time > 180000) {
            ctx->final_boss_timer = time; // Reset boss timer instead of pausing
        }
        if(time - ctx->day_night_timer > (ctx->is_night ? 78000 : 102000)) {
            ctx->is_night = !ctx->is_night;
            ctx->day_night_timer = time;
            ctx->sun_moon_x = 0;
        }
        ctx->sun_moon_x += 1;
        if(ctx->sun_moon_x > WORLD_WIDTH) ctx->sun_moon_x = 0;
        if(time - ctx->weather_timer > 60000ULL + (uint64_t)(rand() % 120000)) {
            ctx->weather_active = !ctx->weather_active;
            ctx->weather_timer = time;
            ctx->weather_duration = 30000 + rand() % 60000;
            ctx->weather_type = rand() % 4;
        }
        if(ctx->weather_active && time - ctx->weather_timer > ctx->weather_duration) {
            ctx->weather_active = false;
            ctx->weather_type = 0;
        }
        // Update pickups
        for(int i = 0; i < ctx->pickup_count; i++) {
            if(ctx->pickups[i].falling) {
                bool on_platform = false;
                for(int j = 0; j < ctx->platform_count; j++) {
                    if(collides_with_platform(
                           ctx->pickups[i].x, ctx->pickups[i].y + 1, 1, 1, &ctx->platforms[j])) {
                        ctx->pickups[i].y = ctx->platforms[j].y - 1;
                        on_platform = true;
                        break;
                    }
                }
                if(!on_platform) {
                    ctx->pickups[i].y += 1;
                    if(ctx->pickups[i].y > PLAY_HEIGHT) {
                        ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                        i--;
                        continue;
                    }
                    // Auto-use if hits player or final boss
                    if(ctx->pickups[i].type == PICKUP_ALMOND_WATER ||
                       ctx->pickups[i].type == PICKUP_CHEST) {
                        if(abs(ctx->pickups[i].x - ctx->player.x) < ctx->player.width &&
                           abs(ctx->pickups[i].y - ctx->player.y) < ctx->player.height) {
                            if(ctx->pickups[i].type == PICKUP_ALMOND_WATER &&
                               ctx->almond_water_count < 3) {
                                ctx->almond_water_count++;
                                furi_hal_vibro_on(true);
                                furi_delay_ms(20);
                                furi_hal_vibro_on(false);
                            } else if(ctx->pickups[i].type == PICKUP_CHEST) {
                                int roll = rand() % 5;
                                if(roll == 0 && ctx->almond_water_count < 3)
                                    ctx->almond_water_count++;
                                else if(roll == 1 && ctx->entity_count < 4)
                                    ctx->entity_count++;
                                else if(roll == 2)
                                    ctx->entities[2].health_bars = 10 + ctx->player.power_level;
                                else if(roll != 3) {
                                    ctx->player.power_points += 10;
                                    check_level_up(ctx);
                                }
                                furi_hal_vibro_on(true);
                                furi_delay_ms(20);
                                furi_hal_vibro_on(false);
                            }
                            ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                            i--;
                            continue;
                        }
                        for(int j = 0; j < ctx->entity_count; j++) {
                            if(ctx->entities[j].type == ENTITY_FINAL_BOSS &&
                               abs(ctx->pickups[i].x - ctx->entities[j].x) <
                                   ctx->entities[j].width &&
                               abs(ctx->pickups[i].y - ctx->entities[j].y) <
                                   ctx->entities[j].height) {
                                if(ctx->pickups[i].type == PICKUP_ALMOND_WATER) {
                                    ctx->entities[j].health_bars++;
                                } else if(ctx->pickups[i].type == PICKUP_CHEST) {
                                    ctx->entities[j].double_power_timer = time + 3000;
                                    ctx->entities[j].pickup_cooldown = time + 6000;
                                }
                                ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                                i--;
                                break;
                            }
                        }
                    }
                } else {
                    ctx->pickups[i].falling = false;
                }
            }
        }
        // Player pickup interaction
        for(int i = 0; i < ctx->pickup_count; i++) {
            Pickup* pk = &ctx->pickups[i];
            if(!pk->falling && abs(ctx->player.x - pk->x) < ctx->player.width &&
               abs(ctx->player.y - pk->y) < ctx->player.height) {
                if(pk->type == PICKUP_ALMOND_WATER && ctx->almond_water_count < 3) {
                    ctx->almond_water_count++;
                    furi_hal_vibro_on(true);
                    furi_delay_ms(20);
                    furi_hal_vibro_on(false);
                } else if(pk->type == PICKUP_CHEST) {
                    int roll = rand() % 5;
                    if(roll == 0 && ctx->almond_water_count < 3)
                        ctx->almond_water_count++;
                    else if(roll == 1 && ctx->entity_count < 4)
                        ctx->entity_count++;
                    else if(roll == 2)
                        ctx->entities[2].health_bars = 10 + ctx->player.power_level;
                    else if(roll != 3) {
                        ctx->player.power_points += 10;
                        check_level_up(ctx);
                    }
                    furi_hal_vibro_on(true);
                    furi_delay_ms(20);
                    furi_hal_vibro_on(false);
                }
                ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                i--;
            }
        }
        // Auto-use almond water
        if(ctx->player.health_bars <= 1 && ctx->almond_water_count > 0) {
            ctx->player.health_bars++;
            ctx->almond_water_count--;
            furi_hal_vibro_on(true);
            furi_delay_ms(20);
            furi_hal_vibro_on(false);
        }
        // Entity updates
        for(int i = 0; i < ctx->entity_count; i++) {
            Entity* e = &ctx->entities[i];
            int dx = (rand() % 3) - 1;
            int dy = (rand() % 3) - 1;
            int dist = abs(e->x - ctx->player.x) + abs(e->y - ctx->player.y);
            if(dist <= 8) {
                dx = (ctx->player.x > e->x) ? 1 : ((ctx->player.x < e->x) ? -1 : 0);
                dy = (ctx->player.y > e->y) ? 1 : ((ctx->player.y < e->y) ? -1 : 0);
            }
            int new_x = e->x + dx;
            int new_y = e->y + dy;
            if(new_x >= 0 && new_x + e->width <= WORLD_WIDTH && new_y >= 9 &&
               new_y + e->height <= PLAY_HEIGHT &&
               !collides_with_wall(new_x, new_y, e->width, e->height, ctx) &&
               !collides_with_furniture(new_x, new_y, e->width, e->height, ctx) &&
               (dy <= 0 || can_move_down(new_x, new_y, e->width, e->height, ctx))) {
                e->x = new_x;
                e->y = new_y;
                if(dx != 0) e->facing_right = (dx > 0);
            }
            if((e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) &&
               time > e->pickup_cooldown) {
                for(int j = 0; j < ctx->pickup_count; j++) {
                    if(!ctx->pickups[j].falling && abs(e->x - ctx->pickups[j].x) < e->width &&
                       abs(e->y - ctx->pickups[j].y) < e->height) {
                        if(ctx->pickups[j].type == PICKUP_ALMOND_WATER) {
                            e->health_bars++;
                        } else if(ctx->pickups[j].type == PICKUP_CHEST) {
                            e->double_power_timer = time + 3000;
                            e->pickup_cooldown = time + 6000;
                        }
                        ctx->pickups[j] = ctx->pickups[--ctx->pickup_count];
                        j--;
                    }
                }
            }
            if(rand() % 100 < 5) {
                if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                    if(ctx->enemy_projectile_count < 2) {
                        Projectile* proj = &ctx->enemy_projectiles[ctx->enemy_projectile_count++];
                        proj->active = true;
                        proj->x = e->x + (e->facing_right ? e->width : -2);
                        proj->y = e->y + e->height / 2;
                        proj->dx = e->facing_right ? 2 : -2;
                        proj->dy = 0;
                        proj->from_player = false;
                    }
                } else {
                    if(ctx->enemy_projectile_count < 1) {
                        Projectile* proj = &ctx->enemy_projectiles[0];
                        proj->active = true;
                        proj->x = e->x + (e->facing_right ? e->width : -2);
                        proj->y = e->y + e->height / 2;
                        proj->dx = e->facing_right ? 2 : -2;
                        proj->dy = 0;
                        proj->from_player = false;
                        ctx->enemy_projectile_count = 1;
                    }
                }
            }
            if(!ctx->in_iframes && abs(e->x - ctx->player.x) < e->width &&
               abs(e->y - ctx->player.y) < e->height) {
                ctx->player.health_bars--;
                ctx->in_iframes = true;
                ctx->iframe_timer = time + 230;
                furi_hal_vibro_on(true);
                furi_delay_ms(20);
                furi_hal_vibro_on(false);
                if(ctx->player_projectile.active) {
                    ctx->player.health_bars = 2;
                    ctx->special_active = true;
                    ctx->special_timer = time + 2000;
                    ctx->in_iframes = true;
                    ctx->iframe_timer = time + 2000;
                }
                if(ctx->player.health_bars <= 0) {
                    log_event("Entering STATE_DEATH");
                    ctx->state = STATE_DEATH;
                    ctx->last_frame_time = time;
                }
            }
            // Check for entity death
            if(e->health_bars <= 0) {
                ctx->player.power_points +=
                    (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) ? 25 : 10;
                check_level_up(ctx);
                // Drop chance
                if(rand() % 100 < 50) {
                    ctx->pickups[ctx->pickup_count].type =
                        (rand() % 4 == 0) ? PICKUP_ALMOND_WATER : PICKUP_CHEST;
                    ctx->pickups[ctx->pickup_count].x = e->x;
                    ctx->pickups[ctx->pickup_count].y = e->y;
                    ctx->pickups[ctx->pickup_count].falling = true;
                    ctx->pickup_count++;
                } else if(
                    (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) &&
                    rand() % 100 < 10) {
                    ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
                    ctx->pickups[ctx->pickup_count].x = e->x;
                    ctx->pickups[ctx->pickup_count].y = PLAY_HEIGHT - 7; // Place on ground
                    ctx->pickups[ctx->pickup_count].falling = false;
                    ctx->pickup_count++;
                }
                if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                    ctx->final_boss_timer = time;
                    if(e->type == ENTITY_FINAL_BOSS) {
                        log_event("Final boss defeated");
                        final_boss_health =
                            (int)(final_boss_health * 1.1); // Increase health by 10%
                    }
                }
                // Remove entity
                for(int j = i; j < ctx->entity_count - 1; j++) {
                    ctx->entities[j] = ctx->entities[j + 1];
                }
                ctx->entity_count--;
                i--;
            }
        }
        // Respawn enemies if all are dead
        if(ctx->entity_count == 0) {
            int spawn_count = 1 + (rand() % 2); // 1 or 2 enemies
            for(int i = 0; i < spawn_count && ctx->entity_count < 4; i++) {
                Entity* e = &ctx->entities[ctx->entity_count];
                e->type = (rand() % 100 < 20) ? ENTITY_BOSS : ENTITY_ENEMY;
                e->x = rand() % (WORLD_WIDTH - (e->type == ENTITY_BOSS ? 3 : 4));
                e->y = rand() % (PLAY_HEIGHT - 15) + 9;
                e->width = (e->type == ENTITY_BOSS) ? 3 : 4;
                e->height = (e->type == ENTITY_BOSS) ? 6 : 4;
                e->health_bars = (e->type == ENTITY_BOSS) ? 10 + ctx->player.power_level :
                                                            5 + ctx->player.power_level;
                e->power_points = 0;
                e->power_level = 0;
                e->facing_right = true;
                e->double_power_timer = 0;
                e->pickup_cooldown = 0;
                // Move to platform if below ground
                if(e->y + e->height > PLAY_HEIGHT - 6) {
                    bool placed = false;
                    for(int j = 0; j < ctx->platform_count; j++) {
                        if(e->x + e->width > ctx->platforms[j].x + 1 &&
                           e->x < ctx->platforms[j].x + ctx->platforms[j].width - 1) {
                            e->y = ctx->platforms[j].y - e->height;
                            placed = true;
                            break;
                        }
                    }
                    if(!placed) {
                        e->y = PLAY_HEIGHT - e->height - 6; // Place on ground
                    }
                }
                ctx->entity_count++;
            }
            log_event("Respawned enemies");
        }
        // Projectile updates
        if(ctx->player_projectile.active) {
            ctx->player_projectile.x += ctx->player_projectile.dx;
            ctx->player_projectile.y += ctx->player_projectile.dy;
            if(ctx->player_projectile.x < 0 || ctx->player_projectile.x >= WORLD_WIDTH ||
               ctx->player_projectile.y < 9 || ctx->player_projectile.y >= PLAY_HEIGHT) {
                ctx->player_projectile.active = false;
            } else {
                for(int i = 0; i < ctx->platform_count; i++) {
                    if(collides_with_platform(
                           ctx->player_projectile.x,
                           ctx->player_projectile.y,
                           2,
                           2,
                           &ctx->platforms[i])) {
                        ctx->player_projectile.active = false;
                        break;
                    }
                }
                for(int i = 0; i < ctx->entity_count; i++) {
                    Entity* e = &ctx->entities[i];
                    if(abs(ctx->player_projectile.x - e->x) < e->width &&
                       abs(ctx->player_projectile.y - e->y) < e->height) {
                        int damage = (ctx->player.double_power_timer > time) ? 2 : 1;
                        e->health_bars -= damage;
                        ctx->player_projectile.active = false;
                        furi_hal_vibro_on(true);
                        furi_delay_ms(20);
                        furi_hal_vibro_on(false);
                        if(e->health_bars <= 0) {
                            ctx->player.power_points +=
                                (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) ? 25 : 10;
                            check_level_up(ctx);
                            // Drop chance
                            if(rand() % 100 < 50) {
                                ctx->pickups[ctx->pickup_count].type =
                                    (rand() % 4 == 0) ? PICKUP_ALMOND_WATER : PICKUP_CHEST;
                                ctx->pickups[ctx->pickup_count].x = e->x;
                                ctx->pickups[ctx->pickup_count].y = e->y;
                                ctx->pickups[ctx->pickup_count].falling = true;
                                ctx->pickup_count++;
                            } else if(
                                (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) &&
                                rand() % 100 < 10) {
                                ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
                                ctx->pickups[ctx->pickup_count].x = e->x;
                                ctx->pickups[ctx->pickup_count].y =
                                    PLAY_HEIGHT - 7; // Place on ground
                                ctx->pickups[ctx->pickup_count].falling = false;
                                ctx->pickup_count++;
                            }
                            if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                                ctx->final_boss_timer = time;
                                if(e->type == ENTITY_FINAL_BOSS) {
                                    log_event("Final boss defeated");
                                    final_boss_health =
                                        (int)(final_boss_health * 1.1); // Increase health by 10%
                                }
                            }
                            // Remove entity
                            for(int j = i; j < ctx->entity_count - 1; j++) {
                                ctx->entities[j] = ctx->entities[j + 1];
                            }
                            ctx->entity_count--;
                            i--;
                        }
                    }
                }
            }
        }
        for(int i = 0; i < ctx->enemy_projectile_count; i++) {
            if(ctx->enemy_projectiles[i].active) {
                ctx->enemy_projectiles[i].x += ctx->enemy_projectiles[i].dx;
                ctx->enemy_projectiles[i].y += ctx->enemy_projectiles[i].dy;
                if(ctx->enemy_projectiles[i].x < 0 || ctx->enemy_projectiles[i].x >= WORLD_WIDTH ||
                   ctx->enemy_projectiles[i].y < 9 || ctx->enemy_projectiles[i].y >= PLAY_HEIGHT) {
                    ctx->enemy_projectiles[i].active = false;
                } else {
                    for(int j = 0; j < ctx->platform_count; j++) {
                        if(collides_with_platform(
                               ctx->enemy_projectiles[i].x,
                               ctx->enemy_projectiles[i].y,
                               2,
                               2,
                               &ctx->platforms[j])) {
                            ctx->enemy_projectiles[i].active = false;
                            break;
                        }
                    }
                    if(!ctx->in_iframes &&
                       abs(ctx->enemy_projectiles[i].x - ctx->player.x) < ctx->player.width &&
                       abs(ctx->enemy_projectiles[i].y - ctx->player.y) < ctx->player.height) {
                        ctx->player.health_bars--;
                        ctx->enemy_projectiles[i].active = false;
                        ctx->in_iframes = true;
                        ctx->iframe_timer = time + 230;
                        furi_hal_vibro_on(true);
                        furi_delay_ms(20);
                        furi_hal_vibro_on(false);
                        if(ctx->player_projectile.active) {
                            ctx->player.health_bars = 2;
                            ctx->special_active = true;
                            ctx->special_timer = time + 2000;
                            ctx->in_iframes = true;
                            ctx->iframe_timer = time + 2000;
                        }
                        if(ctx->player.health_bars <= 0) {
                            log_event("Entering STATE_DEATH");
                            ctx->state = STATE_DEATH;
                            ctx->last_frame_time = time;
                        }
                    }
                }
            }
        }
        // Door timer
        if(ctx->pickup_count == 0 && time - ctx->door_timer > 90000 &&
           time - ctx->door_cooldown > 180000) {
            for(int i = 1; i < ctx->platform_count; i++) { // Skip ground platform
                if(rand() % 100 < 50) {
                    ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
                    ctx->pickups[ctx->pickup_count].x =
                        ctx->platforms[i].x + (ctx->platforms[i].width - 6) / 2;
                    ctx->pickups[ctx->pickup_count].y =
                        ctx->platforms[i].y - 7; // Place on platform
                    ctx->pickups[ctx->pickup_count].falling = false;
                    ctx->pickup_count++;
                    ctx->door_timer = time;
                    ctx->door_cooldown = time + 30000; // Door stays for 30s
                    break;
                }
            }
        }
        // Remove expired doors
        for(int i = 0; i < ctx->pickup_count; i++) {
            if(ctx->pickups[i].type == PICKUP_DOOR && time - ctx->door_timer > 30000) {
                ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                ctx->door_cooldown = time;
                i--;
            }
        }
        // Timers
        if(ctx->in_iframes && time > ctx->iframe_timer) {
            ctx->in_iframes = false;
        }
        if(ctx->special_active && time > ctx->special_timer) {
            ctx->special_active = false;
        }
        if(time - ctx->final_boss_timer > 300000 && ctx->oset[3] == '1') {
            ctx->oset[3] = 'z';
            ctx->entities[2].type = ENTITY_FINAL_BOSS;
            ctx->entities[2].health_bars = final_boss_health;
            ctx->entities[2].x = ctx->player.x + (ctx->player.facing_right ? 20 : -20);
            ctx->entities[2].y = ctx->player.y;
            ctx->entities[2].width = 5;
            ctx->entities[2].height = 5;
            log_event("Final boss spawned");
        }
    }
    ctx->last_frame_time = time;
    ctx->frame_count++;
}

// Render callback
static void render_callback(Canvas* canvas, void* context) {
    GameContext* ctx = context;
    if(!ctx || !canvas) return;
    ctx->canvas = canvas;
    if(ctx->state == STATE_TITLE) {
        draw_title(ctx, canvas);
    } else if(ctx->state == STATE_GAME) {
        draw_game(ctx, canvas);
    } else if(ctx->state == STATE_PAUSED) {
        draw_pause(ctx, canvas);
    } else if(ctx->state == STATE_DEATH) {
        draw_death(ctx, canvas);
    }
}

// Input callback
static void input_callback(InputEvent* input, void* context) {
    GameContext* ctx = context;
    if(!ctx || !input) return;
    ctx->input = *input;
    handle_input(ctx, input);
}

// Main entry point
int32_t grokadven_app(void* p) {
    UNUSED(p);
    GameContext* ctx = malloc(sizeof(GameContext));
    if(!ctx) {
        log_event("Failed to allocate GameContext");
        return -1;
    }
    game_init(ctx);

    ViewPort* view_port = view_port_alloc();
    if(!view_port) {
        log_event("Failed to allocate ViewPort");
        free(ctx);
        return -1;
    }
    view_port_draw_callback_set(view_port, render_callback, ctx);
    view_port_input_callback_set(view_port, input_callback, ctx);

    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        log_event("Failed to open GUI");
        view_port_free(view_port);
        free(ctx);
        return -1;
    }
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(ctx->state != STATE_EXIT) {
        update_game(ctx);
        view_port_update(view_port);
        furi_delay_ms(1000 / 17); // 17 FPS
    }

    // Cleanup
    log_event("Initiating cleanup");
    furi_hal_vibro_on(false); // Ensure vibration is off
    furi_delay_ms(100);
    if(ctx->canvas) {
        canvas_set_color(ctx->canvas, ColorBlack);
        canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        view_port_update(view_port);
        furi_delay_ms(100);
    }
    if(save_array(WMA_FILE, ctx->world_map, ARRAY_SIZE)) {
        log_event("Saved world map");
    }
    furi_delay_ms(100);
    if(save_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE)) {
        log_event("Saved world objects");
    }
    furi_delay_ms(100);
    if(save_settings(ctx)) {
        log_event("Saved settings");
    }
    furi_delay_ms(100);
    if(save_player_data(ctx)) {
        log_event("Saved player data");
    }
    furi_delay_ms(100);
    view_port_enabled_set(view_port, false);
    furi_delay_ms(100);
    gui_remove_view_port(gui, view_port);
    furi_delay_ms(100);
    view_port_free(view_port);
    furi_delay_ms(100);
    furi_record_close(RECORD_GUI);
    furi_delay_ms(100);
    furi_record_close(RECORD_STORAGE);
    furi_delay_ms(100);
    free(ctx);
    log_event("Cleanup complete");
    furi_delay_ms(500);
    return 0;
}
