/*
 * Bounce for Flipper Zero
 * A tribute to the classic Nokia Bounce: roll the ball, jump, collect every
 * ring to open the exit, avoid thorns and spikers.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "levels.h"

#define TILE   8
#define VIEW_W 128
#define VIEW_H 56 /* bottom 8px are the HUD bar */

#define MAX_W       128
#define MAX_H       16
#define MAX_ENEMIES 16

#define TICK_HZ 30

/* Ball physics, in pixels and ticks */
#define BALL_HS           3.5f /* half size of the ball's 7x7 box */
#define GRAVITY           0.45f
#define MAX_FALL          6.5f
#define JUMP_V            4.5f /* ~20px high: clears two tiles */
#define SPRING_V          6.3f /* ~41px high: clears five tiles */
#define MAX_VX            2.0f
#define ACCEL_GROUND      0.22f
#define ACCEL_AIR         0.15f
#define FRICTION_GROUND   0.80f
#define FRICTION_AIR      0.97f
#define BOUNCE_MIN        2.2f /* landing faster than this rebounds */
#define BOUNCE_DAMP       0.45f
#define WALL_BOUNCE       0.3f
#define COYOTE_TICKS      4
#define JUMP_BUFFER_TICKS 6
#define INVULN_TICKS      45

#define WATER_LIFT   0.10f
#define WATER_DRAG   0.90f
#define WATER_MAX_VX 1.3f
#define WATER_MAX_VY 2.0f

#define ENEMY_SPEED 0.8f

#define START_LIVES 3
#define MAX_LIVES   9

#define SAVE_PATH    APP_DATA_PATH("bounce.save")
#define SAVE_MAGIC   0xB0
#define SAVE_VERSION 1

typedef enum {
    TileEmpty,
    TileBrick,
    TileSpikeUp,
    TileSpikeDown,
    TileRing,
    TileRingTaken,
    TileRingLow, /* lower half of a ring, drawn by the tile above */
    TileExit,
    TileCheckpoint,
    TileCheckpointOn,
    TileSpring,
    TileLife,
} TileType;

#define TILE_WATER   0x80
#define TILE_TYPE(t) ((t) & 0x7F)

typedef enum {
    StateTitle,
    StateIntro,
    StatePlaying,
    StatePaused,
    StateDying,
    StateLevelClear,
    StateGameOver,
    StateWin,
} GameState;

typedef struct {
    uint8_t unlocked; /* levels available in level select, 1..LEVEL_COUNT */
    uint8_t sound;
    uint32_t best_score;
} BounceSave;

typedef struct {
    float x, y; /* top-left corner */
    float vx, vy;
} Enemy;

typedef struct {
    BounceSave save;
    bool save_pending;

    GameState state;
    uint32_t tick;
    uint16_t state_timer;
    uint8_t menu_sel;
    uint8_t title_level;

    uint8_t level_idx;
    uint8_t lives;
    uint32_t score;
    uint32_t score_at_start; /* restored when a level is restarted */

    /* level */
    uint8_t w, h;
    uint8_t tiles[MAX_H][MAX_W];
    Enemy enemies[MAX_ENEMIES];
    uint8_t enemy_count;
    uint8_t rings_total, rings_left;
    float spawn_x, spawn_y;
    int16_t cp_c, cp_r; /* active checkpoint, -1 if none */

    /* ball */
    float bx, by, vx, vy;
    float roll;
    bool in_water;
    uint8_t coyote;
    uint8_t jump_buffer;
    uint8_t invuln;
    float pop_x, pop_y;

    /* held keys */
    bool key_left, key_right, key_up, key_down, key_ok;

    float cam_x, cam_y;

    char msg[24];
    uint8_t msg_timer;

    /* title screen ball */
    float t_x, t_y, t_vx, t_vy;

    bool running;
    NotificationApp* notif;
    FuriMutex* mutex;
} Game;

typedef enum {
    EventTick,
    EventInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

/* ---------------------------------------------------------------- sound */

static const NotificationSequence seq_jump = {
    &message_note_c6,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_spring = {
    &message_note_c6,
    &message_delay_10,
    &message_note_g6,
    &message_delay_10,
    &message_note_c7,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_ring = {
    &message_note_e6,
    &message_delay_25,
    &message_note_g6,
    &message_delay_25,
    &message_note_c7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_checkpoint = {
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_life = {
    &message_note_c6,
    &message_delay_50,
    &message_note_e6,
    &message_delay_50,
    &message_note_g6,
    &message_delay_50,
    &message_note_c7,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_door = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_pop = {
    &message_note_c5,
    &message_delay_50,
    &message_note_g4,
    &message_delay_50,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_clear = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_game_over = {
    &message_note_g4,
    &message_delay_250,
    &message_note_e4,
    &message_delay_250,
    &message_note_c4,
    &message_delay_500,
    &message_sound_off,
    NULL,
};

static void play(Game* g, const NotificationSequence* seq) {
    if(g->save.sound) notification_message(g->notif, seq);
}

static void show_msg(Game* g, const char* text) {
    strncpy(g->msg, text, sizeof(g->msg) - 1);
    g->msg[sizeof(g->msg) - 1] = '\0';
    g->msg_timer = 45;
}

/* ----------------------------------------------------------------- save */

static void save_load(Game* g) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc_set(SAVE_PATH);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);

    if(!saved_struct_load(SAVE_PATH, &g->save, sizeof(BounceSave), SAVE_MAGIC, SAVE_VERSION)) {
        g->save.unlocked = 1;
        g->save.sound = 1;
        g->save.best_score = 0;
    }
    if(g->save.unlocked < 1) g->save.unlocked = 1;
    if(g->save.unlocked > LEVEL_COUNT) g->save.unlocked = LEVEL_COUNT;
}

static void save_store(Game* g) {
    saved_struct_save(SAVE_PATH, &g->save, sizeof(BounceSave), SAVE_MAGIC, SAVE_VERSION);
}

/* ---------------------------------------------------------------- level */

static uint8_t tile_at(const Game* g, int c, int r) {
    if(c < 0 || c >= g->w || r < 0 || r >= g->h) return TileEmpty;
    return g->tiles[r][c];
}

static bool tile_is_solid(const Game* g, int c, int r) {
    if(c < 0 || c >= g->w || r < 0) return true;
    if(r >= g->h) return false; /* falling out of the map is fatal, not blocked */
    uint8_t t = TILE_TYPE(g->tiles[r][c]);
    return t == TileBrick || t == TileSpring || (t == TileExit && g->rings_left > 0);
}

static bool rect_hits_solid(const Game* g, float l, float t, float r, float b) {
    int c0 = (int)floorf(l / TILE), c1 = (int)floorf((r - 0.001f) / TILE);
    int r0 = (int)floorf(t / TILE), r1 = (int)floorf((b - 0.001f) / TILE);
    for(int rr = r0; rr <= r1; rr++) {
        for(int cc = c0; cc <= c1; cc++) {
            if(tile_is_solid(g, cc, rr)) return true;
        }
    }
    return false;
}

static bool ball_hits_solid(const Game* g, float x, float y) {
    return rect_hits_solid(g, x - BALL_HS, y - BALL_HS, x + BALL_HS, y + BALL_HS);
}

static void camera_update(Game* g, bool snap) {
    float max_x = g->w * TILE - VIEW_W;
    float max_y = g->h * TILE - VIEW_H;
    if(max_x < 0) max_x = 0;
    if(max_y < 0) max_y = 0;

    float tx = g->bx - VIEW_W / 2 + g->vx * 10.0f; /* look ahead */
    float ty = g->by - VIEW_H / 2 - 2.0f;
    if(tx < 0) tx = 0;
    if(tx > max_x) tx = max_x;
    if(ty < 0) ty = 0;
    if(ty > max_y) ty = max_y;

    if(snap) {
        g->cam_x = tx;
        g->cam_y = ty;
    } else {
        g->cam_x += (tx - g->cam_x) * 0.2f;
        g->cam_y += (ty - g->cam_y) * 0.25f;
    }
}

static void ball_spawn(Game* g) {
    if(g->cp_c >= 0) {
        g->bx = g->cp_c * TILE + TILE / 2;
        g->by = g->cp_r * TILE + TILE - BALL_HS;
    } else {
        g->bx = g->spawn_x;
        g->by = g->spawn_y;
    }
    g->vx = 0;
    g->vy = 0;
    g->coyote = 0;
    g->jump_buffer = 0;
    g->invuln = INVULN_TICKS;
    camera_update(g, true);
}

static void level_load(Game* g, uint8_t idx) {
    const LevelDef* def = &levels[idx];
    g->level_idx = idx;
    g->h = def->height > MAX_H ? MAX_H : def->height;
    size_t w = strlen(def->rows[0]);
    g->w = w > MAX_W ? MAX_W : (uint8_t)w;

    memset(g->tiles, 0, sizeof(g->tiles));
    g->enemy_count = 0;
    g->rings_total = 0;
    g->cp_c = -1;
    g->cp_r = -1;
    g->spawn_x = TILE + TILE / 2;
    g->spawn_y = TILE * 2 - BALL_HS;

    for(int r = 0; r < g->h; r++) {
        const char* row = def->rows[r];
        size_t len = strlen(row);
        for(int c = 0; c < g->w; c++) {
            char ch = (size_t)c < len ? row[c] : ' ';
            uint8_t t = TileEmpty;
            switch(ch) {
            case '#':
                t = TileBrick;
                break;
            case '^':
                t = TileSpikeUp;
                break;
            case 'v':
                t = TileSpikeDown;
                break;
            case 'O':
                t = TileRing;
                g->rings_total++;
                break;
            case 'o':
                t = TileRing | TILE_WATER;
                g->rings_total++;
                break;
            case 'E':
                t = TileExit;
                break;
            case 'C':
                t = TileCheckpoint;
                break;
            case 'J':
                t = TileSpring;
                break;
            case '~':
                t = TILE_WATER;
                break;
            case '+':
                t = TileLife;
                break;
            case 'S':
                g->spawn_x = c * TILE + TILE / 2;
                g->spawn_y = r * TILE + TILE - BALL_HS;
                break;
            case 'm':
            case 'n':
                t = TILE_WATER;
                /* fall through */
            case 'M':
            case 'N':
                if(g->enemy_count < MAX_ENEMIES) {
                    Enemy* e = &g->enemies[g->enemy_count++];
                    bool horizontal = (ch == 'M' || ch == 'm');
                    e->x = c * TILE;
                    e->y = r * TILE;
                    e->vx = horizontal ? ENEMY_SPEED : 0;
                    e->vy = horizontal ? 0 : ENEMY_SPEED;
                }
                break;
            default:
                break;
            }
            g->tiles[r][c] = t;
        }
    }

    /* Rings are two tiles tall: mark the lower halves */
    for(int r = 0; r + 1 < g->h; r++) {
        for(int c = 0; c < g->w; c++) {
            if(TILE_TYPE(g->tiles[r][c]) == TileRing &&
               TILE_TYPE(g->tiles[r + 1][c]) == TileEmpty) {
                g->tiles[r + 1][c] = TileRingLow | (g->tiles[r + 1][c] & TILE_WATER);
            }
        }
    }

    g->rings_left = g->rings_total;
    g->msg_timer = 0;
    ball_spawn(g);
    g->invuln = 0;
}

/* -------------------------------------------------------------- physics */

static bool ball_on_spring(const Game* g) {
    int r = (int)floorf((g->by + BALL_HS + 0.5f) / TILE);
    int c0 = (int)floorf((g->bx - BALL_HS) / TILE);
    int c1 = (int)floorf((g->bx + BALL_HS - 0.001f) / TILE);
    for(int c = c0; c <= c1; c++) {
        if(TILE_TYPE(tile_at(g, c, r)) == TileSpring) return true;
    }
    return false;
}

static void hint_locked_exit(Game* g, int dir) {
    if(g->rings_left == 0 || g->msg_timer > 0) return;
    int c = (int)floorf((g->bx + dir * (BALL_HS + 1.0f)) / TILE);
    int r0 = (int)floorf((g->by - BALL_HS) / TILE);
    int r1 = (int)floorf((g->by + BALL_HS - 0.001f) / TILE);
    for(int r = r0; r <= r1; r++) {
        if(TILE_TYPE(tile_at(g, c, r)) == TileExit) {
            char buf[24];
            snprintf(
                buf, sizeof(buf), "%u ring%s left!", g->rings_left, g->rings_left == 1 ? "" : "s");
            show_msg(g, buf);
            return;
        }
    }
}

static void ball_move_x(Game* g) {
    float dx = g->vx;
    if(dx == 0) return;
    int steps = (int)ceilf(fabsf(dx) / 0.5f);
    float s = dx / steps;
    for(int i = 0; i < steps; i++) {
        float nx = g->bx + s;
        if(!ball_hits_solid(g, nx, g->by)) {
            g->bx = nx;
            continue;
        }
        /* Snap flush against the obstacle */
        float snap = s > 0 ? floorf((nx + BALL_HS) / TILE) * TILE - BALL_HS :
                             ceilf((nx - BALL_HS) / TILE) * TILE + BALL_HS;
        if((s > 0 ? snap >= g->bx : snap <= g->bx) && !ball_hits_solid(g, snap, g->by)) {
            g->bx = snap;
        }
        hint_locked_exit(g, s > 0 ? 1 : -1);
        g->vx = fabsf(g->vx) > 1.0f ? -g->vx * WALL_BOUNCE : 0;
        break;
    }
}

static void ball_move_y(Game* g) {
    float dy = g->vy;
    if(dy == 0) return;
    int steps = (int)ceilf(fabsf(dy) / 0.5f);
    float s = dy / steps;
    for(int i = 0; i < steps; i++) {
        float ny = g->by + s;
        if(!ball_hits_solid(g, g->bx, ny)) {
            g->by = ny;
            continue;
        }
        float snap = s > 0 ? floorf((ny + BALL_HS) / TILE) * TILE - BALL_HS :
                             ceilf((ny - BALL_HS) / TILE) * TILE + BALL_HS;
        if((s > 0 ? snap >= g->by : snap <= g->by) && !ball_hits_solid(g, g->bx, snap)) {
            g->by = snap;
        }
        if(s > 0) {
            if(ball_on_spring(g)) {
                g->vy = -SPRING_V;
                play(g, &seq_spring);
            } else if(
                g->vy > BOUNCE_MIN && !g->in_water &&
                !(g->jump_buffer > 0 || g->key_up || g->key_ok)) {
                /* Rebound only when the player isn't about to jump anyway */
                g->vy = -g->vy * BOUNCE_DAMP;
            } else {
                g->vy = 0;
            }
        } else {
            g->vy = 0; /* bumped the ceiling */
        }
        break;
    }
}

static void ball_update(Game* g) {
    int cc = (int)floorf(g->bx / TILE);
    g->in_water = tile_at(g, cc, (int)floorf(g->by / TILE)) & TILE_WATER;

    bool grounded = ball_hits_solid(g, g->bx, g->by + 0.6f);
    if(grounded && g->vy >= 0) {
        g->coyote = COYOTE_TICKS;
    } else if(g->coyote > 0) {
        g->coyote--;
    }

    /* Rolling */
    int dir = (int)g->key_right - (int)g->key_left;
    float max_vx = g->in_water ? WATER_MAX_VX : MAX_VX;
    if(dir != 0) {
        float a = grounded ? ACCEL_GROUND : ACCEL_AIR;
        if(dir * g->vx < 0) a *= 1.6f; /* snappy turnaround */
        g->vx += dir * a;
    } else {
        g->vx *= grounded ? FRICTION_GROUND : FRICTION_AIR;
        if(fabsf(g->vx) < 0.05f) g->vx = 0;
    }
    if(g->vx > max_vx) g->vx = max_vx;
    if(g->vx < -max_vx) g->vx = -max_vx;

    /* Jumping */
    bool key_jump = g->key_up || g->key_ok;
    bool want_jump = g->jump_buffer > 0 || key_jump;
    if(g->jump_buffer > 0) g->jump_buffer--;

    if(g->in_water) {
        int top_r = (int)floorf((g->by - BALL_HS) / TILE);
        bool surfaced = !(tile_at(g, cc, top_r) & TILE_WATER);
        if(want_jump && surfaced) {
            g->vy = -JUMP_V * 0.95f;
            g->jump_buffer = 0;
            play(g, &seq_jump);
        } else if(want_jump && grounded) {
            g->vy = -2.5f;
            g->jump_buffer = 0;
        } else {
            g->vy -= WATER_LIFT;
            if(g->key_down) g->vy += 0.35f;
            if(key_jump) g->vy -= 0.15f;
            g->vy *= WATER_DRAG;
            if(g->vy > WATER_MAX_VY) g->vy = WATER_MAX_VY;
            if(g->vy < -WATER_MAX_VY) g->vy = -WATER_MAX_VY;
        }
    } else {
        if(want_jump && g->coyote > 0 && g->vy > -1.0f) {
            g->vy = -JUMP_V;
            g->coyote = 0;
            g->jump_buffer = 0;
            play(g, &seq_jump);
        }
        g->vy += GRAVITY;
        if(g->vy > MAX_FALL) g->vy = MAX_FALL;
    }

    ball_move_x(g);
    ball_move_y(g);
    g->roll += g->vx / BALL_HS;
}

static bool
    rects_overlap(float al, float at, float ar, float ab, float bl, float bt, float br, float bb) {
    return al < br && ar > bl && at < bb && ab > bt;
}

static bool ball_hits_hazard(const Game* g) {
    /* A slightly smaller box than the ball keeps near misses fair */
    float l = g->bx - BALL_HS + 1.2f, r = g->bx + BALL_HS - 1.2f;
    float t = g->by - BALL_HS + 1.2f, b = g->by + BALL_HS - 1.2f;

    int c0 = (int)floorf(l / TILE), c1 = (int)floorf(r / TILE);
    int r0 = (int)floorf(t / TILE), r1 = (int)floorf(b / TILE);
    for(int rr = r0; rr <= r1; rr++) {
        for(int cc = c0; cc <= c1; cc++) {
            uint8_t type = TILE_TYPE(tile_at(g, cc, rr));
            float x = cc * TILE, y = rr * TILE;
            if(type == TileSpikeUp &&
               rects_overlap(l, t, r, b, x + 1.5f, y + 2.0f, x + 6.5f, y + 8.0f)) {
                return true;
            }
            if(type == TileSpikeDown &&
               rects_overlap(l, t, r, b, x + 1.5f, y, x + 6.5f, y + 6.0f)) {
                return true;
            }
        }
    }

    for(int i = 0; i < g->enemy_count; i++) {
        const Enemy* e = &g->enemies[i];
        if(rects_overlap(l, t, r, b, e->x + 1.5f, e->y + 1.5f, e->x + 6.5f, e->y + 6.5f)) {
            return true;
        }
    }
    return false;
}

static void level_complete(Game* g);

static void ball_collect(Game* g) {
    int c0 = (int)floorf((g->bx - BALL_HS) / TILE);
    int c1 = (int)floorf((g->bx + BALL_HS - 0.001f) / TILE);
    int r0 = (int)floorf((g->by - BALL_HS) / TILE);
    int r1 = (int)floorf((g->by + BALL_HS - 0.001f) / TILE);

    for(int r = r0; r <= r1; r++) {
        for(int c = c0; c <= c1; c++) {
            if(c < 0 || c >= g->w || r < 0 || r >= g->h) continue;
            uint8_t type = TILE_TYPE(g->tiles[r][c]);

            if(type == TileRing || type == TileRingLow) {
                int top = type == TileRingLow ? r - 1 : r;
                if(top < 0 || TILE_TYPE(g->tiles[top][c]) != TileRing) continue;
                /* Only counts when the ball goes through the middle of the ring */
                float cx = c * TILE + TILE / 2;
                float ty = top * TILE;
                if(fabsf(g->bx - cx) <= 3.5f && g->by > ty + 3.0f && g->by < ty + 13.0f) {
                    g->tiles[top][c] = TileRingTaken | (g->tiles[top][c] & TILE_WATER);
                    g->rings_left--;
                    g->score += 100;
                    if(g->rings_left == 0) {
                        play(g, &seq_door);
                        show_msg(g, "Exit open!");
                    } else {
                        play(g, &seq_ring);
                    }
                }
            } else if(type == TileCheckpoint) {
                if(g->cp_c >= 0) g->tiles[g->cp_r][g->cp_c] = TileCheckpoint;
                g->tiles[r][c] = TileCheckpointOn;
                g->cp_c = c;
                g->cp_r = r;
                g->score += 50;
                play(g, &seq_checkpoint);
                show_msg(g, "Checkpoint");
            } else if(type == TileLife) {
                g->tiles[r][c] &= TILE_WATER;
                if(g->lives < MAX_LIVES) g->lives++;
                g->score += 250;
                play(g, &seq_life);
                show_msg(g, "Extra life!");
            } else if(type == TileExit && g->rings_left == 0) {
                level_complete(g);
                return;
            }
        }
    }
}

static void enemies_update(Game* g) {
    for(int i = 0; i < g->enemy_count; i++) {
        Enemy* e = &g->enemies[i];
        float nx = e->x + e->vx, ny = e->y + e->vy;
        if(rect_hits_solid(g, nx, ny, nx + TILE, ny + TILE) || ny + TILE > g->h * TILE) {
            e->vx = -e->vx;
            e->vy = -e->vy;
        } else {
            e->x = nx;
            e->y = ny;
        }
    }
}

/* ------------------------------------------------------------ game flow */

static void title_enter(Game* g) {
    g->state = StateTitle;
    g->menu_sel = 0;
    g->title_level = g->save.unlocked;
    g->t_x = 10;
    g->t_y = 30.5f;
    g->t_vx = 1.2f;
    g->t_vy = -3.0f;
}

static void level_start(Game* g, uint8_t idx) {
    level_load(g, idx);
    g->score_at_start = g->score;
    g->state = StateIntro;
    g->state_timer = 50;
}

static void game_new(Game* g, uint8_t idx) {
    g->lives = START_LIVES;
    g->score = 0;
    level_start(g, idx);
}

static void update_best(Game* g) {
    if(g->score > g->save.best_score) {
        g->save.best_score = g->score;
        g->save_pending = true;
    }
}

static void ball_die(Game* g) {
    g->state = StateDying;
    g->state_timer = 30;
    g->pop_x = g->bx;
    g->pop_y = g->by;
    play(g, &seq_pop);
    notification_message(g->notif, &sequence_single_vibro);
}

static void level_complete(Game* g) {
    g->state = StateLevelClear;
    g->state_timer = 0;
    g->score += 500 + 100 * g->lives;
    uint8_t next = g->level_idx + 2; /* 1-based number of the next level */
    if(next <= LEVEL_COUNT && next > g->save.unlocked) {
        g->save.unlocked = next;
        g->save_pending = true;
    }
    update_best(g);
    play(g, &seq_clear);
}

static void level_advance(Game* g) {
    if(g->level_idx + 1 < (int)LEVEL_COUNT) {
        level_start(g, g->level_idx + 1);
    } else {
        g->state = StateWin;
        g->state_timer = 0;
        update_best(g);
    }
}

static void game_tick(Game* g) {
    g->tick++;
    if(g->msg_timer > 0) g->msg_timer--;

    switch(g->state) {
    case StateTitle:
        g->t_x += g->t_vx;
        if(g->t_x < 4 || g->t_x > 76) g->t_vx = -g->t_vx;
        g->t_vy += GRAVITY;
        g->t_y += g->t_vy;
        if(g->t_y > 30.5f) {
            g->t_y = 30.5f;
            g->t_vy = -3.0f;
        }
        break;

    case StateIntro:
        if(g->state_timer > 0) g->state_timer--;
        if(g->state_timer == 0) g->state = StatePlaying;
        break;

    case StatePlaying:
        if(g->invuln > 0) g->invuln--;
        ball_update(g);
        enemies_update(g);
        if(g->by - BALL_HS > g->h * TILE) {
            ball_die(g);
            break;
        }
        if(g->invuln == 0 && ball_hits_hazard(g)) {
            ball_die(g);
            break;
        }
        ball_collect(g);
        camera_update(g, false);
        break;

    case StateDying:
        enemies_update(g);
        if(g->state_timer > 0) g->state_timer--;
        if(g->state_timer == 0) {
            g->lives--;
            if(g->lives == 0) {
                g->state = StateGameOver;
                g->state_timer = 0;
                update_best(g);
                play(g, &seq_game_over);
            } else {
                ball_spawn(g);
                g->state = StatePlaying;
            }
        }
        break;

    case StateLevelClear:
        if(++g->state_timer >= 90) level_advance(g);
        break;

    case StateGameOver:
    case StateWin:
        if(g->state_timer < 0xFFFF) g->state_timer++;
        break;

    case StatePaused:
        break;
    }
}

/* ------------------------------------------------------------ rendering */

/* 8px wide bitmaps, one byte per row, LSB is the leftmost pixel */
static const uint8_t bmp_brick[] = {0xFF, 0x08, 0x08, 0x08, 0xFF, 0x80, 0x80, 0x80};
static const uint8_t bmp_spike_up[] = {0x18, 0x18, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0xFF};
static const uint8_t bmp_spike_down[] = {0xFF, 0xFF, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18};
static const uint8_t bmp_spring[] = {0xFF, 0xFF, 0x42, 0x24, 0x18, 0x24, 0x42, 0xFF};
static const uint8_t bmp_exit_locked[] = {0xFF, 0x99, 0xA5, 0xC3, 0xC3, 0xA5, 0x99, 0xFF};
static const uint8_t bmp_flag_off[] = {0x1E, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02, 0x07};
static const uint8_t bmp_flag_on[] = {0x1E, 0x3E, 0x3E, 0x1E, 0x02, 0x02, 0x02, 0x07};
static const uint8_t bmp_heart[] = {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00};
static const uint8_t bmp_spiker_a[] = {0x99, 0x5A, 0x3C, 0xFF, 0xFF, 0x3C, 0x5A, 0x99};
static const uint8_t bmp_spiker_b[] = {0x18, 0x18, 0x3C, 0xFF, 0xFF, 0x3C, 0x18, 0x18};
static const uint8_t bmp_ring[] = {
    0x3C,
    0x7E,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x66,
    0x7E,
    0x3C,
};
static const uint8_t bmp_ring_taken[] = {
    0x18,
    0x42,
    0x00,
    0x42,
    0x00,
    0x42,
    0x00,
    0x42,
    0x00,
    0x42,
    0x00,
    0x42,
    0x00,
    0x42,
    0x00,
    0x18,
};

/* 5x7 letters for the title, bit 4 is the leftmost pixel */
static const uint8_t title_font[6][7] = {
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
    {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F}, /* C */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
};

static void dot_clipped(Canvas* canvas, int x, int y) {
    if(x >= 0 && x < VIEW_W && y >= 0 && y < VIEW_H) canvas_draw_dot(canvas, x, y);
}

/* Draws an 8px wide bitmap, clipping it to the play area */
static void draw_bits(Canvas* canvas, int x, int y, const uint8_t* bits, int h) {
    if(x >= 0 && y >= 0 && x + 8 <= VIEW_W && y + h <= VIEW_H) {
        canvas_draw_xbm(canvas, x, y, 8, h, bits);
        return;
    }
    for(int r = 0; r < h; r++) {
        for(int c = 0; c < 8; c++) {
            if(bits[r] & (1 << c)) dot_clipped(canvas, x + c, y + r);
        }
    }
}

static void draw_water(Canvas* canvas, const Game* g, int c, int r, int sx, int sy) {
    bool surface = !(tile_at(g, c, r - 1) & TILE_WATER);
    int phase = g->tick / 6;
    for(int xx = 0; xx < TILE; xx++) {
        int wx = c * TILE + xx;
        if(surface) dot_clipped(canvas, sx + xx, sy + (((wx + phase) >> 1) & 1));
        for(int yy = 3; yy < TILE; yy += 4) {
            int wy = r * TILE + yy;
            if((wx + (wy >> 2) * 3 + phase) % 6 == 0) dot_clipped(canvas, sx + xx, sy + yy);
        }
    }
}

static void draw_ball(Canvas* canvas, int cx, int cy, float roll, bool outline) {
    canvas_set_color(canvas, ColorBlack);
    if(outline) {
        canvas_draw_circle(canvas, cx, cy, 3);
        return;
    }
    canvas_draw_disc(canvas, cx, cy, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, cx - 1, cy - 1);
    canvas_draw_dot(
        canvas, cx + (int)roundf(2.0f * cosf(roll)), cy + (int)roundf(2.0f * sinf(roll)));
    canvas_set_color(canvas, ColorBlack);
}

static void draw_pop(Canvas* canvas, int cx, int cy, int t) {
    static const int8_t dx[8] = {10, 7, 0, -7, -10, -7, 0, 7};
    static const int8_t dy[8] = {0, 7, 10, 7, 0, -7, -10, -7};
    int rad = 2 + t / 2;
    for(int i = 0; i < 8; i++) {
        dot_clipped(canvas, cx + dx[i] * rad / 10, cy + dy[i] * rad / 10);
        dot_clipped(canvas, cx + dx[i] * (rad + 2) / 10, cy + dy[i] * (rad + 2) / 10);
    }
}

static void draw_world(Canvas* canvas, const Game* g) {
    int camx = (int)roundf(g->cam_x), camy = (int)roundf(g->cam_y);
    int c0 = camx / TILE, c1 = (camx + VIEW_W - 1) / TILE;
    int r0 = camy / TILE - 1, r1 = (camy + VIEW_H - 1) / TILE; /* -1: rings reach down */
    bool blink = (g->tick / 8) & 1;

    canvas_set_color(canvas, ColorBlack);
    for(int r = r0; r <= r1; r++) {
        for(int c = c0; c <= c1; c++) {
            uint8_t t = tile_at(g, c, r);
            int sx = c * TILE - camx, sy = r * TILE - camy;
            if(t & TILE_WATER) draw_water(canvas, g, c, r, sx, sy);
            switch(TILE_TYPE(t)) {
            case TileBrick:
                draw_bits(canvas, sx, sy, bmp_brick, 8);
                break;
            case TileSpikeUp:
                draw_bits(canvas, sx, sy, bmp_spike_up, 8);
                break;
            case TileSpikeDown:
                draw_bits(canvas, sx, sy, bmp_spike_down, 8);
                break;
            case TileRing:
                draw_bits(canvas, sx, sy, bmp_ring, 16);
                break;
            case TileRingTaken:
                draw_bits(canvas, sx, sy, bmp_ring_taken, 16);
                break;
            case TileSpring:
                draw_bits(canvas, sx, sy, bmp_spring, 8);
                break;
            case TileCheckpoint:
                draw_bits(canvas, sx, sy, bmp_flag_off, 8);
                break;
            case TileCheckpointOn:
                draw_bits(canvas, sx, sy, bmp_flag_on, 8);
                break;
            case TileLife:
                draw_bits(canvas, sx, sy + (blink ? 1 : 0), bmp_heart, 8);
                break;
            case TileExit:
                if(g->rings_left > 0) {
                    draw_bits(canvas, sx, sy, bmp_exit_locked, 8);
                } else {
                    /* Open gate: side posts with sparkles rising through it */
                    for(int yy = 0; yy < TILE; yy++) {
                        dot_clipped(canvas, sx, sy + yy);
                        dot_clipped(canvas, sx + 7, sy + yy);
                        if((r * TILE + yy + g->tick / 2) % 4 == 0) {
                            dot_clipped(canvas, sx + 3 + (yy & 1), sy + yy);
                        }
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    for(int i = 0; i < g->enemy_count; i++) {
        const Enemy* e = &g->enemies[i];
        draw_bits(
            canvas,
            (int)roundf(e->x) - camx,
            (int)roundf(e->y) - camy,
            blink ? bmp_spiker_a : bmp_spiker_b,
            8);
    }

    if(g->state == StateDying) {
        int cx = (int)floorf(g->pop_x - BALL_HS) + 3 - camx;
        int cy = (int)floorf(g->pop_y - BALL_HS) + 3 - camy;
        draw_pop(canvas, cx, cy, 30 - g->state_timer);
    } else {
        int cx = (int)floorf(g->bx - BALL_HS) + 3 - camx;
        int cy = (int)floorf(g->by - BALL_HS) + 3 - camy;
        draw_ball(canvas, cx, cy, g->roll, g->invuln > 0 && ((g->tick / 3) & 1));
    }
}

static void draw_hud(Canvas* canvas, const Game* g) {
    char buf[16];
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, VIEW_H, 128, 64 - VIEW_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_disc(canvas, 3, 59, 2);
    snprintf(buf, sizeof(buf), "%u", g->lives);
    canvas_draw_str(canvas, 8, 63, buf);

    canvas_draw_rframe(canvas, 20, 57, 5, 7, 2);
    snprintf(buf, sizeof(buf), "%u/%u", g->rings_total - g->rings_left, g->rings_total);
    canvas_draw_str(canvas, 27, 63, buf);

    snprintf(buf, sizeof(buf), "L%u", g->level_idx + 1);
    canvas_draw_str(canvas, 66 - canvas_string_width(canvas, buf) / 2, 63, buf);

    snprintf(buf, sizeof(buf), "%06lu", (unsigned long)g->score);
    canvas_draw_str(canvas, 127 - canvas_string_width(canvas, buf), 63, buf);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_panel(Canvas* canvas, int x, int y, int w, int h) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, x, y, w, h, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, w, h, 3);
}

static void draw_centered(Canvas* canvas, int y, const char* text) {
    canvas_draw_str(canvas, 64 - canvas_string_width(canvas, text) / 2, y, text);
}

static void draw_banner(Canvas* canvas, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, text) + 10;
    draw_panel(canvas, 64 - w / 2, 2, w, 12);
    draw_centered(canvas, 11, text);
}

/* Menu row; the selected one is drawn inverted */
static void draw_menu_item(Canvas* canvas, int y, const char* text, bool selected) {
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 14, y, 100, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    draw_centered(canvas, y + 8, text);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_title(Canvas* canvas, const Game* g) {
    char buf[24];
    for(int i = 0; i < 6; i++) {
        for(int row = 0; row < 7; row++) {
            for(int col = 0; col < 5; col++) {
                if(title_font[i][row] & (0x10 >> col)) {
                    canvas_draw_box(canvas, 29 + i * 12 + col * 2, 2 + row * 2, 2, 2);
                }
            }
        }
    }

    for(int x = 0; x < VIEW_W; x += TILE)
        draw_bits(canvas, x, 34, bmp_brick, 8);
    int cx = (int)floorf(g->t_x - BALL_HS) + 3;
    int cy = (int)floorf(g->t_y - BALL_HS) + 3;
    draw_ball(canvas, cx, cy, g->t_x / BALL_HS, false);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 127 - canvas_string_width(canvas, "BEST"), 24, "BEST");
    snprintf(buf, sizeof(buf), "%06lu", (unsigned long)g->save.best_score);
    canvas_draw_str(canvas, 127 - canvas_string_width(canvas, buf), 32, buf);

    snprintf(
        buf,
        sizeof(buf),
        "%s Level %u %s",
        g->title_level > 1 ? "<" : " ",
        g->title_level,
        g->title_level < g->save.unlocked ? ">" : " ");
    draw_menu_item(canvas, 43, buf, g->menu_sel == 0);
    draw_menu_item(canvas, 53, g->save.sound ? "Sound: On" : "Sound: Off", g->menu_sel == 1);
}

static void draw_intro(Canvas* canvas, const Game* g) {
    char buf[16];
    draw_panel(canvas, 14, 10, 100, 32);
    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "LEVEL %u", g->level_idx + 1);
    draw_centered(canvas, 24, buf);
    canvas_set_font(canvas, FontSecondary);
    draw_centered(canvas, 36, levels[g->level_idx].name);
}

static void draw_pause(Canvas* canvas, const Game* g) {
    static const char* items[] = {"Resume", "Restart level", "Title screen"};
    draw_panel(canvas, 14, 3, 100, 49);
    canvas_set_font(canvas, FontPrimary);
    draw_centered(canvas, 15, "PAUSED");
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 3; i++) {
        draw_menu_item(canvas, 19 + i * 10, items[i], g->menu_sel == i);
    }
}

static void draw_clear(Canvas* canvas, const Game* g) {
    char buf[24];
    draw_panel(canvas, 14, 6, 100, 42);
    canvas_set_font(canvas, FontPrimary);
    draw_centered(canvas, 19, "LEVEL CLEAR!");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Score %06lu", (unsigned long)g->score);
    draw_centered(canvas, 31, buf);
    draw_centered(canvas, 43, "OK: continue");
}

static void draw_end(Canvas* canvas, const Game* g, const char* title, const char* subtitle) {
    char buf[24];
    canvas_set_font(canvas, FontPrimary);
    draw_centered(canvas, 12, title);
    canvas_set_font(canvas, FontSecondary);
    draw_centered(canvas, 24, subtitle);
    snprintf(buf, sizeof(buf), "Score %06lu", (unsigned long)g->score);
    draw_centered(canvas, 36, buf);
    snprintf(buf, sizeof(buf), "Best  %06lu", (unsigned long)g->save.best_score);
    draw_centered(canvas, 46, buf);
    for(int x = 0; x < VIEW_W; x += TILE)
        draw_bits(canvas, x, 48, bmp_brick, 8);
    draw_centered(canvas, 63, "OK: title screen");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    Game* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(g->state) {
    case StateTitle:
        draw_title(canvas, g);
        break;
    case StateGameOver:
        draw_end(canvas, g, "GAME OVER", "Out of balls!");
        break;
    case StateWin:
        draw_end(canvas, g, "YOU WIN!", "Every level cleared");
        break;
    default:
        draw_world(canvas, g);
        draw_hud(canvas, g);
        if(g->state == StateIntro) {
            draw_intro(canvas, g);
        } else if(g->state == StatePaused) {
            draw_pause(canvas, g);
        } else if(g->state == StateLevelClear) {
            draw_clear(canvas, g);
        } else if(g->msg_timer > 0) {
            draw_banner(canvas, g->msg);
        }
        break;
    }

    furi_mutex_release(g->mutex);
}

/* ---------------------------------------------------------------- input */

static void handle_input(Game* g, const InputEvent* ev) {
    if(ev->type == InputTypePress || ev->type == InputTypeRelease) {
        bool down = ev->type == InputTypePress;
        switch(ev->key) {
        case InputKeyLeft:
            g->key_left = down;
            break;
        case InputKeyRight:
            g->key_right = down;
            break;
        case InputKeyUp:
            g->key_up = down;
            break;
        case InputKeyDown:
            g->key_down = down;
            break;
        case InputKeyOk:
            g->key_ok = down;
            break;
        default:
            break;
        }
    }

    bool press = ev->type == InputTypePress;
    bool nav = press || ev->type == InputTypeRepeat;
    bool back = ev->key == InputKeyBack && ev->type == InputTypeShort;

    switch(g->state) {
    case StateTitle:
        if(back) {
            g->running = false;
        } else if(nav && (ev->key == InputKeyUp || ev->key == InputKeyDown)) {
            g->menu_sel ^= 1;
        } else if(g->menu_sel == 0) {
            if(nav && ev->key == InputKeyLeft && g->title_level > 1) g->title_level--;
            if(nav && ev->key == InputKeyRight && g->title_level < g->save.unlocked) {
                g->title_level++;
            }
            if(press && ev->key == InputKeyOk) game_new(g, g->title_level - 1);
        } else if(
            press &&
            (ev->key == InputKeyOk || ev->key == InputKeyLeft || ev->key == InputKeyRight)) {
            g->save.sound ^= 1;
            g->save_pending = true;
        }
        break;

    case StateIntro:
        if(press && ev->key == InputKeyOk) g->state = StatePlaying;
        if(back) {
            g->state = StatePaused;
            g->menu_sel = 0;
        }
        break;

    case StatePlaying:
        if(press && (ev->key == InputKeyUp || ev->key == InputKeyOk)) {
            g->jump_buffer = JUMP_BUFFER_TICKS;
        } else if(back) {
            g->state = StatePaused;
            g->menu_sel = 0;
        }
        break;

    case StatePaused:
        if(back) {
            g->state = StatePlaying;
        } else if(nav && ev->key == InputKeyUp) {
            g->menu_sel = (g->menu_sel + 2) % 3;
        } else if(nav && ev->key == InputKeyDown) {
            g->menu_sel = (g->menu_sel + 1) % 3;
        } else if(press && ev->key == InputKeyOk) {
            if(g->menu_sel == 0) {
                g->state = StatePlaying;
            } else if(g->menu_sel == 1) {
                g->score = g->score_at_start;
                level_start(g, g->level_idx);
            } else {
                update_best(g);
                title_enter(g);
            }
        }
        break;

    case StateLevelClear:
        if(press && ev->key == InputKeyOk && g->state_timer > 15) level_advance(g);
        break;

    case StateGameOver:
    case StateWin:
        if(g->state_timer > 20 && ((press && ev->key == InputKeyOk) || back)) title_enter(g);
        break;

    case StateDying:
        break;
    }
}

/* ----------------------------------------------------------------- main */

static void input_callback(InputEvent* input, void* ctx) {
    FuriMessageQueue* queue = ctx;
    GameEvent ev = {.type = EventInput, .input = *input};
    furi_message_queue_put(queue, &ev, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    FuriMessageQueue* queue = ctx;
    GameEvent ev = {.type = EventTick};
    furi_message_queue_put(queue, &ev, 0);
}

int32_t bounce_app(void* p) {
    UNUSED(p);

    Game* g = malloc(sizeof(Game));
    memset(g, 0, sizeof(Game));
    g->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    save_load(g);
    title_enter(g);
    g->running = true;

    FuriMessageQueue* queue = furi_message_queue_alloc(16, sizeof(GameEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, g);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    g->notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(g->notif, &sequence_display_backlight_enforce_on);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / TICK_HZ);

    GameEvent ev;
    while(g->running) {
        if(furi_message_queue_get(queue, &ev, FuriWaitForever) != FuriStatusOk) continue;

        furi_mutex_acquire(g->mutex, FuriWaitForever);
        if(ev.type == EventTick) {
            game_tick(g);
        } else {
            handle_input(g, &ev.input);
        }
        bool do_save = g->save_pending;
        g->save_pending = false;
        furi_mutex_release(g->mutex);

        /* SD card writes happen outside the lock so drawing never stalls */
        if(do_save) save_store(g);
        view_port_update(view_port);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);

    notification_message(g->notif, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(queue);
    furi_mutex_free(g->mutex);
    free(g);
    return 0;
}
