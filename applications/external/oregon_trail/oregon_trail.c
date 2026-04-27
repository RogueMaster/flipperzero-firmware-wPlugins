#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "game_state.h"
#include "sprites.h"
#include "hunt.h"
#include "day.h"
#include "event_draw.h"
#include "river.h"
#include "sound.h"

// ── Name edit state ───────────────────────────────────────────
// cursor: 0=player1, 1=player2, 2=Start (default)
// editing: true = character-edit sub-mode active
typedef struct {
    int  cursor;
    bool editing;
    int  char_pos;
    int  edit_player;
    char edit_buf[NAME_LEN];
} NameEditState;

// ── App context ───────────────────────────────────────────────
typedef struct {
    GameState         gs;
    HuntState         hunt;
    ActiveEvent       active_event;
    RiverState        river;
    NameEditState     name_edit;
    FuriMessageQueue* input_queue;
    ViewPort*         view_port;
    Gui*              gui;
    uint32_t          last_tick_ms;
    uint32_t          anim_ms;
    int               map_page;
    int               rest_food_used;
    int               fort_cursor;
    uint32_t          hold_advance_ms;
    bool              ok_held;
    bool              confirm_exit;     // show "are you sure?" overlay on trail
    int               dead_player_idx;
} App;

// ── Forward declarations ──────────────────────────────────────
static void draw_title      (Canvas* c, uint32_t ms);
static void draw_name_edit  (Canvas* c, const GameState* gs, const NameEditState* ne);
static void draw_trail      (Canvas* c, const GameState* gs, bool confirm_exit);
static void draw_fort_arrival (Canvas* c, const GameState* gs);
static void draw_fort_store   (Canvas* c, const GameState* gs, int cursor);
static void draw_map        (Canvas* c, const GameState* gs, int page);
static void draw_rest_card  (Canvas* c, const GameState* gs, int food_used);
static void draw_death_card (Canvas* c, const GameState* gs, int player_idx);
static void draw_game_over  (Canvas* c);
static void draw_win        (Canvas* c, const GameState* gs);

// ── Input callback ────────────────────────────────────────────
static void input_callback(InputEvent* event, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->input_queue, event, 0);
}

// ── Draw callback ─────────────────────────────────────────────
static void draw_callback(Canvas* c, void* ctx) {
    App* app = ctx;
    const GameState* gs = &app->gs;

    switch(gs->screen) {
        case SCREEN_TITLE:     draw_title(c, app->anim_ms);                          break;
        case SCREEN_NAME_EDIT: draw_name_edit(c, gs, &app->name_edit);               break;
        case SCREEN_TRAIL:     draw_trail(c, gs, app->confirm_exit);               break;
        case SCREEN_EVENT:     event_draw(c, &app->active_event, gs);                break;
        case SCREEN_HUNT:      hunt_draw (c, &app->hunt, gs);                        break;
        case SCREEN_RIVER:         river_draw(c, &app->river, gs);                  break;
        case SCREEN_RIVER_OUTCOME: river_draw_outcome(c, &app->river, gs);           break;
        case SCREEN_MAP:       draw_map(c, gs, app->map_page);                       break;
        case SCREEN_REST:      draw_rest_card(c, gs, app->rest_food_used);           break;
        case SCREEN_FORT_ARRIVAL: draw_fort_arrival(c, gs);                          break;
        case SCREEN_FORT:      draw_fort_store(c, gs, app->fort_cursor);             break;
        case SCREEN_DEATH:     draw_death_card(c, gs, app->dead_player_idx);         break;
        case SCREEN_GAME_OVER: draw_game_over(c);                                    break;
        case SCREEN_WIN:       draw_win(c, gs);                                      break;
    }
}

// ── Title screen ──────────────────────────────────────────────
// Two mountain layers at different scroll speeds behind a fixed wagon.
static int mountain_height(int x, float scroll, float amp, float freq, float phase) {
    float h = amp * fabsf(sinf((x + scroll) * freq + phase));
    return (int)h;
}

static void draw_title(Canvas* c, uint32_t ms) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    float far_scroll  = ms * 0.008f;
    float near_scroll = ms * 0.022f;

    // Far mountains (shorter, slower)
    for(int x = 0; x < 128; x++) {
        int h = mountain_height(x, far_scroll, 5.0f, 0.055f, 0.3f);
        if(h > 0) canvas_draw_box(c, x, 36 - h, 1, h);
    }
    // Near mountains (taller, faster)
    for(int x = 0; x < 128; x++) {
        int h = mountain_height(x, near_scroll, 10.0f, 0.088f, 1.4f);
        if(h > 0) canvas_draw_box(c, x, 36 - h, 1, h);
    }

    // Ground strip
    canvas_draw_box(c, 0, 36, 128, 4);
    // Moving trail dots
    int dot_offset = (int)(ms / 40) % 6;
    for(int x = -dot_offset; x < 128; x += 6) {
        if(x >= 0) canvas_draw_box(c, x, 37, 3, 1);
    }

    // Wagon (centered, static)
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 50, 23, 28, 14); // clear area first
    canvas_set_color(c, ColorBlack);
    sprite_wagon(c, 50, 23);

    // Title text — single line
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 4, AlignCenter, AlignTop, "OREGON TRAIL");

    // Divider
    canvas_draw_line(c, 10, 18, 117, 18);

    // Footer — year above prompt
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignTop, "1848");
    canvas_draw_str_aligned(c, 64, 55, AlignCenter, AlignTop, "PRESS OK TO START");
}

// ── Name edit screen ──────────────────────────────────────────
// Character set cycled with Up/Down in edit mode
static const char NAME_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz 0123456789-'";
#define NUM_NAME_CHARS ((int)(sizeof(NAME_CHARS)-1))

static int char_index(char ch) {
    for(int i = 0; i < NUM_NAME_CHARS; i++)
        if(NAME_CHARS[i] == ch) return i;
    return 0;
}

static void draw_name_edit(Canvas* c, const GameState* gs, const NameEditState* ne) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // Header
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, "PLAYER NAMES");
    canvas_set_color(c, ColorBlack);

    if(ne->editing) {
        // ── Character edit sub-mode ──────────────────────────
        canvas_draw_str_aligned(c, 64, 12, AlignCenter, AlignTop, "EDIT NAME:");

        // Draw name with cursor box under active character
        int len = (int)strlen(ne->edit_buf);
        int char_w = 7, start_x = 8;
        for(int i = 0; i < NAME_LEN-1; i++) {
            char ch_str[2] = { i < len ? ne->edit_buf[i] : '_', 0 };
            int cx = start_x + i * char_w;
            if(i == ne->char_pos) {
                canvas_draw_box(c, cx-1, 22, char_w, 9);
                canvas_set_color(c, ColorWhite);
                canvas_draw_str_aligned(c, cx+2, 23, AlignCenter, AlignTop, ch_str);
                canvas_set_color(c, ColorBlack);
            } else {
                canvas_draw_str_aligned(c, cx+2, 23, AlignCenter, AlignTop, ch_str);
            }
        }

        canvas_draw_line(c, 0, 34, 127, 34);
        canvas_draw_str_aligned(c, 64, 36, AlignCenter, AlignTop, "\x12\x13 change char");
        canvas_draw_str_aligned(c, 64, 45, AlignCenter, AlignTop, "\x14\x15 move  OK: save");
    } else {
        // ── Name select mode ─────────────────────────────────
        canvas_draw_line(c, 0, 37, 127, 37);  // divider before Start
        canvas_draw_line(c, 0, 48, 127, 48);  // divider before Sound

        const char* rows[4] = {
            gs->players[0].name,
            gs->players[1].name,
            "Start Game",
            sound_is_on() ? "Sound: ON" : "Sound: OFF"
        };
        int ys[4] = { 14, 25, 39, 50 };

        for(int i = 0; i < 4; i++) {
            char label[NAME_LEN + 8];
            if(i < 2) {
                snprintf(label, sizeof(label), "P%d: %s", i+1, rows[i]);
            } else {
                strncpy(label, rows[i], sizeof(label)-1);
                label[sizeof(label)-1] = '\0';
            }
            if(ne->cursor == i) {
                canvas_draw_box(c, 0, ys[i]-1, 128, 9);
                canvas_set_color(c, ColorWhite);
                canvas_draw_str_aligned(c, 4, ys[i], AlignLeft, AlignTop, "\x15");
                canvas_draw_str_aligned(c, 14, ys[i], AlignLeft, AlignTop, label);
                canvas_set_color(c, ColorBlack);
            } else {
                canvas_draw_str_aligned(c, 14, ys[i], AlignLeft, AlignTop, label);
            }
        }
    }
}

// ── Landmark table ────────────────────────────────────────────
#define NUM_LANDMARKS 11
typedef struct { const char* name; const char* abbr; uint32_t miles; } Landmark;
static const Landmark LANDMARKS[NUM_LANDMARKS] = {
    {"Independence",  "START",    0},
    {"Ft. Kearney",   "F.KEA",  307},
    {"Chimney Rock",  "CHIM.",   554},
    {"Ft. Laramie",   "F.LAR",   640},
    {"South Pass",    "S.PAS",   947},
    {"Ft. Bridger",   "F.BRI",  1070},
    {"Snake River",   "SNAKE",  1288},
    {"Ft. Boise",     "F.BOI",  1438},
    {"Blue Mtns.",    "BLUE.",  1600},
    {"The Dalles",    "DALL.",  1800},
    {"Oregon City",   "OREG!",  2000},
};
static int landmark_idx(uint32_t miles) {
    int idx = 0;
    for(int i = 0; i < NUM_LANDMARKS; i++)
        if(miles >= LANDMARKS[i].miles) idx = i;
    return idx;
}
// Scale miles to map x (4px margin each side, 120px usable)
static int map_x_pos(uint32_t miles) {
    return 4 + (int)((float)miles / 2000.0f * 120.0f);
}

// ── Regional backdrops  (viewport y=18..38) ───────────────────
static void draw_backdrop_plains(Canvas* c) {
    for(int x = 0; x < 128; x++) {
        int h = (int)(3.0f * fabsf(sinf(x * 0.04f + 0.5f)));
        if(h) canvas_draw_box(c, x, 33-h, 1, h);
    }
    static const int tx[] = {8, 22, 45, 67, 89, 105, 118};
    for(int i = 0; i < 7; i++) {
        canvas_draw_box(c, tx[i],   36, 1, 2);
        canvas_draw_box(c, tx[i]-1, 37, 1, 1);
        canvas_draw_box(c, tx[i]+2, 37, 1, 1);
    }
}
static void draw_backdrop_hills(Canvas* c) {
    for(int x = 0; x < 128; x++) {
        int h = (int)(9.0f * fabsf(sinf(x * 0.055f + 1.0f)));
        if(h) canvas_draw_box(c, x, 34-h, 1, h);
    }
}
static void draw_backdrop_mountains(Canvas* c) {
    static const int mxs[] = {20, 54, 90, 114};
    static const int mhs[] = {7,  5,  9,   4};
    for(int m = 0; m < 4; m++)
        for(int i = 0; i < mhs[m]; i++)
            canvas_draw_box(c, mxs[m]-i, 18+i, 2*i+1, 1);
}
static void draw_cactus(Canvas* c, int x, int y) {
    canvas_draw_box(c, x+2, y,   1, 8);
    canvas_draw_box(c, x,   y+2, 3, 1);
    canvas_draw_box(c, x,   y+2, 1, 3);
    canvas_draw_box(c, x+3, y+4, 3, 1);
    canvas_draw_box(c, x+5, y+4, 1, 3);
}
static void draw_backdrop_desert(Canvas* c) {
    for(int x = 0; x < 128; x++) {
        int h = (int)(2.0f * fabsf(sinf(x * 0.03f + 2.0f)));
        if(h) canvas_draw_box(c, x, 35-h, 1, h);
    }
    static const int cx[] = {15, 55, 95};
    for(int i = 0; i < 3; i++) draw_cactus(c, cx[i], 28);
}
static void draw_pine(Canvas* c, int x, int y, int h) {
    for(int i = 0; i < h; i++)
        canvas_draw_box(c, x-i, y+i, 2*i+1, 1);
    canvas_draw_box(c, x, y+h, 1, 3);
}
static void draw_backdrop_forest(Canvas* c) {
    draw_pine(c, 12, 20, 7);  draw_pine(c, 32, 22, 5);
    draw_pine(c, 58, 18, 9);  draw_pine(c, 82, 21, 6);
    draw_pine(c, 102, 19, 8); draw_pine(c, 120, 23, 4);
}
static void draw_backdrop(Canvas* c, uint32_t miles) {
    Region r = region_from_miles(miles);
    switch(r) {
        case REGION_PRAIRIE:
        case REGION_PLAINS:     draw_backdrop_plains(c);    break;
        case REGION_HIGH_PLAINS:draw_backdrop_hills(c);     break;
        case REGION_MOUNTAINS:  draw_backdrop_mountains(c); break;
        case REGION_DESERT:     draw_backdrop_desert(c);    break;
        case REGION_FOREST:     draw_backdrop_forest(c);    break;
        default:                draw_backdrop_plains(c);    break;
    }
}

// ── Trail status screen ───────────────────────────────────────
static const char* month_name(int m) {
    static const char* MONTHS[] = {
        "","JAN","FEB","MAR","APR","MAY","JUN",
        "JUL","AUG","SEP","OCT","NOV","DEC"
    };
    return (m >= 1 && m <= 12) ? MONTHS[m] : "?";
}

static void draw_trail(Canvas* c, const GameState* gs, bool confirm_exit) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    const Trail* t = &gs->trail;

    // ── Bar 1: Landmark + money (inverted) ───────────────────
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64,  1, AlignCenter, AlignTop,
                            LANDMARKS[landmark_idx(t->miles)].name);
    char money_str[8];
    snprintf(money_str, sizeof(money_str), "$%d", t->money);
    canvas_draw_str_aligned(c, 126, 1, AlignRight,  AlignTop, money_str);
    canvas_set_color(c, ColorBlack);

    // ── Bar 2: Date + miles (plain, y=9-16) ──────────────────
    char date_str[12];
    snprintf(date_str, sizeof(date_str), "%s %d", month_name(t->month), t->day);
    canvas_draw_str_aligned(c, 2,   10, AlignLeft,  AlignTop, date_str);
    char miles_str[24];
    snprintf(miles_str, sizeof(miles_str), "%lu of 2000 mi.", (unsigned long)t->miles);
    canvas_draw_str_aligned(c, 126, 10, AlignRight, AlignTop, miles_str);
    canvas_draw_line(c, 0, 18, 127, 18);

    // ── Regional backdrop (y=18..38) ─────────────────────────
    draw_backdrop(c, t->miles);

    // Ground line + trail dots
    canvas_draw_box(c, 0, 39, 128, 2);
    for(int x = 4;  x < 38;  x += 4) canvas_draw_box(c, x, 40, 2, 1);
    for(int x = 68; x < 124; x += 4) canvas_draw_box(c, x, 40, 2, 1);

    // Wagon — top at y=25, wheels land at y=38
    sprite_wagon(c, 43, 25);

    // ── Food bar (y=41-49) ────────────────────────────────────
    canvas_draw_line(c, 0, 41, 127, 41);
    canvas_set_font(c, FontSecondary);
    char food_str[16];
    snprintf(food_str, sizeof(food_str), "Food: %d lb", t->food_lbs);
    canvas_draw_str_aligned(c, 2,   43, AlignLeft,  AlignTop, food_str);
    canvas_draw_str_aligned(c, 126, 43, AlignRight, AlignTop, "OK:Go Rt:Hunt");

    // ── Player HP columns (2 × 64px, single line) ────────────
    canvas_draw_line(c, 0, 51, 127, 51);
    canvas_draw_line(c, 64, 52, 64, 63);

    for(int i = 0; i < gs->num_players; i++) {
        const Player* p = &gs->players[i];
        int sx = i * 64 + 2;

        // Name — up to 9 chars, 1px higher
        char disp[10];
        strncpy(disp, p->name, 9); disp[9] = '\0';
        canvas_draw_str_aligned(c, sx, 54, AlignLeft, AlignTop, disp);

        if(p->hp <= 0.0f) {
            sprite_skull(c, sx + 50, 56);
        } else {
            for(int b = 0; b < MAX_HP; b++) {
                int bx = sx + 62 - (MAX_HP - b) * 4;
                float threshold = (float)(b + 1);
                float half      = (float)b + 0.5f;
                canvas_draw_box(c, bx, 56, 3, 5);
                if(p->hp >= threshold) {
                    // full
                } else if(p->hp >= half) {
                    canvas_set_color(c, ColorWhite);
                    canvas_draw_box(c, bx+1, 59, 1, 2);
                    canvas_set_color(c, ColorBlack);
                } else {
                    canvas_set_color(c, ColorWhite);
                    canvas_draw_box(c, bx+1, 57, 1, 3);
                    canvas_set_color(c, ColorBlack);
                }
            }
        }
    }

    // ── Confirm exit overlay ──────────────────────────────────
    if(confirm_exit) {
        // Semi-opaque dialog box centered on screen
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 18, 20, 92, 26);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, 18, 20, 92, 26);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 23, AlignCenter, AlignTop, "Exit game?");
        canvas_draw_str_aligned(c, 64, 33, AlignCenter, AlignTop, "OK:Yes  Back:No");
    }
}

// ── Fort arrival splash ───────────────────────────────────────
static void draw_fort_arrival(Canvas* c, const GameState* gs) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    // Inverted header with fort name
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop,
                            LANDMARKS[landmark_idx(gs->trail.miles)].name);
    canvas_set_color(c, ColorBlack);

    // Wagon sprite
    sprite_wagon(c, 51, 12);

    // Arrival message
    canvas_draw_str_aligned(c, 64, 30, AlignCenter, AlignTop, "You have arrived!");
    canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignTop, "Supplies available.");

    canvas_draw_line(c, 0, 50, 127, 50);
    canvas_draw_str_aligned(c, 64, 53, AlignCenter, AlignTop,
                            "Right:Trade  Back:Resume");
}

// ── Fort store ────────────────────────────────────────────────
// Store items are built dynamically based on current state.
// Up to 6 items. Prices: buy high, sell low.

#define FORT_MAX_ITEMS 6
typedef struct {
    char  label[32];
    int   food_delta;   // positive = gain food
    int   ammo_delta;
    int   money_delta;  // positive = gain money
    float hp_delta[MAX_PLAYERS]; // per-player HP change
    bool  available;
} FortItem;

static int build_fort_items(FortItem items[], const GameState* gs) {
    int n = 0;
    const Trail* t = &gs->trail;

    // Buy food
    snprintf(items[n].label, 32, "Buy food 40lb    $15");
    items[n] = (FortItem){ .food_delta=40, .money_delta=-15, .available=(t->money>=15) };
    snprintf(items[n].label, 32, "Buy food 40lb    $15");
    n++;

    // Sell food
    if(t->food_lbs >= 40) {
        items[n] = (FortItem){ .food_delta=-40, .money_delta=6, .available=true };
        snprintf(items[n].label, 32, "Sell food 40lb  +$6");
        n++;
    }

    // Buy ammo
    items[n] = (FortItem){ .ammo_delta=20, .money_delta=-10, .available=(t->money>=10) };
    snprintf(items[n].label, 32, "Buy ammo 20rds  $10");
    n++;

    // Sell ammo
    if(t->ammo >= 20) {
        items[n] = (FortItem){ .ammo_delta=-20, .money_delta=4, .available=true };
        snprintf(items[n].label, 32, "Sell ammo 20rds +$4");
        n++;
    }

    // Medicine for each living player below max HP
    for(int i = 0; i < gs->num_players && n < FORT_MAX_ITEMS; i++) {
        const Player* p = &gs->players[i];
        if(p->hp > 0.0f && p->hp < MAX_HP_F) {
            items[n] = (FortItem){ .money_delta=-20, .available=(t->money>=20) };
            items[n].hp_delta[i] = 2.0f;
            snprintf(items[n].label, 32, "Medicine: %s  $20", p->name);
            n++;
        }
    }

    return n;
}

static void draw_fort_store(Canvas* c, const GameState* gs, int cursor) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    // Header
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    const char* fort_name = LANDMARKS[landmark_idx(gs->trail.miles)].name;
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, fort_name);
    canvas_set_color(c, ColorBlack);

    // Money and ammo status
    char status[32];
    snprintf(status, sizeof(status), "Food:%d Ammo:%d $%d",
             gs->trail.food_lbs, gs->trail.ammo, gs->trail.money);
    canvas_draw_str_aligned(c, 126, 10, AlignRight, AlignTop, status);
    canvas_draw_line(c, 0, 17, 127, 17);

    // Build items
    FortItem items[FORT_MAX_ITEMS];
    int n = build_fort_items(items, gs);

    // Scroll window: 4 visible, keep cursor in view
    int visible = 4;
    int scroll_top = cursor - 1;          // try to keep cursor on row 2
    if(scroll_top > n - visible) scroll_top = n - visible;
    if(scroll_top < 0)           scroll_top = 0;

    // Scroll indicators
    canvas_set_font(c, FontSecondary);
    if(scroll_top > 0)
        canvas_draw_str_aligned(c, 126, 19, AlignRight, AlignTop, "^");
    if(scroll_top + visible < n)
        canvas_draw_str_aligned(c, 126, 46, AlignRight, AlignTop, "v");

    // Draw items
    for(int i = 0; i < visible; i++) {
        int idx = scroll_top + i;
        if(idx >= n) break;
        int y = 19 + i * 9;
        bool selected = (idx == cursor);
        bool avail    = items[idx].available;

        if(selected) {
            canvas_draw_box(c, 0, y-1, 128, 9);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str_aligned(c, selected ? 8 : 4, y, AlignLeft, AlignTop, items[idx].label);
        if(selected)  canvas_draw_str_aligned(c, 4, y, AlignLeft, AlignTop, "\x15");
        if(!avail && !selected) canvas_draw_box(c, 2, y+4, 4, 1); // dim marker
        canvas_set_color(c, ColorBlack);
    }

    canvas_draw_line(c, 0, 55, 127, 55);
    canvas_draw_str_aligned(c, 64, 57, AlignCenter, AlignTop,
                            "OK:Buy  Up/Dn:Sel  Back:Go");
}

// ── Map screen ────────────────────────────────────────────────
static void draw_map(Canvas* c, const GameState* gs, int page) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // Header
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str_aligned(c, 64,  1, AlignCenter, AlignTop, "TRAIL MAP");
    const char* pg_str = (page == 0) ? "1/3" : (page == 1) ? "2/3" : "3/3";
    canvas_draw_str_aligned(c, 126, 1, AlignRight,  AlignTop, pg_str);
    canvas_set_color(c, ColorBlack);

    uint32_t miles = gs->trail.miles;
    int cur = landmark_idx(miles);

    if(page == 0) {
        // ── Visual trail map ─────────────────────────────────
        // Trail line at y=28
        canvas_draw_line(c, 4, 28, 124, 28);

        // Landmark ticks and dots
        for(int i = 0; i < NUM_LANDMARKS; i++) {
            int lx = map_x_pos(LANDMARKS[i].miles);
            bool passed = (miles >= LANDMARKS[i].miles);
            canvas_draw_box(c, lx, 26, 1, 4);          // tick
            if(passed) {
                canvas_draw_box(c, lx-1, 30, 3, 3);    // filled dot
            } else {
                canvas_draw_frame(c, lx-1, 30, 3, 3);  // open dot
            }
        }

        // Current position marker (▲ above trail)
        int cx = map_x_pos(miles);
        canvas_draw_box(c, cx,   22, 1, 1);
        canvas_draw_box(c, cx-1, 23, 3, 1);
        canvas_draw_box(c, cx-2, 24, 5, 1);

        // Current and next stop labels
        canvas_draw_line(c, 0, 35, 127, 35);
        canvas_draw_str_aligned(c, 2,   37, AlignLeft,  AlignTop, LANDMARKS[cur].abbr);
        if(cur < NUM_LANDMARKS-1) {
            uint32_t dist = LANDMARKS[cur+1].miles - miles;
            char nxt[24];
            snprintf(nxt, sizeof(nxt), "%s %lu mi", LANDMARKS[cur+1].abbr,
                     (unsigned long)dist);
            canvas_draw_str_aligned(c, 126, 37, AlignRight, AlignTop, nxt);
        }

        canvas_draw_line(c, 0, 46, 127, 46);
        canvas_draw_str_aligned(c, 64, 48, AlignCenter, AlignTop,
                                "Up/Dn: Page  Back: Return");

    } else if(page == 1) {
        // ── Text details ─────────────────────────────────────
        uint32_t to_go = (miles < 2000) ? 2000 - miles : 0;

        canvas_draw_str_aligned(c, 2, 11, AlignLeft, AlignTop, "NEAR:");
        canvas_draw_str_aligned(c, 40, 11, AlignLeft, AlignTop, LANDMARKS[cur].name);

        char tr_str[24];
        snprintf(tr_str, sizeof(tr_str), "%lu mi traveled", (unsigned long)miles);
        canvas_draw_str_aligned(c, 2, 20, AlignLeft, AlignTop, tr_str);

        char og_str[24];
        snprintf(og_str, sizeof(og_str), "%lu mi to Oregon", (unsigned long)to_go);
        canvas_draw_str_aligned(c, 2, 29, AlignLeft, AlignTop, og_str);

        canvas_draw_line(c, 0, 38, 127, 38);

        if(cur < NUM_LANDMARKS-1) {
            uint32_t dist = LANDMARKS[cur+1].miles - miles;
            char ns_str[32];
            snprintf(ns_str, sizeof(ns_str), "Next: %s", LANDMARKS[cur+1].name);
            char ds[16];
            snprintf(ds, sizeof(ds), "%lu mi", (unsigned long)dist);
            canvas_draw_str_aligned(c, 2,   40, AlignLeft,  AlignTop, ns_str);
            canvas_draw_str_aligned(c, 126, 40, AlignRight, AlignTop, ds);
        } else {
            canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignTop, "YOU MADE IT!");
        }

        canvas_draw_line(c, 0, 50, 127, 50);
        canvas_draw_str_aligned(c, 64, 53, AlignCenter, AlignTop,
                                "Up/Dn: Page  Back: Return");

    } else if(page == 2) {
        // ── Page 3: Trail settings ────────────────────────────
        // settings_cursor: 0=pace selected, 1=rations selected
        canvas_draw_str_aligned(c, 64, 11, AlignCenter, AlignTop, "TRAIL SETTINGS");
        canvas_draw_line(c, 0, 19, 127, 19);

        // Pace column (left)
        static const char* pace_names[]  = { "", "Steady", "Strenuous", "Grueling" };
        static const char* pace_miles[]  = { "", "15 mi/day", "22 mi/day", "30 mi/day" };
        static const char* ration_names[]= { "", "Filling", "Meager", "Bare Bones" };
        static const char* ration_lbs[]  = { "", "4 lb/day", "2 lb/day", "1 lb/day" };

        bool pace_sel   = (gs->settings_cursor == 0);
        bool ration_sel = (gs->settings_cursor == 1);

        // Pace block
        if(pace_sel) {
            canvas_draw_box(c, 0, 21, 62, 10);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str_aligned(c, 2, 23, AlignLeft, AlignTop,
                                pace_sel ? "\x15" : " ");
        canvas_draw_str_aligned(c, 10, 23, AlignLeft, AlignTop,
                                pace_names[gs->trail.pace]);
        canvas_set_color(c, ColorBlack);
        canvas_draw_str_aligned(c, 10, 33, AlignLeft, AlignTop,
                                pace_miles[gs->trail.pace]);

        // Vertical divider
        canvas_draw_line(c, 63, 19, 63, 56);

        // Rations block
        if(ration_sel) {
            canvas_draw_box(c, 64, 21, 64, 10);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str_aligned(c, 66, 23, AlignLeft, AlignTop,
                                ration_sel ? "\x15" : " ");
        canvas_draw_str_aligned(c, 74, 23, AlignLeft, AlignTop,
                                ration_names[gs->trail.rations]);
        canvas_set_color(c, ColorBlack);
        canvas_draw_str_aligned(c, 74, 33, AlignLeft, AlignTop,
                                ration_lbs[gs->trail.rations]);

        // Health effect labels
        static const char* pace_fx[]   = { "", "Safe", "Tiring", "Dangerous" };
        static const char* ration_fx[] = { "", "Healthy", "Slow drain", "Fast drain" };
        canvas_draw_str_aligned(c, 10, 43, AlignLeft, AlignTop, pace_fx[gs->trail.pace]);
        canvas_draw_str_aligned(c, 74, 43, AlignLeft, AlignTop, ration_fx[gs->trail.rations]);

        canvas_draw_line(c, 0, 55, 127, 55);
        canvas_draw_str_aligned(c, 64, 57, AlignCenter, AlignTop,
                                "LR:Select  OK:Change  Up:Back");
    }
}

// ── Rest day card ─────────────────────────────────────────────
static void draw_rest_card(Canvas* c, const GameState* gs, int food_used) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, 0, 128, 64);
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "REST DAY");
    canvas_set_color(c, ColorBlack);

    canvas_draw_str_aligned(c, 64, 13, AlignCenter, AlignTop, "The party rested.");
    canvas_draw_str_aligned(c, 64, 21, AlignCenter, AlignTop, "+0.5 HP each player.");

    canvas_draw_line(c, 1, 30, 126, 30);

    // Player HP bars after rest
    for(int i = 0; i < gs->num_players; i++) {
        const Player* p = &gs->players[i];
        int y = 32 + i * 10;
        char disp[7];
        strncpy(disp, p->name, 6); disp[6] = '\0';
        canvas_draw_str_aligned(c, 3, y, AlignLeft, AlignTop, disp);
        for(int b = 0; b < MAX_HP; b++) {
            int bx = 54 + b * 7;
            canvas_draw_box(c, bx, y, 6, 6);
            if((float)(b + 1) > p->hp) {
                canvas_set_color(c, ColorWhite);
                canvas_draw_box(c, bx+1, y+1, 4, 4);
                canvas_set_color(c, ColorBlack);
            }
        }
    }

    canvas_draw_line(c, 1, 52, 126, 52);
    char food_str[20];
    snprintf(food_str, sizeof(food_str), "Food: -%d lb", food_used);
    canvas_draw_str_aligned(c, 3,   54, AlignLeft,  AlignTop, food_str);
    canvas_draw_str_aligned(c, 125, 54, AlignRight, AlignTop, "OK: Continue");
}

// ── Death card ────────────────────────────────────────────────
static void draw_death_card(Canvas* c, const GameState* gs, int player_idx) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, 0, 128, 64);
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "OBITUARY");
    canvas_set_color(c, ColorBlack);

    // Tombstone sprite centered left
    sprite_tombstone(c, 10, 13);

    // Name and flavor text right of tombstone
    const char* name = (player_idx >= 0 && player_idx < gs->num_players)
                       ? gs->players[player_idx].name : "Unknown";
    char line1[32];
    snprintf(line1, sizeof(line1), "%s has died.", name);
    canvas_draw_str_aligned(c, 32, 13, AlignLeft, AlignTop, line1);
    canvas_draw_str_aligned(c, 32, 21, AlignLeft, AlignTop, "3 days for burial,");
    canvas_draw_str_aligned(c, 32, 29, AlignLeft, AlignTop, "and the journey");
    canvas_draw_str_aligned(c, 32, 37, AlignLeft, AlignTop, "goes on.");

    canvas_draw_line(c, 1, 47, 126, 47);
    canvas_draw_str_aligned(c, 125, 50, AlignRight, AlignTop, "OK: Continue");
}

// ── Game over card ────────────────────────────────────────────
static void draw_game_over(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, 0, 128, 64);
    canvas_draw_box(c, 1, 1, 126, 10);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "GAME OVER");
    canvas_set_color(c, ColorBlack);

    // Two tombstones side by side
    sprite_tombstone(c, 20, 13);
    sprite_tombstone(c, 52, 13);

    canvas_draw_str_aligned(c, 64, 34, AlignCenter, AlignTop, "Your party has");
    canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignTop, "perished on the trail.");

    canvas_draw_line(c, 1, 52, 126, 52);
    canvas_draw_str_aligned(c, 64, 55, AlignCenter, AlignTop, "OK: Start Over");
}

// ── Win screen ────────────────────────────────────────────────
static void draw_win(Canvas* c, const GameState* gs) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // Inverted header
    canvas_draw_box(c, 0, 0, 128, 9);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, "OREGON CITY!");
    canvas_set_color(c, ColorBlack);

    // Wagon + celebration
    sprite_wagon(c, 51, 11);   // wagon prominently centered

    canvas_draw_str_aligned(c, 64, 27, AlignCenter, AlignTop, "You made it!");
    canvas_draw_str_aligned(c, 64, 35, AlignCenter, AlignTop, "The Willamette");
    canvas_draw_str_aligned(c, 64, 43, AlignCenter, AlignTop, "Valley awaits.");

    // Player HP summary
    canvas_draw_line(c, 0, 52, 127, 52);
    for(int i = 0; i < gs->num_players; i++) {
        const Player* p = &gs->players[i];
        int sx = i * 64 + 2;
        char disp[7];
        strncpy(disp, p->name, 6); disp[6] = '\0';
        canvas_draw_str_aligned(c, sx, 54, AlignLeft, AlignTop, disp);

        if(p->hp <= 0.0f) {
            sprite_skull(c, sx + 50, 54);
        } else {
            for(int b = 0; b < MAX_HP; b++) {
                int bx = sx + 62 - (MAX_HP - b) * 4;
                canvas_draw_box(c, bx, 54, 3, 5);
                float threshold = (float)(b + 1);
                float half      = (float)b + 0.5f;
                if(p->hp < threshold) {
                    canvas_set_color(c, ColorWhite);
                    canvas_draw_box(c, bx+1, p->hp >= half ? 57 : 55, 1, p->hp >= half ? 2 : 3);
                    canvas_set_color(c, ColorBlack);
                }
            }
        }
    }
}

// ── River crossing ─────────────────────────────────────────────
// Content area: y=10..63 (54px).
// Info panel: y=10..34 (24px). Options: y=35..63 (28px, ~9px per option).
// ── Game reset ────────────────────────────────────────────────
static void game_reset(App* app) {
    char name0[NAME_LEN], name1[NAME_LEN];
    strncpy(name0, app->gs.players[0].name, NAME_LEN-1); name0[NAME_LEN-1]='\0';
    strncpy(name1, app->gs.players[1].name, NAME_LEN-1); name1[NAME_LEN-1]='\0';

    app->gs.screen      = SCREEN_TRAIL;
    app->gs.num_players = 2;
    strncpy(app->gs.players[0].name, name0, NAME_LEN-1);
    strncpy(app->gs.players[1].name, name1, NAME_LEN-1);
    app->gs.players[0].hp    = MAX_HP_F;
    app->gs.players[0].alive = true;
    app->gs.players[1].hp    = MAX_HP_F;
    app->gs.players[1].alive = true;
    app->gs.trail = (Trail){
        .food_lbs=250, .money=124, .day=3, .month=5, .miles=0,
        .pace=1, .rations=1, .recent_rain=false,
        .ammo=20, .last_fort_visited=-1, .rivers_visited=0
    };
    memset(&app->active_event, 0, sizeof(ActiveEvent));
    hunt_init(&app->hunt);
    day_init();
    app->dead_player_idx = -1;
    app->ok_held         = false;
    app->hold_advance_ms = 0;
    app->fort_cursor     = 0;
    app->confirm_exit    = false;
    app->gs.trail.rivers_visited = 0;
}

// ── Game loop ─────────────────────────────────────────────────
int32_t oregon_trail_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    // Init game state
    app->gs.screen      = SCREEN_TITLE;
    app->gs.running     = true;
    app->gs.num_players = 2;
    strncpy(app->gs.players[0].name, "Jeff",      NAME_LEN-1);
    strncpy(app->gs.players[1].name, "Offspring", NAME_LEN-1);
    app->gs.players[0].hp    = MAX_HP_F;
    app->gs.players[0].alive = true;
    app->gs.players[1].hp    = MAX_HP_F;
    app->gs.players[1].alive = true;
    app->gs.trail = (Trail){
        .food_lbs=250, .money=124, .day=3, .month=5, .miles=0,
        .pace=1, .rations=1, .recent_rain=false,
        .ammo=20, .last_fort_visited=-1, .rivers_visited=0
    };

    // Name edit state — cursor starts on "Start Game" (row 2)
    app->name_edit.cursor      = 2;
    app->name_edit.editing     = false;
    app->name_edit.char_pos    = 0;
    app->name_edit.edit_player = 0;

    hunt_init(&app->hunt);
    day_init();
    sound_init(true);
    memset(&app->active_event, 0, sizeof(ActiveEvent));

    // Flipper GUI setup
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port   = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->last_tick_ms    = furi_get_tick();
    app->hold_advance_ms = 0;
    app->ok_held         = false;
    app->dead_player_idx = -1;

    // ── Main loop ─────────────────────────────────────────────
    while(app->gs.running) {
        uint32_t now = furi_get_tick();
        uint32_t dt  = now - app->last_tick_ms;
        if(dt > 50) dt = 50; // cap at 50ms (~20 fps min)
        app->last_tick_ms = now;
        app->anim_ms     += dt;

        // ── Process input ────────────────────────────────────
        InputEvent ev;
        while(furi_message_queue_get(app->input_queue, &ev, 0) == FuriStatusOk) {
            bool is_press   = (ev.type == InputTypePress);
            bool is_release = (ev.type == InputTypeRelease);
            if(!is_press && !is_release) continue;

            // Track OK held state for trail fast-advance
            if(ev.key == InputKeyOk) {
                if(is_press)   app->ok_held = true;
                if(is_release) { app->ok_held = false; app->hold_advance_ms = 0; }
            }

            switch(app->gs.screen) {
                case SCREEN_TITLE:
                    if(is_press && ev.key == InputKeyOk)
                        app->gs.screen = SCREEN_NAME_EDIT;
                    if(is_press && ev.key == InputKeyBack)
                        app->gs.running = false;
                    break;

                case SCREEN_NAME_EDIT: {
                    if(!is_press) break;
                    NameEditState* ne = &app->name_edit;
                    if(ne->editing) {
                        Player* p = &app->gs.players[ne->edit_player];
                        int len = (int)strlen(ne->edit_buf);
                        if(ev.key == InputKeyUp) {
                            char cur = ne->char_pos < len ? ne->edit_buf[ne->char_pos] : 'A';
                            int idx = (char_index(cur) + 1) % NUM_NAME_CHARS;
                            if(ne->char_pos >= len) {
                                ne->edit_buf[ne->char_pos] = NAME_CHARS[idx];
                                ne->edit_buf[ne->char_pos+1] = '\0';
                            } else {
                                ne->edit_buf[ne->char_pos] = NAME_CHARS[idx];
                            }
                        } else if(ev.key == InputKeyDown) {
                            char cur = ne->char_pos < len ? ne->edit_buf[ne->char_pos] : 'A';
                            int idx = (char_index(cur) - 1 + NUM_NAME_CHARS) % NUM_NAME_CHARS;
                            if(ne->char_pos >= len) {
                                ne->edit_buf[ne->char_pos] = NAME_CHARS[idx];
                                ne->edit_buf[ne->char_pos+1] = '\0';
                            } else {
                                ne->edit_buf[ne->char_pos] = NAME_CHARS[idx];
                            }
                        } else if(ev.key == InputKeyRight) {
                            if(ne->char_pos < NAME_LEN-2) ne->char_pos++;
                        } else if(ev.key == InputKeyLeft) {
                            if(ne->char_pos > 0) ne->char_pos--;
                        } else if(ev.key == InputKeyOk) {
                            int end = (int)strlen(ne->edit_buf);
                            while(end > 0 && ne->edit_buf[end-1] == ' ') end--;
                            ne->edit_buf[end] = '\0';
                            if(end == 0)
                                strncpy(ne->edit_buf,
                                        ne->edit_player == 0 ? "Jeff" : "Offspring",
                                        NAME_LEN-1);
                            strncpy(p->name, ne->edit_buf, NAME_LEN-1);
                            ne->editing = false;
                        } else if(ev.key == InputKeyBack) {
                            ne->editing = false;
                        }
                    } else {
                        if(ev.key == InputKeyUp) {
                            if(ne->cursor > 0) ne->cursor--;
                        } else if(ev.key == InputKeyDown) {
                            if(ne->cursor < 3) ne->cursor++;
                        } else if(ev.key == InputKeyOk) {
                            if(ne->cursor == 2) {
                                game_reset(app);
                                app->gs.screen = SCREEN_TRAIL;
                            } else if(ne->cursor == 3) {
                                sound_toggle();
                            } else {
                                ne->edit_player = ne->cursor;
                                strncpy(ne->edit_buf,
                                        app->gs.players[ne->cursor].name,
                                        NAME_LEN-1);
                                ne->edit_buf[NAME_LEN-1] = '\0';
                                ne->char_pos = 0;
                                ne->editing  = true;
                            }
                        } else if(ev.key == InputKeyBack) {
                            app->gs.screen = SCREEN_TITLE;
                        }
                    }
                    break;
                }

                case SCREEN_TRAIL:
                    if(ev.key == InputKeyOk && is_press) {
                        if(app->confirm_exit) {
                            app->confirm_exit = false;
                            app->gs.screen = SCREEN_TITLE;
                        } else {
                            app->hold_advance_ms = 0;
                            sound_play(SND_TRAIL_ADVANCE);
                            DayResult result = day_advance(&app->gs, &app->active_event);
                        if(result == DAY_EVENT) {
                            // Good fortune events have "GOOD FORTUNE" header
                            bool good = (app->active_event.def &&
                                        app->active_event.def->header[0] == 'G');
                            sound_play(good ? SND_EVENT_GOOD : SND_EVENT_BAD);
                            app->gs.screen = SCREEN_EVENT;
                        }
                        else if(result == DAY_ARRIVED)  { sound_play(SND_WIN); app->gs.screen = SCREEN_WIN; }
                        else if(result == DAY_ALL_DEAD) { sound_play(SND_GAME_OVER); app->gs.screen = SCREEN_GAME_OVER; }
                        else {
                            // Fort check
                            static const int FORT_INDICES[] = {1, 3, 5, 7};
                            int li = landmark_idx(app->gs.trail.miles);
                            bool intercepted = false;
                            for(int f = 0; f < 4; f++) {
                                if(li == FORT_INDICES[f] &&
                                   app->gs.trail.last_fort_visited != li) {
                                    app->gs.trail.last_fort_visited = li;
                                    app->fort_cursor = 0;
                                    app->gs.screen = SCREEN_FORT_ARRIVAL;
                                    sound_play(SND_FORT_ARRIVE);
                                    furi_message_queue_reset(app->input_queue);
                                    intercepted = true;
                                    break;
                                }
                            }
                            // River check
                            if(!intercepted) {
                                for(int ri = 0; ri < NUM_RIVERS; ri++) {
                                    if(app->gs.trail.miles >= RIVERS[ri].miles &&
                                       !(app->gs.trail.rivers_visited & (1 << ri))) {
                                        app->gs.trail.rivers_visited |= (1 << ri);
                                        river_init(&app->river, ri, &app->gs.trail);
                                        app->gs.screen = SCREEN_RIVER;
                                        furi_message_queue_reset(app->input_queue);
                                        break;
                                    }
                                }
                            }
                        }
                        } // end else (not confirm_exit)
                    }
                    if(is_press && ev.key == InputKeyRight) {
                        if(app->confirm_exit) {
                            app->confirm_exit = false; // Back cancels
                        } else {
                            Region r = region_from_miles(app->gs.trail.miles);
                            bool off = (r != REGION_PRAIRIE && r != REGION_PLAINS);
                            hunt_set_region(&app->hunt, off);
                            app->gs.screen = SCREEN_HUNT;
                        }
                    }
                    if(is_press && ev.key == InputKeyDown && !app->confirm_exit) {
                        app->map_page  = 0;
                        app->gs.screen = SCREEN_MAP;
                    }
                    if(is_press && ev.key == InputKeyLeft && !app->confirm_exit) {
                        day_rest(&app->gs, &app->rest_food_used);
                        app->gs.screen = SCREEN_REST;
                    }
                    if(is_press && ev.key == InputKeyBack) {
                        if(app->confirm_exit) {
                            app->confirm_exit = false; // Back = cancel
                        } else {
                            app->confirm_exit = true;  // first Back = show dialog
                        }
                    }
                    break;

                case SCREEN_FORT_ARRIVAL:
                    if(!is_press) break;
                    if(ev.key == InputKeyRight) {
                        app->fort_cursor = 0;
                        app->gs.screen = SCREEN_FORT;
                    }
                    if(ev.key == InputKeyBack)
                        app->gs.screen = SCREEN_TRAIL;
                    break;

                case SCREEN_FORT: {
                    if(!is_press) break;
                    FortItem items[FORT_MAX_ITEMS];
                    int n = build_fort_items(items, &app->gs);
                    if(ev.key == InputKeyUp   && app->fort_cursor > 0)   app->fort_cursor--;
                    if(ev.key == InputKeyDown  && app->fort_cursor < n-1) app->fort_cursor++;
                    if(ev.key == InputKeyOk && app->fort_cursor < n) {
                        FortItem* it = &items[app->fort_cursor];
                        if(it->available) {
                            app->gs.trail.food_lbs += it->food_delta;
                            if(app->gs.trail.food_lbs < 0) app->gs.trail.food_lbs = 0;
                            app->gs.trail.ammo     += it->ammo_delta;
                            if(app->gs.trail.ammo < 0) app->gs.trail.ammo = 0;
                            app->gs.trail.money    += it->money_delta;
                            if(app->gs.trail.money < 0) app->gs.trail.money = 0;
                            for(int i = 0; i < app->gs.num_players; i++) {
                                if(it->hp_delta[i] != 0.0f) {
                                    app->gs.players[i].hp += it->hp_delta[i];
                                    if(app->gs.players[i].hp > MAX_HP_F)
                                        app->gs.players[i].hp = MAX_HP_F;
                                }
                            }
                        }
                        // Rebuild item count after transaction
                        n = build_fort_items(items, &app->gs);
                        if(app->fort_cursor >= n) app->fort_cursor = n > 0 ? n-1 : 0;
                    }
                    if(ev.key == InputKeyBack) app->gs.screen = SCREEN_FORT_ARRIVAL;
                    break;
                }

                case SCREEN_MAP:
                    if(!is_press) break;
                    if(app->map_page == 2) {
                        if(ev.key == InputKeyLeft)
                            app->gs.settings_cursor = 0;
                        if(ev.key == InputKeyRight)
                            app->gs.settings_cursor = 1;
                        if(ev.key == InputKeyOk) {
                            if(app->gs.settings_cursor == 0)
                                app->gs.trail.pace = (app->gs.trail.pace % 3) + 1;
                            else
                                app->gs.trail.rations = (app->gs.trail.rations % 3) + 1;
                        }
                        if(ev.key == InputKeyUp)   app->map_page = 1;
                        if(ev.key == InputKeyBack)  app->gs.screen = SCREEN_TRAIL;
                    } else {
                        if(ev.key == InputKeyUp   && app->map_page > 0) app->map_page--;
                        if(ev.key == InputKeyDown  && app->map_page < 2) app->map_page++;
                        if(ev.key == InputKeyBack)  app->gs.screen = SCREEN_TRAIL;
                    }
                    break;

                case SCREEN_REST:
                    if(!is_press) break;
                    app->gs.screen = SCREEN_TRAIL;
                    break;

                case SCREEN_RIVER:
                    if(!is_press) break;
                    if(ev.key == InputKeyUp   && app->river.cursor > 0) app->river.cursor--;
                    if(ev.key == InputKeyDown  && app->river.cursor < 2) app->river.cursor++;
                    if(ev.key == InputKeyOk) {
                        river_commit(&app->river, &app->gs);
                        if(app->river.cursor == 2) {
                            // Wait — stay on river screen
                        } else {
                            if(app->river.outcome == FORD_SAFE)
                                sound_play(SND_RIVER_SAFE);
                            else
                                sound_play(SND_RIVER_SPLASH);
                            app->gs.screen = SCREEN_RIVER_OUTCOME;
                        }
                    }
                    if(ev.key == InputKeyBack) app->gs.screen = SCREEN_TRAIL;
                    break;

                case SCREEN_RIVER_OUTCOME:
                    if(!is_press) break;
                    if(ev.key == InputKeyOk || ev.key == InputKeyBack)
                        app->gs.screen = SCREEN_TRAIL;
                    break;

                case SCREEN_EVENT:
                    if(!is_press) break;
                    if(ev.key == InputKeyUp) {
                        if(app->active_event.choice_cursor > 0)
                            app->active_event.choice_cursor--;
                    } else if(ev.key == InputKeyDown) {
                        if(app->active_event.choice_cursor <
                           app->active_event.def->num_choices - 1)
                            app->active_event.choice_cursor++;
                    } else if(ev.key == InputKeyOk) {
                        event_apply_choice(&app->gs, &app->active_event,
                                           app->active_event.choice_cursor);
                        app->gs.screen = SCREEN_TRAIL;
                    } else if(ev.key == InputKeyBack) {
                        app->gs.screen = SCREEN_TRAIL;
                    }
                    break;

                case SCREEN_HUNT:
                    if(!is_press) break;
                    if(ev.key == InputKeyOk) {
                        if(app->hunt.gored) {
                            hunt_init(&app->hunt);
                            app->gs.screen = SCREEN_TRAIL;
                        } else {
                            sound_play(SND_HUNT_FIRE);
                            hunt_fire(&app->hunt, &app->gs);
                        }
                    }
                    if(ev.key == InputKeyBack) {
                        hunt_back_penalty(&app->hunt, &app->gs);
                        if(!app->hunt.gored)
                            app->gs.screen = SCREEN_TRAIL;
                        // if gored=true, stay on hunt to show the card
                    }
                    break;

                case SCREEN_DEATH:
                    if(!is_press) break;
                    if(ev.key == InputKeyOk) {
                        for(int d = 0; d < 3; d++) {
                            ActiveEvent dummy; dummy.active = false;
                            day_advance(&app->gs, &dummy);
                        }
                        app->gs.screen = SCREEN_TRAIL;
                    }
                    break;

                case SCREEN_WIN:
                    if(is_press) {
                        game_reset(app);
                        app->gs.screen = SCREEN_NAME_EDIT;
                    }
                    break;

                case SCREEN_GAME_OVER:
                    if(is_press) {
                        game_reset(app);
                        app->gs.screen = SCREEN_NAME_EDIT;
                    }
                    break;
            }
        }

        // ── Update ───────────────────────────────────────────
        if(app->gs.screen == SCREEN_HUNT) {
            hunt_update(&app->hunt, dt, &app->gs);
            if(app->hunt.pending_sound >= 0) {
                sound_play((SoundEvent)app->hunt.pending_sound);
                app->hunt.pending_sound = -1;
            }
        }

        // Check for player death after any state change
        if(app->gs.screen == SCREEN_TRAIL || app->gs.screen == SCREEN_HUNT) {
            bool all_dead = true;
            int  newly_dead = -1;
            for(int i = 0; i < app->gs.num_players; i++) {
                if(app->gs.players[i].hp <= 0.0f) {
                    if(app->dead_player_idx != i) newly_dead = i;
                } else {
                    all_dead = false;
                }
            }
            if(all_dead) {
                sound_play(SND_GAME_OVER);
                app->gs.screen = SCREEN_GAME_OVER;
            } else if(newly_dead >= 0) {
                app->dead_player_idx = newly_dead;
                sound_play(SND_PLAYER_DEATH);
                app->gs.screen = SCREEN_DEATH;
                app->ok_held = false;
            }
        }

        // Held-OK fast advance on trail — 1 day per second, frame-rate based
        if(app->ok_held && app->gs.screen == SCREEN_TRAIL) {
            app->hold_advance_ms += dt;
            while(app->hold_advance_ms >= 800) {
                app->hold_advance_ms -= 800;
                DayResult result = day_advance(&app->gs, &app->active_event);
                if(result == DAY_EVENT) {
                    bool good = (strncmp(app->active_event.def->header, "GOOD", 4) == 0);
                    sound_play(good ? SND_EVENT_GOOD : SND_EVENT_BAD);
                    app->gs.screen = SCREEN_EVENT;
                    app->ok_held = false;
                    break;
                } else if(result == DAY_ARRIVED) {
                    sound_play(SND_WIN);
                    app->gs.screen = SCREEN_WIN;
                    app->ok_held = false;
                    break;
                } else if(result == DAY_ALL_DEAD) {
                    sound_play(SND_GAME_OVER);
                    app->gs.screen = SCREEN_GAME_OVER;
                    app->ok_held = false;
                    break;
                } else {
                    sound_play(SND_TRAIL_ADVANCE);
                }
                // Fort check during held advance
                {
                    static const int FORT_IDX[] = {1, 3, 5, 7};
                    int li = landmark_idx(app->gs.trail.miles);
                    bool intercepted = false;
                    for(int f = 0; f < 4; f++) {
                        if(li == FORT_IDX[f] &&
                           app->gs.trail.last_fort_visited != li) {
                            app->gs.trail.last_fort_visited = li;
                            app->fort_cursor = 0;
                            app->gs.screen = SCREEN_FORT_ARRIVAL;
                            app->ok_held = false;
                            furi_message_queue_reset(app->input_queue);
                            sound_play(SND_FORT_ARRIVE);
                            intercepted = true;
                            goto done_hold;
                        }
                    }
                    // River check during held advance
                    if(!intercepted) {
                        for(int ri = 0; ri < NUM_RIVERS; ri++) {
                            if(app->gs.trail.miles >= RIVERS[ri].miles &&
                               !(app->gs.trail.rivers_visited & (1 << ri))) {
                                app->gs.trail.rivers_visited |= (1 << ri);
                                river_init(&app->river, ri, &app->gs.trail);
                                app->gs.screen = SCREEN_RIVER;
                                app->ok_held = false;
                                furi_message_queue_reset(app->input_queue);
                                goto done_hold;
                            }
                        }
                    }
                }
            }
        }
        done_hold:;

        // ── Render ───────────────────────────────────────────
        view_port_update(app->view_port);

        // Target ~33ms per frame (30fps). Sleep remainder.
        uint32_t frame_ms = furi_get_tick() - now;
        if(frame_ms < 33) furi_delay_ms(33 - frame_ms);
    }

    // ── Cleanup ───────────────────────────────────────────────
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
