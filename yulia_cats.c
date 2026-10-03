// Yulia's Cats - a cozy virtual pet for Flipper Zero.
//
// Yulia shares a little room with her two cats, Nugget and Baby. Feed them,
// play with them, pet them, make tea and tuck everyone in for a nap. Time
// keeps passing while the app is closed, but nobody ever comes to harm.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include "sprites.h"

#define SAVE_DIR   EXT_PATH("apps_data/yulia_cats")
#define SAVE_PATH  SAVE_DIR "/save.bin"
#define SAVE_MAGIC 0x31434C59 // "YLC1"

#define TICK_MS      100
#define STAT_MAX     1000
#define DECAY_PERIOD 15 // seconds per point of hunger / boredom
#define AWAY_CAP     (12 * 60 * 60)
#define AWAY_FLOOR   200 // time away never drops a stat below this

#define ROOM_X    43
#define ROOM_MINX 56
#define ROOM_MAXX 114
#define FLOOR_Y   52
#define BAR_Y     54
#define BOWL_X    80
#define PET_X     55

#define CAT_COUNT   2
#define HEART_COUNT 8

#define SLEEPY_BELOW 150
#define RESTED_ABOVE 800
#define HUNGRY_BELOW 250
#define FULL_ABOVE   850

typedef enum {
    CatSit,
    CatWalk,
    CatSleep,
    CatEat,
    CatChase,
    CatComePet,
} CatMode;

typedef struct {
    int16_t full;
    int16_t fun;
    int16_t energy;
} CatStats;

typedef struct {
    CatStats s;
    int16_t x; // centre of the cat on the floor
    int16_t target;
    int8_t dir;
    CatMode mode;
    uint16_t timer;
    uint8_t pet_timer;
    uint8_t bubble;
} Cat;

typedef struct {
    uint32_t magic;
    uint32_t first_ts;
    uint32_t last_ts;
    uint32_t love;
    int16_t cozy;
    CatStats cats[CAT_COUNT];
} SaveData;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t life;
} Heart;

typedef enum {
    ActionFeed,
    ActionPlay,
    ActionPetNugget,
    ActionPetBaby,
    ActionTea,
    ActionNap,
    ActionCount,
} Action;

typedef enum {
    FaceSmile,
    FaceHappy,
    FaceFlat,
    FaceSad,
    FaceSip,
    FaceAsleep,
} Face;

typedef struct {
    FuriMutex* mutex;
    Cat cats[CAT_COUNT];
    Heart hearts[HEART_COUNT];
    int16_t cozy;
    uint32_t love;
    uint32_t first_ts;
    uint32_t decay_ts;
    uint32_t tick;
    Action action;
    bool show_stats;
    bool lights_off;
    bool bowl_full;
    uint8_t yarn_timer;
    int16_t yarn_x;
    int8_t yarn_dx;
    uint8_t tea_timer;
    uint8_t happy_timer;
    uint8_t blink;
    uint8_t toast_timer;
    const char* toast;
    bool vibro;
} App;

static const char* const cat_names[CAT_COUNT] = {"Nugget", "Baby"};

static const Sprite* const cat_sit[CAT_COUNT] = {&spr_nugget_sit, &spr_baby_sit};
static const Sprite* const cat_sleep[CAT_COUNT] = {&spr_nugget_sleep, &spr_baby_sleep};
// [cat][facing left][frame]
static const Sprite* const cat_walk[CAT_COUNT][2][2] = {
    {{&spr_nugget_walk_a, &spr_nugget_walk_b}, {&spr_nugget_walk_a_l, &spr_nugget_walk_b_l}},
    {{&spr_baby_walk_a, &spr_baby_walk_b}, {&spr_baby_walk_a_l, &spr_baby_walk_b_l}},
};

static int16_t clamp_stat(int32_t v) {
    return v < 0 ? 0 : v > STAT_MAX ? STAT_MAX : v;
}

static int16_t rand_range(int16_t lo, int16_t hi) {
    return lo + (int16_t)(furi_hal_random_get() % (uint32_t)(hi - lo + 1));
}

// ---------------------------------------------------------------- save

static void game_new(App* app, uint32_t now) {
    app->first_ts = now;
    app->love = 0;
    app->cozy = 600;
    for(int i = 0; i < CAT_COUNT; i++) {
        app->cats[i].s = (CatStats){.full = 550, .fun = 600, .energy = 900};
    }
}

// Catch up on the time spent away. The cats nap while Yulia is out, so they
// come back rested, a bit hungry and keen to play - never worse than that.
static int16_t away_drop(int16_t value, uint32_t steps) {
    int32_t floor = value < AWAY_FLOOR ? value : AWAY_FLOOR;
    int32_t v = (int32_t)value - (int32_t)steps;
    return v < floor ? floor : v;
}

static void game_load(App* app) {
    uint32_t now = furi_hal_rtc_get_timestamp();
    SaveData save;
    bool ok = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = storage_file_read(file, &save, sizeof(save)) == sizeof(save) &&
             save.magic == SAVE_MAGIC;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        game_new(app, now);
        app->toast = "Hello, Yulia!";
        app->toast_timer = 30;
    } else {
        app->first_ts = save.first_ts;
        app->love = save.love;
        app->cozy = clamp_stat(save.cozy);
        uint32_t away = now > save.last_ts ? now - save.last_ts : 0;
        if(away > AWAY_CAP) away = AWAY_CAP;
        uint32_t steps = away / DECAY_PERIOD;
        app->cozy = away_drop(app->cozy, steps);
        for(int i = 0; i < CAT_COUNT; i++) {
            CatStats* s = &app->cats[i].s;
            *s = save.cats[i];
            s->full = away_drop(clamp_stat(s->full), steps);
            s->fun = away_drop(clamp_stat(s->fun), steps);
            s->energy = clamp_stat((int32_t)s->energy + (int32_t)(away / 4));
        }
        if(away > 60 * 60) {
            app->toast = "They missed you!";
            app->toast_timer = 30;
        }
    }
    app->decay_ts = now;
}

static void game_save(App* app) {
    SaveData save = {
        .magic = SAVE_MAGIC,
        .first_ts = app->first_ts,
        .last_ts = furi_hal_rtc_get_timestamp(),
        .love = app->love,
        .cozy = app->cozy,
    };
    for(int i = 0; i < CAT_COUNT; i++)
        save.cats[i] = app->cats[i].s;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &save, sizeof(save));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// ---------------------------------------------------------------- game

static void toast(App* app, const char* text) {
    app->toast = text;
    app->toast_timer = 22;
}

static void meow(void) {
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;
    if(!furi_hal_speaker_acquire(30)) return;
    furi_hal_speaker_start(880.0f, 0.4f);
    furi_delay_ms(45);
    furi_hal_speaker_start(1175.0f, 0.4f);
    furi_delay_ms(60);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

static void heart_spawn(App* app, int16_t x, int16_t y) {
    for(int i = 0; i < HEART_COUNT; i++) {
        if(app->hearts[i].life == 0) {
            app->hearts[i] = (Heart){.x = x + rand_range(-5, 5), .y = y, .life = 14};
            return;
        }
    }
}

static bool cat_asleep(const Cat* cat) {
    return cat->mode == CatSleep;
}

static void cat_rest(Cat* cat) {
    cat->mode = CatSit;
    cat->timer = rand_range(15, 60);
}

static bool busy(const App* app) {
    if(app->yarn_timer || app->tea_timer) return true;
    for(int i = 0; i < CAT_COUNT; i++) {
        const Cat* cat = &app->cats[i];
        if(cat->mode == CatEat || cat->mode == CatComePet || cat->pet_timer) return true;
    }
    return false;
}

static void lights_on(App* app) {
    if(!app->lights_off) return;
    app->lights_off = false;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat->s.energy >= SLEEPY_BELOW) cat_rest(cat);
    }
}

static void do_feed(App* app) {
    int hungry = 0;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat->s.full > FULL_ABOVE) continue;
        hungry++;
        cat->mode = CatEat;
        cat->target = i == 0 ? BOWL_X - 10 : BOWL_X + 25;
        cat->timer = 30;
    }
    if(hungry) {
        app->bowl_full = true;
        app->happy_timer = 20;
        toast(app, "Dinner time!");
    } else {
        toast(app, "Not hungry");
    }
}

static void do_play(App* app) {
    int players = 0;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat->s.energy < SLEEPY_BELOW) continue;
        players++;
        cat->mode = CatChase;
    }
    if(players) {
        app->yarn_timer = 80;
        app->yarn_x = 86;
        app->yarn_dx = furi_hal_random_get() & 1 ? 3 : -3;
        app->happy_timer = 80;
    } else {
        toast(app, "Too sleepy...");
    }
}

static void do_pet(App* app, int i) {
    Cat* cat = &app->cats[i];
    if(cat_asleep(cat)) {
        // A gentle pat where the cat is curled up.
        cat->pet_timer = 30;
    } else {
        cat->mode = CatComePet;
        cat->target = PET_X;
    }
    app->happy_timer = 50;
}

static void do_action(App* app) {
    if(busy(app)) return;
    if(app->action == ActionNap) {
        if(app->lights_off) {
            lights_on(app);
            toast(app, "Good morning!");
        } else {
            app->lights_off = true;
            for(int i = 0; i < CAT_COUNT; i++)
                app->cats[i].mode = CatSleep;
        }
        return;
    }
    lights_on(app);
    switch(app->action) {
    case ActionFeed:
        do_feed(app);
        break;
    case ActionPlay:
        do_play(app);
        break;
    case ActionPetNugget:
        do_pet(app, 0);
        break;
    case ActionPetBaby:
        do_pet(app, 1);
        break;
    case ActionTea:
        app->tea_timer = 40;
        app->cozy = clamp_stat(app->cozy + 300);
        toast(app, "So cozy...");
        break;
    default:
        break;
    }
}

static bool cat_step(Cat* cat, int16_t speed) {
    int16_t d = cat->target - cat->x;
    if(d == 0) return true;
    cat->dir = d > 0 ? 1 : -1;
    if(d > speed) d = speed;
    if(d < -speed) d = -speed;
    cat->x += d;
    return cat->x == cat->target;
}

static void cat_tick(App* app, int i) {
    Cat* cat = &app->cats[i];
    if(cat->bubble) cat->bubble--;

    if(cat->pet_timer) {
        cat->pet_timer--;
        if(cat->pet_timer % 6 == 0) heart_spawn(app, cat->x, FLOOR_Y - 16);
        if(cat->pet_timer == 0) {
            cat->s.fun = clamp_stat(cat->s.fun + 200);
            app->cozy = clamp_stat(app->cozy + 60);
            app->love++;
            toast(app, "Purr...");
        }
    }

    switch(cat->mode) {
    case CatSit:
        if(cat->pet_timer) break;
        if(cat->s.energy < SLEEPY_BELOW) {
            cat->mode = CatSleep;
        } else if(cat->timer) {
            cat->timer--;
            if(cat->s.full < HUNGRY_BELOW && !cat->bubble && furi_hal_random_get() % 250 == 0) {
                cat->bubble = 25;
                meow();
            }
        } else {
            cat->mode = CatWalk;
            cat->target = rand_range(ROOM_MINX, ROOM_MAXX);
        }
        break;
    case CatWalk:
        // Nugget is an older gentleman and takes his time.
        if(i == 0 && (app->tick & 1)) break;
        if(cat_step(cat, 1)) cat_rest(cat);
        break;
    case CatSleep:
        if(app->tick % 10 == 0) cat->s.energy = clamp_stat(cat->s.energy + 4);
        if(!app->lights_off && cat->s.energy > RESTED_ABOVE) cat_rest(cat);
        break;
    case CatEat:
        if(cat_step(cat, 2)) {
            cat->dir = i == 0 ? 1 : -1;
            if(cat->timer) {
                cat->timer--;
            } else {
                cat->s.full = clamp_stat(cat->s.full + 450);
                heart_spawn(app, cat->x, FLOOR_Y - 14);
                cat_rest(cat);
            }
        }
        break;
    case CatChase:
        cat->target = app->yarn_x + (i == 0 ? -8 : 8);
        if(cat->target < ROOM_MINX) cat->target = ROOM_MINX;
        if(cat->target > ROOM_MAXX) cat->target = ROOM_MAXX;
        cat_step(cat, i == 0 ? 1 : 2);
        if(app->yarn_timer == 0) {
            cat->s.fun = clamp_stat(cat->s.fun + 350);
            cat->s.energy = clamp_stat(cat->s.energy - 60);
            heart_spawn(app, cat->x, FLOOR_Y - 14);
            cat_rest(cat);
        }
        break;
    case CatComePet:
        if(cat_step(cat, 2)) {
            cat->dir = -1;
            cat->pet_timer = 35;
            cat->mode = CatSit;
            cat->timer = 50;
        }
        break;
    }
}

static void game_tick(App* app) {
    app->tick++;

    // Slow drift of the needs, tied to the real-time clock.
    uint32_t now = furi_hal_rtc_get_timestamp();
    if(now < app->decay_ts) app->decay_ts = now;
    while(now - app->decay_ts >= DECAY_PERIOD) {
        app->decay_ts += DECAY_PERIOD;
        app->cozy = clamp_stat(app->cozy - 1);
        for(int i = 0; i < CAT_COUNT; i++) {
            Cat* cat = &app->cats[i];
            cat->s.full = clamp_stat(cat->s.full - 1);
            cat->s.fun = clamp_stat(cat->s.fun - 1);
            // Nugget tires sooner; Baby is always ready for a snack.
            if(!cat_asleep(cat)) cat->s.energy = clamp_stat(cat->s.energy - (i == 0 ? 2 : 1));
            if(i == 1 && (app->decay_ts / DECAY_PERIOD) % 2)
                cat->s.full = clamp_stat(cat->s.full - 1);
        }
    }

    if(app->yarn_timer) {
        app->yarn_timer--;
        app->yarn_x += app->yarn_dx;
        if(app->yarn_x < ROOM_MINX - 4 || app->yarn_x > ROOM_MAXX + 6) {
            app->yarn_dx = -app->yarn_dx;
            app->yarn_x += app->yarn_dx * 2;
        }
        if(app->yarn_timer == 0) toast(app, "So much fun!");
    }

    bool eating = false;
    bool purring = false;
    for(int i = 0; i < CAT_COUNT; i++) {
        cat_tick(app, i);
        if(app->cats[i].mode == CatEat) eating = true;
        if(app->cats[i].pet_timer) purring = true;
    }
    if(!eating) app->bowl_full = false;

    // Purr: a soft buzz through the vibration motor while a cat is petted.
    bool vibro = purring && (app->tick & 1);
    if(vibro != app->vibro) {
        app->vibro = vibro;
        furi_hal_vibro_on(vibro);
    }

    for(int i = 0; i < HEART_COUNT; i++) {
        Heart* heart = &app->hearts[i];
        if(heart->life) {
            heart->life--;
            heart->y--;
        }
    }

    if(app->tea_timer) app->tea_timer--;
    if(app->happy_timer) app->happy_timer--;
    if(app->toast_timer) app->toast_timer--;
    if(app->blink) {
        app->blink--;
    } else if(furi_hal_random_get() % 45 == 0) {
        app->blink = 2;
    }
}

static int16_t mood(const App* app) {
    int32_t sum = 0;
    for(int i = 0; i < CAT_COUNT; i++) {
        const CatStats* s = &app->cats[i].s;
        sum += s->full + s->fun + s->energy;
    }
    int32_t cats = sum / (CAT_COUNT * 3);
    return (cats * 2 + app->cozy) / 3;
}

// ---------------------------------------------------------------- draw

static void draw_sprite(Canvas* canvas, int32_t x, int32_t y, const Sprite* sprite) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_xbm(canvas, x, y, sprite->w, sprite->h, sprite->white);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, x, y, sprite->w, sprite->h, sprite->black);
}

static Face yulia_face(const App* app) {
    if(app->lights_off) return FaceAsleep;
    if(app->tea_timer) return FaceSip;
    if(app->happy_timer) return FaceHappy;
    int16_t m = mood(app);
    if(m > 550) return FaceSmile;
    if(m > 300) return FaceFlat;
    return FaceSad;
}

static void draw_yulia(Canvas* canvas, const App* app) {
    draw_sprite(canvas, 0, BAR_Y - spr_yulia.h, &spr_yulia);
    Face face = yulia_face(app);
    canvas_set_color(canvas, ColorBlack);

    // Eyes sit behind the two lenses.
    for(int i = 0; i < 2; i++) {
        int32_t x = i ? 26 : 14;
        if(face == FaceHappy || face == FaceSip) {
            canvas_draw_line(canvas, x - 2, 29, x, 27);
            canvas_draw_line(canvas, x + 1, 27, x + 3, 29);
        } else if(face == FaceAsleep || app->blink) {
            canvas_draw_line(canvas, x - 1, 29, x + 2, 29);
        } else {
            canvas_draw_box(canvas, x, 26, 2, 4);
        }
    }

    switch(face) {
    case FaceHappy:
        canvas_draw_line(canvas, 18, 37, 23, 37);
        canvas_draw_line(canvas, 19, 38, 22, 38);
        canvas_draw_line(canvas, 20, 39, 21, 39);
        break;
    case FaceFlat:
        canvas_draw_line(canvas, 19, 38, 22, 38);
        break;
    case FaceSad:
        canvas_draw_line(canvas, 19, 37, 22, 37);
        canvas_draw_dot(canvas, 18, 38);
        canvas_draw_dot(canvas, 23, 38);
        break;
    case FaceSip:
        break; // hidden behind the mug
    default:
        canvas_draw_dot(canvas, 18, 37);
        canvas_draw_dot(canvas, 23, 37);
        canvas_draw_line(canvas, 19, 38, 22, 38);
        break;
    }

    if(app->tea_timer) {
        draw_sprite(canvas, 17, 35, &spr_mug);
        // Steam curls up from the mug.
        for(int i = 0; i < 2; i++) {
            int32_t x = 19 + i * 3;
            int32_t phase = (app->tick / 3 + i) & 3;
            for(int k = 0; k < 3; k++) {
                int32_t wobble = ((k + phase) & 3) == 0 ? 1 : 0;
                canvas_draw_dot(canvas, x + wobble, 33 - k);
            }
        }
    }
}

static void draw_window(Canvas* canvas) {
    const int32_t x = 100, y = 5, w = 23, h = 21;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);
    canvas_draw_line(canvas, x - 2, y + h, x + w + 1, y + h); // sill

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    bool day = now.hour >= 7 && now.hour < 19;
    if(day) {
        canvas_draw_disc(canvas, x + 15, y + 7, 3);
        canvas_draw_dot(canvas, x + 15, y + 2);
        canvas_draw_dot(canvas, x + 10, y + 7);
        canvas_draw_dot(canvas, x + 20, y + 7);
        canvas_draw_dot(canvas, x + 15, y + 12);
        canvas_draw_dot(canvas, x + 11, y + 3);
        canvas_draw_dot(canvas, x + 19, y + 3);
        canvas_draw_dot(canvas, x + 11, y + 11);
        canvas_draw_dot(canvas, x + 19, y + 11);
    } else {
        canvas_draw_disc(canvas, x + 15, y + 7, 4);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, x + 17, y + 6, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_dot(canvas, x + 4, y + 4);
        canvas_draw_dot(canvas, x + 8, y + 11);
        canvas_draw_dot(canvas, x + 6, y + 16);
        canvas_draw_dot(canvas, x + 18, y + 16);
    }
    canvas_draw_line(canvas, x + w / 2, y, x + w / 2, y + h - 1);
    canvas_draw_line(canvas, x, y + h / 2, x + w - 1, y + h / 2);
}

static void draw_room(Canvas* canvas) {
    draw_window(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Shelf with a potted plant and a couple of books.
    canvas_draw_line(canvas, 52, 22, 78, 22);
    canvas_draw_frame(canvas, 55, 17, 6, 5);
    canvas_draw_line(canvas, 58, 16, 58, 12);
    canvas_draw_line(canvas, 58, 14, 55, 11);
    canvas_draw_line(canvas, 58, 13, 61, 10);
    canvas_draw_dot(canvas, 58, 11);
    canvas_draw_frame(canvas, 66, 14, 3, 8);
    canvas_draw_frame(canvas, 69, 12, 3, 10);
    canvas_draw_line(canvas, 73, 21, 76, 13);

    // Skirting board and a rug.
    canvas_draw_line(canvas, ROOM_X, 43, 127, 43);
    for(int32_t x = 60; x < 118; x += 2)
        canvas_draw_dot(canvas, x, FLOOR_Y + 1);
}

static void draw_cat(Canvas* canvas, const App* app, int i) {
    const Cat* cat = &app->cats[i];
    const Sprite* sprite;
    int32_t bob = 0;

    switch(cat->mode) {
    case CatSleep:
        sprite = cat_sleep[i];
        break;
    case CatSit:
        sprite = cat_sit[i];
        break;
    case CatEat:
        if(cat->x == cat->target) {
            sprite = cat_walk[i][cat->dir < 0][0];
            bob = (app->tick / 3) & 1;
            break;
        }
        // fall through
    default:
        sprite = cat_walk[i][cat->dir < 0][(app->tick / 2) & 1];
        break;
    }

    int32_t x = cat->x - sprite->w / 2;
    int32_t y = FLOOR_Y - sprite->h + 1 + bob;
    draw_sprite(canvas, x, y, sprite);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(cat->mode == CatSleep && !cat->pet_timer) {
        int32_t phase = (app->tick / 6) % 3;
        canvas_draw_str(canvas, cat->x + 6 + phase, y - 1 - phase * 2, "z");
    }
    if(cat->bubble) {
        int32_t bx = cat->x - 14;
        if(bx < ROOM_X + 1) bx = ROOM_X + 1;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, bx, y - 12, 29, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, y - 12, 29, 11, 2);
        canvas_draw_str(canvas, bx + 3, y - 3, "meow!");
    }
}

static void draw_scene(Canvas* canvas, const App* app) {
    draw_room(canvas);

    draw_sprite(
        canvas,
        BOWL_X,
        FLOOR_Y - (app->bowl_full ? 6 : 4),
        app->bowl_full ? &spr_bowl_full : &spr_bowl);

    for(int i = 0; i < CAT_COUNT; i++)
        draw_cat(canvas, app, i);

    if(app->yarn_timer) {
        // The ball hops along the floor.
        static const int8_t hop[] = {0, 3, 5, 6, 5, 3};
        int32_t y = FLOOR_Y - 6 - hop[app->tick % 6];
        draw_sprite(canvas, app->yarn_x - 3, y, &spr_yarn);
    }

    for(int i = 0; i < HEART_COUNT; i++) {
        const Heart* heart = &app->hearts[i];
        if(heart->life) draw_sprite(canvas, heart->x - 3, heart->y, &spr_heart);
    }

    if(app->toast_timer && app->toast) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 46, 8, app->toast);
    }

    if(app->lights_off) {
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_box(canvas, ROOM_X, 0, 128 - ROOM_X, BAR_Y);
    }

    draw_yulia(canvas, app);
}

static const char* action_label(const App* app) {
    switch(app->action) {
    case ActionFeed:
        return "Feed";
    case ActionPlay:
        return "Play";
    case ActionPetNugget:
        return "Pet Nugget";
    case ActionPetBaby:
        return "Pet Baby";
    case ActionTea:
        return "Tea time";
    default:
        return app->lights_off ? "Wake up" : "Nap time";
    }
}

static void draw_bar(Canvas* canvas, const App* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, BAR_Y, 127, BAR_Y);
    canvas_set_font(canvas, FontSecondary);

    char buf[12];
    draw_sprite(canvas, 2, 57, &spr_heart);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->love);
    canvas_draw_str(canvas, 12, 63, buf);

    canvas_draw_str(canvas, 46, 63, "<");
    canvas_draw_str(canvas, 122, 63, ">");
    canvas_draw_str_aligned(canvas, 86, 63, AlignCenter, AlignBottom, action_label(app));
}

static void draw_meter(Canvas* canvas, int32_t x, int32_t y, int32_t w, int16_t value) {
    canvas_draw_rframe(canvas, x, y, w, 7, 1);
    int32_t fill = (int32_t)value * (w - 4) / STAT_MAX;
    if(fill > 0) canvas_draw_box(canvas, x + 2, y + 2, fill, 3);
}

static void draw_stats(Canvas* canvas, const App* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 40, 8, "Food");
    canvas_draw_str(canvas, 72, 8, "Fun");
    canvas_draw_str(canvas, 100, 8, "Rest");

    for(int i = 0; i < CAT_COUNT; i++) {
        const CatStats* s = &app->cats[i].s;
        int32_t y = 12 + i * 12;
        canvas_draw_str(canvas, 2, y + 7, cat_names[i]);
        draw_meter(canvas, 38, y, 26, s->full);
        draw_meter(canvas, 68, y, 26, s->fun);
        draw_meter(canvas, 98, y, 26, s->energy);
    }

    canvas_draw_str(canvas, 2, 43, "Yulia");
    canvas_draw_str(canvas, 38, 43, "cozy");
    draw_meter(canvas, 68, 36, 56, app->cozy);

    char buf[32];
    uint32_t now = furi_hal_rtc_get_timestamp();
    uint32_t days = now > app->first_ts ? (now - app->first_ts) / 86400 : 0;
    snprintf(buf, sizeof(buf), "Day %lu together", (unsigned long)(days + 1));
    canvas_draw_line(canvas, 0, 49, 127, 49);
    canvas_draw_str(canvas, 2, 60, buf);
    draw_sprite(canvas, 96, 54, &spr_heart);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->love);
    canvas_draw_str(canvas, 106, 60, buf);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    if(app->show_stats) {
        draw_stats(canvas, app);
    } else {
        draw_scene(canvas, app);
        draw_bar(canvas, app);
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

// Returns false when the app should quit.
static bool handle_input(App* app, const InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    if(app->show_stats) {
        app->show_stats = false;
        return true;
    }

    switch(event->key) {
    case InputKeyLeft:
        app->action = (app->action + ActionCount - 1) % ActionCount;
        break;
    case InputKeyRight:
        app->action = (app->action + 1) % ActionCount;
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) do_action(app);
        break;
    case InputKeyUp:
    case InputKeyDown:
        app->show_stats = true;
        break;
    case InputKeyBack:
        return false;
    default:
        break;
    }
    return true;
}

int32_t yulia_cats_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    game_load(app);
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        cat->x = i == 0 ? 68 : 104;
        cat->dir = i == 0 ? 1 : -1;
        cat_rest(cat);
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    uint32_t next_tick = furi_get_tick() + furi_ms_to_ticks(TICK_MS);
    while(running) {
        uint32_t now = furi_get_tick();
        uint32_t wait = next_tick > now ? next_tick - now : 0;
        InputEvent event;
        if(furi_message_queue_get(queue, &event, wait) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            running = handle_input(app, &event);
            furi_mutex_release(app->mutex);
        } else {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            game_tick(app);
            furi_mutex_release(app->mutex);
            next_tick += furi_ms_to_ticks(TICK_MS);
        }
        view_port_update(view_port);
    }

    furi_hal_vibro_on(false);
    game_save(app);

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
