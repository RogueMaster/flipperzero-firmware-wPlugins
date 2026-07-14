/*
 * Race Game v4.0 for Flipper Zero
 * Features: Multiple obstacles, power-ups, combo, boss truck,
 *           crash particles, difficulty, night mode, high score.
 */

#include <furi.h>
#include <furi_hal_speaker.h>
#include <furi_hal_vibro.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

// ─── Layout ─────────────────────────────────────────────────────────────────
#define SCREEN_W       64
#define SCREEN_H       128
#define ROAD_LEFT      10
#define ROAD_RIGHT     53
#define ROAD_WIDTH     (ROAD_RIGHT - ROAD_LEFT)
#define LANE_COUNT     3
#define LANE_WIDTH     (ROAD_WIDTH / LANE_COUNT)
#define CAR_W          10
#define CAR_H          13
#define PLAYER_Y       105
#define MAX_OBS        5
#define MAX_COINS      3
#define MAX_POWERUPS   2
#define MAX_SCENERY    6
#define MAX_PARTICLES  12
#define INITIAL_LIVES  3
#define MAX_LIVES      5
#define DASH_LEN       8
#define DASH_GAP       8
#define DASH_TOTAL     (DASH_LEN + DASH_GAP)
#define HIGHSCORE_PATH APP_DATA_PATH("highscore.dat")

// ─── Types ──────────────────────────────────────────────────────────────────

typedef enum {
    StateMenu,
    StatePlaying,
    StateGameOver
} GameState;
typedef enum {
    ObsMoto,
    ObsSedan,
    ObsTruck
} ObsType; // 0=narrow/fast, 1=normal, 2=boss(2 lanes)
typedef enum {
    PwShield,
    PwMagnet,
    PwFuel
} PowerUpType;
typedef enum {
    DiffEasy,
    DiffNormal,
    DiffHard
} Difficulty;

typedef struct {
    int8_t lane;
    int16_t y;
    bool alive;
    ObsType type;
} Obstacle;
typedef struct {
    int8_t lane;
    int16_t y;
    bool alive;
} Coin;
typedef struct {
    int8_t lane;
    int16_t y;
    bool alive;
    PowerUpType type;
} PowerUp;
typedef struct {
    int16_t y;
    int8_t side;
    int8_t type;
} Scenery;
typedef struct {
    int16_t x;
    int16_t y;
    int8_t dx;
    int8_t dy;
    uint8_t life;
} Particle;

typedef struct {
    GameState state;
    int8_t player_lane;
    uint32_t score;
    uint32_t high_score;
    uint8_t level;
    uint8_t lives;
    uint16_t speed;
    uint32_t tick_count;
    int8_t road_scroll;
    bool night_mode;
    bool sound_on;
    int8_t menu_idx;
    Difficulty difficulty;
    uint8_t invincible_ticks;
    uint8_t shield_ticks;
    uint8_t magnet_ticks;
    uint8_t combo;
    uint8_t combo_display; // ticks to show combo text
    Obstacle obstacles[MAX_OBS];
    Coin coins[MAX_COINS];
    PowerUp powerups[MAX_POWERUPS];
    Scenery scenery[MAX_SCENERY];
    Particle particles[MAX_PARTICLES];
    FuriTimer* timer;
} RaceGameState;

typedef enum {
    EventTypeTick,
    EventTypeKey
} EventType;
typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

// ─── Difficulty Settings ────────────────────────────────────────────────────
static const uint16_t diff_speed[] = {140, 120, 90};
static const uint16_t diff_min_speed[] = {70, 50, 35};
static const uint8_t diff_spawn[] = {15, 12, 9};

// ─── High Score ─────────────────────────────────────────────────────────────

static void load_high_score(RaceGameState* s) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    s->high_score = 0;
    if(storage_file_open(f, HIGHSCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING))
        storage_file_read(f, &s->high_score, sizeof(uint32_t));
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void save_high_score(RaceGameState* s) {
    if(s->score <= s->high_score) return;
    s->high_score = s->score;
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, HIGHSCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, &s->high_score, sizeof(uint32_t));
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// ─── Sound & Vibration ──────────────────────────────────────────────────────

static void play_sound(RaceGameState* s, float freq, float vol, uint32_t ms) {
    if(!s->sound_on) return;
    if(furi_hal_speaker_acquire(500)) {
        furi_hal_speaker_start(freq, vol);
        furi_delay_ms(ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void vibrate(uint32_t ms) {
    furi_hal_vibro_on(true);
    furi_delay_ms(ms);
    furi_hal_vibro_on(false);
}

// ─── Helpers ────────────────────────────────────────────────────────────────

static int16_t lane_cx(int8_t lane) {
    return ROAD_LEFT + LANE_WIDTH / 2 + lane * LANE_WIDTH;
}
static int16_t car_lx(int8_t lane) {
    return lane_cx(lane) - CAR_W / 2;
}

// ─── Particles ──────────────────────────────────────────────────────────────

static void spawn_particles(RaceGameState* s, int16_t cx, int16_t cy) {
    for(int i = 0; i < MAX_PARTICLES; i++) {
        s->particles[i].x = cx;
        s->particles[i].y = cy;
        s->particles[i].dx = (rand() % 7) - 3;
        s->particles[i].dy = (rand() % 7) - 3;
        s->particles[i].life = 8 + (rand() % 5);
    }
}

static void update_particles(RaceGameState* s) {
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(s->particles[i].life > 0) {
            s->particles[i].x += s->particles[i].dx;
            s->particles[i].y += s->particles[i].dy;
            s->particles[i].life--;
        }
    }
}

static void draw_particles(Canvas* canvas, RaceGameState* s) {
    uint8_t fg = s->night_mode ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(s->particles[i].life > 0) {
            int16_t px = s->particles[i].x;
            int16_t py = s->particles[i].y;
            if(px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                canvas_draw_dot(canvas, px, py);
                if(s->particles[i].life > 4 && px + 1 < SCREEN_W)
                    canvas_draw_dot(canvas, px + 1, py);
            }
        }
    }
}

// ─── Drawing: Cars ──────────────────────────────────────────────────────────

static void draw_player_car(Canvas* canvas, int16_t x, int16_t y, bool night, bool shield) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    uint8_t bg = night ? ColorBlack : ColorWhite;

    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x + 3, y, 4, 3);
    canvas_draw_frame(canvas, x + 2, y + 3, 6, 4);
    canvas_draw_box(canvas, x + 2, y + 7, 6, 4);
    canvas_draw_box(canvas, x + 1, y + 11, 8, 2);
    canvas_draw_box(canvas, x, y + 2, 2, 3);
    canvas_draw_box(canvas, x + 8, y + 2, 2, 3);
    canvas_draw_box(canvas, x, y + 8, 2, 3);
    canvas_draw_box(canvas, x + 8, y + 8, 2, 3);

    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, x + 3, y + 4, 4, 2);
    canvas_set_color(canvas, fg);
    canvas_draw_dot(canvas, x + 4, y + 5);
    canvas_draw_dot(canvas, x + 5, y + 5);

    // Shield aura
    if(shield) {
        canvas_draw_frame(canvas, x - 2, y - 2, CAR_W + 4, CAR_H + 4);
    }
}

static void draw_moto(Canvas* canvas, int16_t x, int16_t y, bool night) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);
    // Narrow bike shape
    canvas_draw_box(canvas, x + 4, y, 2, 10); // Body
    canvas_draw_box(canvas, x + 3, y + 1, 4, 2); // Handlebars
    canvas_draw_box(canvas, x + 3, y + 7, 4, 2); // Rear
    canvas_draw_dot(canvas, x + 4, y + 3); // Rider head
    canvas_draw_dot(canvas, x + 5, y + 3);
}

static void draw_sedan(Canvas* canvas, int16_t x, int16_t y, bool night) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    uint8_t bg = night ? ColorBlack : ColorWhite;

    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x + 1, y, 8, 12);
    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, x + 2, y + 2, 6, 3); // Window
    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x, y + 2, 1, 3);
    canvas_draw_box(canvas, x + 9, y + 2, 1, 3);
    canvas_draw_box(canvas, x, y + 9, 1, 3);
    canvas_draw_box(canvas, x + 9, y + 9, 1, 3);
}

static void draw_boss_truck(Canvas* canvas, int16_t x, int16_t y, bool night) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    uint8_t bg = night ? ColorBlack : ColorWhite;

    // Wide truck spanning ~2 lanes
    canvas_set_color(canvas, fg);
    canvas_draw_box(canvas, x, y, 20, 16);

    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, x + 2, y + 10, 7, 3); // Left window
    canvas_draw_box(canvas, x + 11, y + 10, 7, 3); // Right window
    canvas_draw_box(canvas, x + 3, y + 2, 14, 6); // Cargo

    canvas_set_color(canvas, fg);
    // Wheels
    canvas_draw_box(canvas, x - 1, y + 2, 2, 3);
    canvas_draw_box(canvas, x + 19, y + 2, 2, 3);
    canvas_draw_box(canvas, x - 1, y + 11, 2, 3);
    canvas_draw_box(canvas, x + 19, y + 11, 2, 3);
}

static void draw_obstacle(Canvas* canvas, Obstacle* obs, bool night) {
    int16_t x = car_lx(obs->lane);
    switch(obs->type) {
    case ObsMoto:
        draw_moto(canvas, x, obs->y, night);
        break;
    case ObsSedan:
        draw_sedan(canvas, x, obs->y, night);
        break;
    case ObsTruck:
        // Boss truck centered between two lanes
        draw_boss_truck(canvas, x - 5, obs->y, night);
        break;
    }
}

// ─── Drawing: Coin ──────────────────────────────────────────────────────────

static void draw_coin(Canvas* canvas, int16_t x, int16_t y, bool night, uint32_t tick) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);
    // Animated sparkle
    if(tick % 8 < 4) {
        canvas_draw_circle(canvas, x + 4, y + 4, 3);
        canvas_draw_dot(canvas, x + 4, y + 4);
    } else {
        canvas_draw_circle(canvas, x + 4, y + 4, 3);
        canvas_draw_dot(canvas, x + 3, y + 3);
        canvas_draw_dot(canvas, x + 5, y + 5);
    }
}

// ─── Drawing: Power-Up ──────────────────────────────────────────────────────

static void draw_powerup(Canvas* canvas, int16_t x, int16_t y, PowerUpType type, bool night) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);
    canvas_draw_frame(canvas, x + 1, y, 7, 8);

    switch(type) {
    case PwShield: // S
        canvas_draw_str(canvas, x + 2, y + 7, "S");
        break;
    case PwMagnet: // M
        canvas_draw_str(canvas, x + 2, y + 7, "M");
        break;
    case PwFuel: // +
        canvas_draw_line(canvas, x + 4, y + 2, x + 4, y + 6);
        canvas_draw_line(canvas, x + 2, y + 4, x + 6, y + 4);
        break;
    }
}

// ─── Drawing: Scenery ───────────────────────────────────────────────────────

static void draw_scenery_item(Canvas* canvas, Scenery* sc, bool night) {
    uint8_t fg = night ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);
    int16_t x = (sc->side == 0) ? 1 : ROAD_RIGHT + 3;

    if(sc->type == 0) {
        canvas_draw_dot(canvas, x + 2, sc->y);
        canvas_draw_line(canvas, x + 1, sc->y + 1, x + 3, sc->y + 1);
        canvas_draw_line(canvas, x, sc->y + 2, x + 4, sc->y + 2);
        canvas_draw_dot(canvas, x + 2, sc->y + 3);
        canvas_draw_dot(canvas, x + 2, sc->y + 4);
    } else {
        canvas_draw_box(canvas, x + 2, sc->y, 1, 5);
        canvas_draw_line(canvas, x, sc->y, x + 4, sc->y);
    }
}

// ─── Drawing: Road ──────────────────────────────────────────────────────────

static void draw_road(Canvas* canvas, RaceGameState* s) {
    uint8_t fg = s->night_mode ? ColorWhite : ColorBlack;
    canvas_set_color(canvas, fg);

    canvas_draw_line(canvas, ROAD_LEFT, 0, ROAD_LEFT, SCREEN_H - 1);
    canvas_draw_line(canvas, ROAD_LEFT - 1, 0, ROAD_LEFT - 1, SCREEN_H - 1);
    canvas_draw_line(canvas, ROAD_RIGHT, 0, ROAD_RIGHT, SCREEN_H - 1);
    canvas_draw_line(canvas, ROAD_RIGHT + 1, 0, ROAD_RIGHT + 1, SCREEN_H - 1);

    for(int ld = 1; ld < LANE_COUNT; ld++) {
        int16_t dx = ROAD_LEFT + ld * LANE_WIDTH;
        for(int16_t dy = -DASH_TOTAL + s->road_scroll; dy < SCREEN_H; dy += DASH_TOTAL) {
            int16_t st = dy < 0 ? 0 : dy;
            int16_t en = dy + DASH_LEN - 1;
            if(en >= SCREEN_H) en = SCREEN_H - 1;
            if(st <= en) canvas_draw_line(canvas, dx, st, dx, en);
        }
    }
}

// ─── Drawing: HUD ───────────────────────────────────────────────────────────

static void draw_hud(Canvas* canvas, RaceGameState* s) {
    uint8_t fg = s->night_mode ? ColorWhite : ColorBlack;
    uint8_t bg = s->night_mode ? ColorBlack : ColorWhite;

    char buf[32];
    snprintf(buf, sizeof(buf), "S:%lu L:%u", (unsigned long)s->score, s->level + 1);

    canvas_set_font(canvas, FontSecondary);
    uint16_t w = canvas_string_width(canvas, buf);
    uint16_t x = (SCREEN_W - w) / 2;

    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, x - 3, 0, w + 6, 11);
    canvas_set_color(canvas, fg);
    canvas_draw_frame(canvas, x - 3, 0, w + 6, 11);
    canvas_draw_str(canvas, x, 9, buf);

    // Lives (hearts)
    for(uint8_t i = 0; i < s->lives; i++) {
        int16_t hx = 1;
        int16_t hy = SCREEN_H - 8 - i * 7;
        canvas_draw_dot(canvas, hx + 1, hy);
        canvas_draw_dot(canvas, hx + 3, hy);
        canvas_draw_line(canvas, hx, hy + 1, hx + 4, hy + 1);
        canvas_draw_line(canvas, hx, hy + 2, hx + 4, hy + 2);
        canvas_draw_line(canvas, hx + 1, hy + 3, hx + 3, hy + 3);
        canvas_draw_dot(canvas, hx + 2, hy + 4);
    }

    // Combo display
    if(s->combo_display > 0 && s->combo > 1) {
        char cbuf[8];
        snprintf(cbuf, sizeof(cbuf), "x%u", s->combo);
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, 14, AlignRight, AlignBottom, cbuf);
    }

    // Shield indicator
    if(s->shield_ticks > 0) {
        canvas_draw_str(canvas, ROAD_RIGHT + 3, 20, "S");
    }
    // Magnet indicator
    if(s->magnet_ticks > 0) {
        canvas_draw_str(canvas, ROAD_RIGHT + 3, 30, "M");
    }
}

// ─── Drawing: Menu ──────────────────────────────────────────────────────────

static const char* diff_names[] = {"EASY", "NORMAL", "HARD"};

static void draw_menu(Canvas* canvas, RaceGameState* s) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 15, AlignCenter, AlignBottom, "RACE");
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 28, AlignCenter, AlignBottom, "GAME");

    canvas_set_font(canvas, FontSecondary);

    char hs[24];
    snprintf(hs, sizeof(hs), "Best: %lu", (unsigned long)s->high_score);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 40, AlignCenter, AlignBottom, hs);

    // 4 menu items: Start, Sound, Night, Difficulty
    const char* labels[] = {"START", "SOUND", "NIGHT", "DIFF"};
    for(int i = 0; i < 4; i++) {
        char buf[24];
        if(i == 0)
            snprintf(
                buf,
                sizeof(buf),
                "%sSTART%s",
                i == s->menu_idx ? "> " : "",
                i == s->menu_idx ? " <" : "");
        else if(i == 1)
            snprintf(
                buf,
                sizeof(buf),
                "%sSOUND:%s%s",
                i == s->menu_idx ? ">" : "",
                s->sound_on ? "ON" : "OFF",
                i == s->menu_idx ? "<" : "");
        else if(i == 2)
            snprintf(
                buf,
                sizeof(buf),
                "%sNIGHT:%s%s",
                i == s->menu_idx ? ">" : "",
                s->night_mode ? "ON" : "OFF",
                i == s->menu_idx ? "<" : "");
        else
            snprintf(
                buf,
                sizeof(buf),
                "%s%s%s",
                i == s->menu_idx ? ">" : "",
                diff_names[s->difficulty],
                i == s->menu_idx ? "<" : "");

        UNUSED(labels);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 52 + i * 11, AlignCenter, AlignBottom, buf);
    }

    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 102, AlignCenter, AlignBottom, "OK:Select");
    draw_player_car(canvas, SCREEN_W / 2 - 5, 112, false, false);
}

// ─── Drawing: Game Over ─────────────────────────────────────────────────────

static void draw_game_over(Canvas* canvas, RaceGameState* s) {
    uint8_t fg = s->night_mode ? ColorWhite : ColorBlack;
    uint8_t bg = s->night_mode ? ColorBlack : ColorWhite;

    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, 4, 28, 56, 70);
    canvas_set_color(canvas, fg);
    canvas_draw_frame(canvas, 4, 28, 56, 70);
    canvas_draw_frame(canvas, 5, 29, 54, 68);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 43, AlignCenter, AlignBottom, "GAME");
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 56, AlignCenter, AlignBottom, "OVER");

    canvas_set_font(canvas, FontSecondary);
    char buf[24];
    snprintf(buf, sizeof(buf), "Score: %lu", (unsigned long)s->score);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 68, AlignCenter, AlignBottom, buf);

    if(s->score >= s->high_score && s->score > 0)
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 78, AlignCenter, AlignBottom, "NEW BEST!");
    else {
        snprintf(buf, sizeof(buf), "Best: %lu", (unsigned long)s->high_score);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 78, AlignCenter, AlignBottom, buf);
    }

    snprintf(buf, sizeof(buf), "Combo: x%u", s->combo > 1 ? s->combo : 1);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 88, AlignCenter, AlignBottom, buf);

    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 95, AlignCenter, AlignBottom, "OK: Menu");
}

// ─── Main Draw ──────────────────────────────────────────────────────────────

static void draw_callback(Canvas* canvas, void* ctx) {
    RaceGameState* s = ctx;
    if(!s) return;
    canvas_clear(canvas);

    if(s->night_mode && s->state != StateMenu) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }

    switch(s->state) {
    case StateMenu:
        draw_menu(canvas, s);
        break;

    case StatePlaying:
        for(int i = 0; i < MAX_SCENERY; i++)
            if(s->scenery[i].y > -10 && s->scenery[i].y < SCREEN_H)
                draw_scenery_item(canvas, &s->scenery[i], s->night_mode);

        draw_road(canvas, s);

        for(int i = 0; i < MAX_COINS; i++)
            if(s->coins[i].alive)
                draw_coin(
                    canvas, car_lx(s->coins[i].lane), s->coins[i].y, s->night_mode, s->tick_count);

        for(int i = 0; i < MAX_POWERUPS; i++)
            if(s->powerups[i].alive)
                draw_powerup(
                    canvas,
                    car_lx(s->powerups[i].lane),
                    s->powerups[i].y,
                    s->powerups[i].type,
                    s->night_mode);

        for(int i = 0; i < MAX_OBS; i++)
            if(s->obstacles[i].alive) draw_obstacle(canvas, &s->obstacles[i], s->night_mode);

        if(s->invincible_ticks == 0 || (s->tick_count % 4 < 2))
            draw_player_car(
                canvas, car_lx(s->player_lane), PLAYER_Y, s->night_mode, s->shield_ticks > 0);

        draw_particles(canvas, s);
        draw_hud(canvas, s);
        break;

    case StateGameOver:
        draw_road(canvas, s);
        for(int i = 0; i < MAX_OBS; i++)
            if(s->obstacles[i].alive) draw_obstacle(canvas, &s->obstacles[i], s->night_mode);
        draw_player_car(canvas, car_lx(s->player_lane), PLAYER_Y, s->night_mode, false);
        draw_particles(canvas, s);
        draw_hud(canvas, s);
        draw_game_over(canvas, s);
        break;
    }
}

// ─── Callbacks ──────────────────────────────────────────────────────────────

static void input_callback(InputEvent* ie, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;
    GameEvent ev = {.type = EventTypeKey, .input = *ie};
    furi_message_queue_put(q, &ev, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;
    GameEvent ev = {.type = EventTypeTick};
    furi_message_queue_put(q, &ev, 0);
}

// ─── Init ───────────────────────────────────────────────────────────────────

static void init_scenery(RaceGameState* s) {
    for(int i = 0; i < MAX_SCENERY; i++) {
        s->scenery[i].y = rand() % SCREEN_H;
        s->scenery[i].side = rand() % 2;
        s->scenery[i].type = rand() % 2;
    }
}

static void game_init(RaceGameState* s) {
    s->state = StatePlaying;
    s->player_lane = 1;
    s->score = 0;
    s->level = 0;
    s->lives = INITIAL_LIVES;
    s->speed = diff_speed[s->difficulty];
    s->tick_count = 0;
    s->road_scroll = 0;
    s->invincible_ticks = 0;
    s->shield_ticks = 0;
    s->magnet_ticks = 0;
    s->combo = 0;
    s->combo_display = 0;

    for(int i = 0; i < MAX_OBS; i++)
        s->obstacles[i].alive = false;
    for(int i = 0; i < MAX_COINS; i++)
        s->coins[i].alive = false;
    for(int i = 0; i < MAX_POWERUPS; i++)
        s->powerups[i].alive = false;
    for(int i = 0; i < MAX_PARTICLES; i++)
        s->particles[i].life = 0;

    init_scenery(s);
    furi_timer_start(s->timer, s->speed);
}

// ─── Collision ──────────────────────────────────────────────────────────────

static bool check_collision(RaceGameState* s) {
    int16_t px = car_lx(s->player_lane);
    for(int i = 0; i < MAX_OBS; i++) {
        if(!s->obstacles[i].alive) continue;
        int16_t ox, ow;
        if(s->obstacles[i].type == ObsTruck) {
            ox = car_lx(s->obstacles[i].lane) - 5;
            ow = 20;
        } else if(s->obstacles[i].type == ObsMoto) {
            ox = car_lx(s->obstacles[i].lane) + 2;
            ow = 6;
        } else {
            ox = car_lx(s->obstacles[i].lane);
            ow = CAR_W;
        }
        int16_t oh =
            (s->obstacles[i].type == ObsTruck) ? 16 : (s->obstacles[i].type == ObsMoto ? 10 : 12);
        if((px < ox + ow) && (px + CAR_W > ox) && (PLAYER_Y < s->obstacles[i].y + oh) &&
           (PLAYER_Y + CAR_H > s->obstacles[i].y))
            return true;
    }
    return false;
}

// ─── Coin / Power-Up Collection ─────────────────────────────────────────────

static void check_collections(RaceGameState* s) {
    int16_t px = car_lx(s->player_lane);

    // Coins
    for(int i = 0; i < MAX_COINS; i++) {
        if(!s->coins[i].alive) continue;
        int16_t cx = car_lx(s->coins[i].lane);
        bool magnet_pull = s->magnet_ticks > 0 && abs(s->coins[i].y - PLAYER_Y) < 30;
        bool touch = (px < cx + 8) && (px + CAR_W > cx) && (PLAYER_Y < s->coins[i].y + 8) &&
                     (PLAYER_Y + CAR_H > s->coins[i].y);
        if(touch || magnet_pull) {
            if(magnet_pull && !touch) {
                // Pull toward player
                s->coins[i].y += (s->coins[i].y < PLAYER_Y) ? 4 : -4;
                if(s->coins[i].lane < s->player_lane)
                    s->coins[i].lane++;
                else if(s->coins[i].lane > s->player_lane)
                    s->coins[i].lane--;
                continue; // Don't collect yet, just pull
            }
            s->coins[i].alive = false;
            s->combo++;
            s->combo_display = 15;
            uint32_t pts = 25 * (s->combo > 1 ? s->combo : 1);
            s->score += pts;
            play_sound(s, 1200 + s->combo * 100, 0.8f, 25);
        }
    }

    // Power-ups
    for(int i = 0; i < MAX_POWERUPS; i++) {
        if(!s->powerups[i].alive) continue;
        int16_t ppx = car_lx(s->powerups[i].lane);
        if((px < ppx + 8) && (px + CAR_W > ppx) && (PLAYER_Y < s->powerups[i].y + 8) &&
           (PLAYER_Y + CAR_H > s->powerups[i].y)) {
            s->powerups[i].alive = false;
            play_sound(s, 660, 0.8f, 40);
            switch(s->powerups[i].type) {
            case PwShield:
                s->shield_ticks = 50;
                break;
            case PwMagnet:
                s->magnet_ticks = 60;
                break;
            case PwFuel:
                if(s->lives < MAX_LIVES) s->lives++;
                break;
            }
        }
    }
}

// ─── Spawning ───────────────────────────────────────────────────────────────

static void spawn_obstacle(RaceGameState* s) {
    for(int i = 0; i < MAX_OBS; i++) {
        if(!s->obstacles[i].alive) {
            s->obstacles[i].alive = true;
            s->obstacles[i].y = -18;
            s->obstacles[i].lane = rand() % LANE_COUNT;

            // Boss truck every 5 levels
            if(s->level > 0 && s->level % 5 == 0 && (rand() % 4) == 0) {
                s->obstacles[i].type = ObsTruck;
                s->obstacles[i].lane = 1; // Center
            } else if(rand() % 3 == 0) {
                s->obstacles[i].type = ObsMoto;
            } else {
                s->obstacles[i].type = ObsSedan;
            }
            break;
        }
    }
}

static void spawn_coin(RaceGameState* s) {
    for(int i = 0; i < MAX_COINS; i++) {
        if(!s->coins[i].alive) {
            s->coins[i].alive = true;
            s->coins[i].y = -12;
            s->coins[i].lane = rand() % LANE_COUNT;
            break;
        }
    }
}

static void spawn_powerup(RaceGameState* s) {
    for(int i = 0; i < MAX_POWERUPS; i++) {
        if(!s->powerups[i].alive) {
            s->powerups[i].alive = true;
            s->powerups[i].y = -12;
            s->powerups[i].lane = rand() % LANE_COUNT;
            s->powerups[i].type = rand() % 3;
            break;
        }
    }
}

// ─── Game Tick ───────────────────────────────────────────────────────────────

static void game_tick(RaceGameState* s) {
    if(s->state != StatePlaying) return;

    s->tick_count++;
    s->road_scroll += 3;
    if(s->road_scroll >= DASH_TOTAL) s->road_scroll -= DASH_TOTAL;

    if(s->invincible_ticks > 0) s->invincible_ticks--;
    if(s->shield_ticks > 0) s->shield_ticks--;
    if(s->magnet_ticks > 0) s->magnet_ticks--;
    if(s->combo_display > 0) s->combo_display--;

    update_particles(s);

    // Move scenery
    for(int i = 0; i < MAX_SCENERY; i++) {
        s->scenery[i].y += 2;
        if(s->scenery[i].y > SCREEN_H + 5) {
            s->scenery[i].y = -(rand() % 20);
            s->scenery[i].side = rand() % 2;
            s->scenery[i].type = rand() % 2;
        }
    }

    // Move obstacles
    int spd = 3 + (s->level / 2);
    for(int i = 0; i < MAX_OBS; i++) {
        if(!s->obstacles[i].alive) continue;
        int move = spd;
        if(s->obstacles[i].type == ObsMoto) move += 1; // Motos are faster
        if(s->obstacles[i].type == ObsTruck) move -= 1; // Trucks are slower
        s->obstacles[i].y += move;
        int16_t oh = (s->obstacles[i].type == ObsTruck) ? 16 : 12;
        if(s->obstacles[i].y > SCREEN_H + oh) {
            s->obstacles[i].alive = false;
            s->score += 10;
        }
    }

    // Move coins + power-ups
    for(int i = 0; i < MAX_COINS; i++) {
        if(s->coins[i].alive) {
            s->coins[i].y += spd;
            if(s->coins[i].y > SCREEN_H) s->coins[i].alive = false;
        }
    }
    for(int i = 0; i < MAX_POWERUPS; i++) {
        if(s->powerups[i].alive) {
            s->powerups[i].y += spd;
            if(s->powerups[i].y > SCREEN_H) s->powerups[i].alive = false;
        }
    }

    // Spawn
    uint8_t si = diff_spawn[s->difficulty];
    if(s->tick_count % si == 0) spawn_obstacle(s);
    if(s->tick_count % 18 == 0) spawn_coin(s);
    if(s->tick_count % 60 == 0) spawn_powerup(s);

    // Collection
    check_collections(s);

    // Collision (skip if shield or invincible)
    if(s->invincible_ticks == 0 && s->shield_ticks == 0 && check_collision(s)) {
        s->lives--;
        s->combo = 0; // Reset combo on hit
        spawn_particles(s, car_lx(s->player_lane) + CAR_W / 2, PLAYER_Y + CAR_H / 2);
        vibrate(80);
        play_sound(s, 100, 1.0f, 80);

        if(s->lives == 0) {
            s->state = StateGameOver;
            furi_timer_stop(s->timer);
            vibrate(200);
            play_sound(s, 80, 1.0f, 200);
            save_high_score(s);
            return;
        }
        s->invincible_ticks = 20;
    }

    // Level up
    uint8_t nl = s->score / 200;
    if(nl > 9) nl = 9;
    if(nl > s->level) {
        s->level = nl;
        uint16_t min_spd = diff_min_speed[s->difficulty];
        s->speed = diff_speed[s->difficulty] - s->level * 8;
        if(s->speed < min_spd) s->speed = min_spd;
        furi_timer_stop(s->timer);
        furi_timer_start(s->timer, s->speed);
        play_sound(s, 880, 1.0f, 40);
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────

int32_t race_game_app(void* p) {
    UNUSED(p);
    srand(DWT->CYCCNT);

    RaceGameState* s = malloc(sizeof(RaceGameState));
    memset(s, 0, sizeof(RaceGameState));
    s->state = StateMenu;
    s->sound_on = true;
    s->night_mode = false;
    s->difficulty = DiffNormal;
    s->menu_idx = 0;
    s->lives = INITIAL_LIVES;

    load_high_score(s);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(GameEvent));
    s->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, q);

    ViewPort* vp = view_port_alloc();
    view_port_set_orientation(vp, ViewPortOrientationVertical);
    view_port_draw_callback_set(vp, draw_callback, s);
    view_port_input_callback_set(vp, input_callback, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    GameEvent ev;
    bool running = true;

    while(running) {
        furi_check(furi_message_queue_get(q, &ev, FuriWaitForever) == FuriStatusOk);

        if(ev.type == EventTypeKey) {
            if(ev.input.type == InputTypePress || ev.input.type == InputTypeRepeat) {
                switch(ev.input.key) {
                case InputKeyBack:
                    if(s->state == StatePlaying) {
                        // Pause: go back to menu
                        furi_timer_stop(s->timer);
                        s->state = StateMenu;
                    } else {
                        running = false;
                    }
                    break;

                case InputKeyOk:
                    if(s->state == StateMenu) {
                        if(s->menu_idx == 0)
                            game_init(s);
                        else if(s->menu_idx == 1)
                            s->sound_on = !s->sound_on;
                        else if(s->menu_idx == 2)
                            s->night_mode = !s->night_mode;
                        else if(s->menu_idx == 3)
                            s->difficulty = (s->difficulty + 1) % 3;
                    } else if(s->state == StateGameOver) {
                        s->state = StateMenu;
                    }
                    break;

                case InputKeyUp:
                    if(s->state == StateMenu) {
                        s->menu_idx--;
                        if(s->menu_idx < 0) s->menu_idx = 3;
                    }
                    break;

                case InputKeyDown:
                    if(s->state == StateMenu) {
                        s->menu_idx = (s->menu_idx + 1) % 4;
                    }
                    break;

                case InputKeyLeft:
                    if(s->state == StatePlaying && s->player_lane > 0) {
                        s->player_lane--;
                        play_sound(s, 440, 1.0f, 12);
                    }
                    break;

                case InputKeyRight:
                    if(s->state == StatePlaying && s->player_lane < LANE_COUNT - 1) {
                        s->player_lane++;
                        play_sound(s, 440, 1.0f, 12);
                    }
                    break;

                default:
                    break;
                }
            }
        } else if(ev.type == EventTypeTick) {
            game_tick(s);
        }

        view_port_update(vp);
    }

    furi_timer_stop(s->timer);
    furi_timer_free(s->timer);
    furi_message_queue_free(q);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    free(s);

    return 0;
}
