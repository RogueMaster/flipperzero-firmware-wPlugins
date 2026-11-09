#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Real seconds per game minute; lower it to speed the pet up.
#define SECONDS_PER_TICK     60
#define HATCH_TICKS          1
#define CHILD_TICKS          10
#define ADULT_TICKS          60
#define HUNGER_EVERY         5
#define STRENGTH_EVERY       7
#define MOOD_EVERY           6
#define POOP_EVERY           10
#define STARVE_MISTAKE_TICKS 10
#define MAX_OFFLINE_TICKS    (24 * 60)
#define SAVE_PATH            APP_DATA_PATH("digipet.sav")
#define SAVE_VERSION         2

// Scene layout: the horizon is far away, the ground line is close to the viewer.
#define HORIZON_Y   22
#define GROUND_Y    51
#define PLAY_FRAMES 150

typedef enum {
    StageEgg,
    StageBaby,
    StageChild,
    StageAdult
} Stage;
typedef enum {
    ModeSelect,
    ModeMain,
    ModeStatus,
    ModeTrain,
    ModeBattle,
    ModeMessage
} Mode;
typedef enum {
    ActFeed,
    ActPlay,
    ActTrain,
    ActBattle,
    ActClean,
    ActMedic,
    ActStatus,
    ActCount
} Action;
typedef enum {
    SpCroc,
    SpTurtle,
    SpDuck,
    SpFrog,
    SpBunny,
    SpCount
} Species;

typedef struct {
    uint32_t version;
    uint32_t last_seen;
    uint32_t age;
    uint8_t stage;
    uint8_t species;
    uint8_t hunger;
    uint8_t strength;
    uint8_t mood;
    uint8_t poop;
    uint8_t sick;
    uint8_t mistakes;
    uint8_t weight;
    uint8_t good_adult;
    uint16_t wins;
    uint16_t battles;
    uint16_t starving;
} Pet;

typedef struct {
    FuriMutex* mutex;
    Pet pet;
    bool has_pet;
    Mode mode;
    Action action;
    uint8_t select;
    uint8_t enemy;
    uint32_t last_tick_ms;
    uint32_t anim;
    int train_pos;
    int train_dir;
    bool battle_done;
    bool battle_won;
    char message[40];
    // World positions are x10: x -500..500 (left..right), z 0..1000 (near..far).
    int px, pz, tx, tz;
    int idle;
    bool moving;
    bool face_left;
    int play;
    int bx, bz, btx, btz;
} App;

static const char* const spr_egg[] = {
    "......####......",
    "....##....##....",
    "...#........#...",
    "..#...##.....#..",
    "..#..####....#..",
    ".#....##......#.",
    ".#............#.",
    ".#...##....#..#.",
    ".#..####..###.#.",
    ".#...##....#..#.",
    ".#............#.",
    "..#..........#..",
    "..#..........#..",
    "...#........#...",
    "....##....##....",
    "......####......"};
static const char* const spr_croc[] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".........#.##...",
    "....#.#.#######.",
    "..##############",
    "################",
    "..#########.#.#.",
    "...##..##.......",
    "...#...#........",
    "................"};
static const char* const spr_turtle[] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....######.....",
    "....#..#...#....",
    "...#.###.##.#.##",
    "..#.#...#...####",
    "..#############.",
    "...##.....##....",
    "...##.....##....",
    "................",
    "................"};
static const char* const spr_duck[] = {
    "................",
    "..........###...",
    ".........#####..",
    ".........##.####",
    ".........#####..",
    "..........###...",
    "..........##....",
    "#.........##....",
    "##.......###....",
    "###########.....",
    "############....",
    ".###.....###....",
    "..#########.....",
    ".....#..#.......",
    "....##.##.......",
    "................"};
static const char* const spr_frog[] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "...###....###...",
    "..#.###..#.###..",
    "..#####..#####..",
    ".##############.",
    ".#............#.",
    ".#..########..#.",
    "..#..........#..",
    "...##########...",
    "..##.##..##.##..",
    ".##..........##.",
    "................"};
static const char* const spr_bunny[] = {
    "....##..##......",
    "....#.#.#.#.....",
    "....#.#.#.#.....",
    "....#.#.#.#.....",
    "....##..##......",
    "...########.....",
    "..#........#....",
    "..#.##..##.#....",
    "..#........#....",
    "..#...##...#....",
    "...#..##..#.....",
    "....######......",
    "...#......#.....",
    "..#........#....",
    "..##########....",
    "................"};
static const char* const spr_poop[] =
    {"...#....", "..##....", ".####...", "######..", "########", "........"};

typedef struct {
    const char* name;
    const char* const* rows;
} SpeciesInfo;

static const SpeciesInfo species_info[SpCount] = {
    {"Croc", spr_croc},
    {"Turtle", spr_turtle},
    {"Duck", spr_duck},
    {"Frog", spr_frog},
    {"Bunny", spr_bunny},
};

static const char* const action_names[ActCount] =
    {"Feed", "Play", "Train", "Battle", "Clean", "Medic", "Status"};
static const char* const stage_names[] = {"Egg", "Baby", "Child", "Adult"};

static void pet_title(const Pet* p, char* buf, size_t len) {
    const char* sp = species_info[p->species % SpCount].name;
    switch(p->stage) {
    case StageEgg:
        snprintf(buf, len, "%s Egg", sp);
        break;
    case StageBaby:
        snprintf(buf, len, "Baby %s", sp);
        break;
    case StageChild:
        snprintf(buf, len, "Young %s", sp);
        break;
    default:
        snprintf(buf, len, "%s %s", p->good_adult ? "Mighty" : "Grumpy", sp);
        break;
    }
}

// Draws a text bitmap scaled to `w` pixels wide, centred on cx with its bottom at by.
static void draw_bitmap(
    Canvas* c,
    const char* const* rows,
    int rw,
    int rh,
    int cx,
    int by,
    int w,
    bool flip) {
    if(w < 3) w = 3;
    int h = w * rh / rw;
    if(h < 2) h = 2;
    int x0 = cx - w / 2, y0 = by - h;
    for(int ty = 0; ty < h; ty++) {
        const char* row = rows[ty * rh / h];
        for(int tx = 0; tx < w; tx++) {
            int sc = tx * rw / w;
            if(row[flip ? rw - 1 - sc : sc] == '#') canvas_draw_dot(c, x0 + tx, y0 + ty);
        }
    }
}

// Perspective helpers: z is 0 (near) .. 100 (far).
static int depth_y(int z) {
    return GROUND_Y - z * (GROUND_Y - HORIZON_Y - 2) / 100;
}

static int depth_size(int z, int full) {
    return full - full * 6 * z / 1000;
}

static int depth_x(int wx, int z) {
    return 64 + wx * (100 - z * 6 / 10) / 100;
}

static int stage_size(uint8_t stage) {
    static const int sizes[] = {22, 18, 24, 30};
    return sizes[stage < 4 ? stage : 3];
}

static void draw_tree(Canvas* c, int wx, int z) {
    int s = depth_size(z, 26);
    int x = depth_x(wx, z), y = depth_y(z);
    canvas_draw_line(c, x, y, x, y - s / 2);
    canvas_draw_circle(c, x, y - s / 2 - s / 4, s / 4);
}

static void draw_scene(Canvas* c, uint32_t anim) {
    canvas_draw_circle(c, 112, 7, 4);
    int cloud = 140 - (int)((anim / 3) % 190);
    canvas_draw_rframe(c, cloud, 3, 16, 6, 3);
    canvas_draw_rframe(c, cloud + 34, 10, 11, 5, 2);

    static const uint8_t hills[][2] = {
        {0, 18},
        {14, 13},
        {26, 17},
        {40, 11},
        {56, 16},
        {70, 12},
        {86, 17},
        {100, 14},
        {116, 18},
        {127, 15}};
    for(size_t i = 0; i + 1 < COUNT_OF(hills); i++) {
        canvas_draw_line(c, hills[i][0], hills[i][1], hills[i + 1][0], hills[i + 1][1]);
    }
    canvas_draw_line(c, 0, HORIZON_Y, 127, HORIZON_Y);

    // Rays converging on the vanishing point give the ground depth.
    for(int bx = -96; bx <= 224; bx += 40) {
        for(int y = HORIZON_Y + 2; y <= GROUND_Y; y += 3) {
            canvas_draw_dot(c, 64 + (bx - 64) * (y - HORIZON_Y) / (GROUND_Y - HORIZON_Y), y);
        }
    }
    static const uint8_t stripes[] = {80, 55, 30, 8};
    for(size_t i = 0; i < COUNT_OF(stripes); i++) {
        int y = depth_y(stripes[i]);
        int step = 2 + (100 - stripes[i]) / 15;
        for(int x = (int)(i % 2) * 2; x < 128; x += step)
            canvas_draw_dot(c, x, y);
    }
    draw_tree(c, -50, 92);
    draw_tree(c, 46, 78);
}

static void draw_pet(Canvas* c, App* app) {
    const Pet* p = &app->pet;
    int z = app->pz / 10, wx = app->px / 10;
    int s = depth_size(z, stage_size(p->stage));
    int x = depth_x(wx, z), y = depth_y(z);
    canvas_draw_line(c, x - s / 3, y, x + s / 3, y);
    if(p->stage == StageEgg) {
        draw_bitmap(c, spr_egg, 16, 16, x + ((app->anim / 5) % 2 ? 1 : 0), y - 1, s, false);
        return;
    }
    int hop = (app->moving && (app->anim / 2) % 2) ? 1 + s / 12 : 0;
    draw_bitmap(c, species_info[p->species].rows, 16, 16, x, y - 1 - hop, s, app->face_left);
    bool needs = p->hunger == 0 || p->sick || p->poop >= 3 || p->mood == 0;
    if(needs && (app->anim / 5) % 2) canvas_draw_str(c, x + s / 2 - 1, y - s - 2, "!");
}

static void draw_ball(Canvas* c, App* app) {
    int z = app->bz / 10;
    int t = app->anim % 8;
    int bounce = t * (8 - t) * depth_size(z, 10) / 32;
    int x = depth_x(app->bx / 10, z), y = depth_y(z);
    int r = depth_size(z, 3);
    if(r < 1) r = 1;
    canvas_draw_line(c, x - r, y, x + r, y);
    canvas_draw_disc(c, x, y - r - 1 - bounce, r);
}

static void draw_main(Canvas* c, App* app) {
    const Pet* p = &app->pet;
    canvas_set_font(c, FontSecondary);
    draw_scene(c, app->anim);

    // Draw far things first so nearer ones overlap them.
    const int tree_z = 35;
    int pet_z = app->pz / 10;
    int ball_z = app->play ? app->bz / 10 : -1;
    bool tree_drawn = false, pet_drawn = false, ball_drawn = !app->play;
    for(int n = 0; n < 3; n++) {
        int best = -1, which = -1;
        if(!tree_drawn && tree_z > best) best = tree_z, which = 0;
        if(!pet_drawn && pet_z > best) best = pet_z, which = 1;
        if(!ball_drawn && ball_z > best) best = ball_z, which = 2;
        if(which == 0) {
            draw_tree(c, -60, tree_z);
            tree_drawn = true;
        } else if(which == 1) {
            draw_pet(c, app);
            pet_drawn = true;
        } else if(which == 2) {
            draw_ball(c, app);
            ball_drawn = true;
        }
    }

    for(int i = 0; i < p->poop; i++) {
        int z = 4 + (i / 2) * 10;
        draw_bitmap(
            c, spr_poop, 8, 6, depth_x(40 + (i % 2) * 10, z), depth_y(z), depth_size(z, 9), false);
    }

    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 0, 53, 128, 11);
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 52, 127, 52);
    canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, action_names[app->action]);
    canvas_draw_str(c, 2, 62, "<");
    canvas_draw_str(c, 122, 62, ">");
}

static void draw_select(Canvas* c, App* app) {
    draw_scene(c, app->anim);
    int hop = (app->anim / 3) % 2 ? 2 : 0;
    canvas_draw_line(c, 50, 47, 78, 47);
    draw_bitmap(c, species_info[app->select].rows, 16, 16, 64, 46 - hop, 30, (app->anim / 20) % 2);

    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 30, 0, 68, 12);
    canvas_draw_box(c, 0, 53, 128, 11);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    char title[24];
    snprintf(title, sizeof(title), "< %s >", species_info[app->select].name);
    canvas_draw_str_aligned(c, 64, 6, AlignCenter, AlignCenter, title);
    canvas_set_font(c, FontSecondary);
    canvas_draw_line(c, 0, 52, 127, 52);
    canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, "Choose pet - OK: hatch");
}

static void draw_meter(Canvas* c, int x, int y, const char* label, int value) {
    canvas_draw_str(c, x, y, label);
    for(int i = 0; i < 4; i++) {
        if(i < value)
            canvas_draw_box(c, x + 50 + i * 10, y - 7, 8, 8);
        else
            canvas_draw_frame(c, x + 50 + i * 10, y - 7, 8, 8);
    }
}

static void draw_status(Canvas* c, App* app) {
    const Pet* p = &app->pet;
    char line[48];
    canvas_set_font(c, FontPrimary);
    pet_title(p, line, sizeof(line));
    canvas_draw_str(c, 2, 9, line);
    canvas_set_font(c, FontSecondary);
    snprintf(line, sizeof(line), "%s %lum", stage_names[p->stage], (unsigned long)p->age);
    canvas_draw_str_aligned(c, 126, 9, AlignRight, AlignBottom, line);
    draw_meter(c, 2, 20, "Hunger", p->hunger);
    draw_meter(c, 2, 30, "Strength", p->strength);
    draw_meter(c, 2, 40, "Mood", p->mood);
    snprintf(
        line,
        sizeof(line),
        "Wt %ug Win %u/%u Err %u%s",
        p->weight,
        p->wins,
        p->battles,
        p->mistakes,
        p->sick ? " Sick" : "");
    canvas_draw_str(c, 2, 51, line);
    canvas_draw_str(c, 2, 62, "Hold OK: new pet");
}

static void draw_train(Canvas* c, App* app) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 10, AlignCenter, AlignCenter, "Training!");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignCenter, "Press OK in the zone");
    canvas_draw_frame(c, 14, 30, 100, 12);
    canvas_draw_box(c, 54, 30, 20, 3);
    canvas_draw_box(c, 54, 39, 20, 3);
    canvas_draw_box(c, 14 + app->train_pos, 31, 4, 10);
    canvas_draw_str(c, 2, 61, "Back: quit");
}

static void draw_battle(Canvas* c, App* app) {
    bool frame = (app->anim / 3) % 2;
    canvas_draw_line(c, 0, HORIZON_Y, 127, HORIZON_Y);
    canvas_draw_line(c, 84, 31, 112, 31);
    draw_bitmap(c, species_info[app->enemy].rows, 16, 16, 98, 30 - (frame ? 0 : 1), 18, true);
    canvas_draw_line(c, 10, 47, 46, 47);
    draw_bitmap(
        c, species_info[app->pet.species].rows, 16, 16, 28, 46 - (frame ? 1 : 0), 30, false);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 66, 30, AlignCenter, AlignCenter, "VS");
    canvas_set_font(c, FontSecondary);
    if(!app->battle_done) {
        canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, "OK: attack!");
    } else {
        canvas_draw_str_aligned(
            c, 64, 10, AlignCenter, AlignCenter, app->battle_won ? "You win!" : "You lost...");
        canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, "OK: continue");
    }
}

static void pet_new(Pet* p, uint8_t species) {
    memset(p, 0, sizeof(Pet));
    p->version = SAVE_VERSION;
    p->species = species % SpCount;
    p->stage = StageEgg;
    p->hunger = 2;
    p->strength = 1;
    p->mood = 3;
    p->weight = 5;
    p->last_seen = furi_hal_rtc_get_timestamp();
}

// The pet never leaves: neglect only costs stats, mood and evolution quality.
static void pet_tick(Pet* p) {
    p->age++;
    if(p->stage == StageEgg) {
        if(p->age >= HATCH_TICKS) p->stage = StageBaby;
        return;
    }
    if(p->age % HUNGER_EVERY == 0 && p->hunger > 0) p->hunger--;
    if(p->age % STRENGTH_EVERY == 0 && p->strength > 0) p->strength--;
    if(p->age % MOOD_EVERY == 0 && p->mood > 0) p->mood--;
    if(p->age % POOP_EVERY == 0 && p->poop < 4) p->poop++;
    if(p->poop >= 3 && !p->sick && rand() % 4 == 0) p->sick = 1;

    if(p->hunger == 0) {
        if(p->starving < 0xFFFF) p->starving++;
        if(p->starving == STARVE_MISTAKE_TICKS && p->mistakes < 255) p->mistakes++;
    } else {
        p->starving = 0;
    }

    if(p->stage == StageBaby && p->age >= CHILD_TICKS) p->stage = StageChild;
    if(p->stage == StageChild && p->age >= ADULT_TICKS) {
        p->stage = StageAdult;
        p->good_adult = p->mistakes <= 2 && p->weight <= 30;
    }
}

static void pet_save(const Pet* p) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, p, sizeof(Pet));
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static bool pet_load(Pet* p) {
    bool ok = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = storage_file_read(f, p, sizeof(Pet)) == sizeof(Pet) && p->version == SAVE_VERSION &&
             p->species < SpCount && p->stage <= StageAdult;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    if(!ok) return false;

    // Catch up on time spent with the app closed.
    uint32_t now = furi_hal_rtc_get_timestamp();
    if(now > p->last_seen) {
        uint32_t ticks = (now - p->last_seen) / SECONDS_PER_TICK;
        if(ticks > MAX_OFFLINE_TICKS) ticks = MAX_OFFLINE_TICKS;
        for(uint32_t i = 0; i < ticks; i++)
            pet_tick(p);
    }
    p->last_seen = now;
    return true;
}

static int step_toward(int cur, int target, int speed) {
    int d = target - cur;
    if(d > speed) return cur + speed;
    if(d < -speed) return cur - speed;
    return target;
}

static void pick_wander_target(App* app) {
    app->tx = rand() % 1000 - 500;
    app->tz = rand() % 900 + 50;
}

static void world_reset(App* app) {
    app->px = 0;
    app->pz = 400;
    app->idle = 10;
    app->play = 0;
    pick_wander_target(app);
}

static void world_update(App* app) {
    const Pet* p = &app->pet;
    app->moving = false;
    if(p->stage == StageEgg) {
        app->px = 0;
        app->pz = 400;
        return;
    }
    int speed = p->sick ? 4 : (p->mood == 0 ? 6 : 10);

    if(app->play > 0) {
        app->play--;
        app->bx = step_toward(app->bx, app->btx, 22);
        app->bz = step_toward(app->bz, app->btz, 22);
        app->tx = app->bx;
        app->tz = app->bz;
        if(abs(app->px - app->bx) < 60 && abs(app->pz - app->bz) < 60) {
            // Kick the ball somewhere new, often off into the distance.
            app->btx = rand() % 1000 - 500;
            app->btz = rand() % 1000;
        }
    } else if(app->idle > 0) {
        app->idle--;
        return;
    }

    if(app->px == app->tx && app->pz == app->tz) {
        if(app->play == 0) {
            app->idle = (p->mood == 0 ? 40 : 10) + rand() % 30;
            pick_wander_target(app);
        }
        return;
    }
    if(app->tx != app->px) app->face_left = app->tx < app->px;
    app->px = step_toward(app->px, app->tx, speed);
    app->pz = step_toward(app->pz, app->tz, speed);
    app->moving = true;
}

static void show_message(App* app, const char* text) {
    strncpy(app->message, text, sizeof(app->message) - 1);
    app->message[sizeof(app->message) - 1] = '\0';
    app->mode = ModeMessage;
}

static void draw_cb(Canvas* c, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(c);
    switch(app->mode) {
    case ModeSelect:
        draw_select(c, app);
        break;
    case ModeStatus:
        draw_status(c, app);
        break;
    case ModeTrain:
        draw_train(c, app);
        break;
    case ModeBattle:
        draw_battle(c, app);
        break;
    case ModeMessage:
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 28, AlignCenter, AlignCenter, app->message);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 50, AlignCenter, AlignCenter, "OK");
        break;
    default:
        draw_main(c, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    furi_message_queue_put(ctx, event, FuriWaitForever);
}

static void do_action(App* app) {
    Pet* p = &app->pet;
    if(p->stage == StageEgg && app->action != ActStatus) {
        show_message(app, "The egg is still warm...");
        return;
    }
    switch(app->action) {
    case ActFeed:
        if(p->hunger >= 4) {
            show_message(app, "Not hungry!");
        } else {
            p->hunger++;
            if(p->weight < 99) p->weight++;
            show_message(app, "Yum!");
        }
        break;
    case ActPlay:
        if(p->mood < 4) p->mood++;
        if(p->weight > 1 && rand() % 2) p->weight--;
        app->play = PLAY_FRAMES;
        app->bx = app->px + (app->face_left ? -150 : 150);
        app->bz = app->pz;
        app->btx = rand() % 1000 - 500;
        app->btz = rand() % 1000;
        app->idle = 0;
        break;
    case ActTrain:
        app->train_pos = 0;
        app->train_dir = 3;
        app->mode = ModeTrain;
        break;
    case ActBattle:
        if(p->hunger == 0 || p->sick) {
            show_message(app, p->sick ? "Too sick to fight" : "Too hungry to fight");
        } else {
            app->battle_done = false;
            app->enemy = rand() % SpCount;
            app->mode = ModeBattle;
        }
        break;
    case ActClean:
        p->poop = 0;
        show_message(app, "All clean!");
        break;
    case ActMedic:
        if(p->sick) {
            p->sick = 0;
            show_message(app, "Feeling better!");
        } else {
            show_message(app, "Healthy already");
        }
        break;
    default:
        app->mode = ModeStatus;
        break;
    }
}

static void handle_ok(App* app) {
    Pet* p = &app->pet;
    switch(app->mode) {
    case ModeSelect:
        pet_new(p, app->select);
        app->has_pet = true;
        app->action = ActFeed;
        world_reset(app);
        pet_save(p);
        app->mode = ModeMain;
        break;
    case ModeMain:
        do_action(app);
        break;
    case ModeTrain: {
        bool hit = app->train_pos >= 38 && app->train_pos <= 58;
        if(hit) {
            if(p->strength < 4) p->strength++;
            if(p->weight > 1) p->weight--;
        }
        show_message(app, hit ? "Great training!" : "Missed! Try again");
        break;
    }
    case ModeBattle:
        if(!app->battle_done) {
            int power = p->strength * 3 + p->stage * 3 + rand() % 6;
            int enemy = rand() % 10 + p->stage * 3;
            app->battle_won = power >= enemy;
            p->battles++;
            if(app->battle_won) {
                p->wins++;
                if(p->mood < 4) p->mood++;
            }
            if(p->hunger > 0) p->hunger--;
            app->battle_done = true;
        } else {
            app->mode = ModeMain;
        }
        break;
    case ModeStatus:
        break;
    default:
        app->mode = ModeMain;
        break;
    }
}

int32_t digipet_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    srand(furi_hal_random_get());
    app->has_pet = pet_load(&app->pet);
    app->mode = app->has_pet ? ModeMain : ModeSelect;
    world_reset(app);
    app->last_tick_ms = furi_get_tick();

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    bool running = true;
    InputEvent ev;
    while(running) {
        bool got = furi_message_queue_get(queue, &ev, 100) == FuriStatusOk;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(got && ev.type == InputTypeLong && ev.key == InputKeyOk && app->mode == ModeStatus) {
            app->select = app->pet.species;
            app->mode = ModeSelect;
        } else if(got && (ev.type == InputTypeShort || ev.type == InputTypeRepeat)) {
            if(ev.key == InputKeyBack) {
                if(app->mode == ModeMain || (app->mode == ModeSelect && !app->has_pet))
                    running = false;
                else
                    app->mode = ModeMain;
            } else if(app->mode == ModeSelect && ev.key == InputKeyLeft) {
                app->select = (app->select + SpCount - 1) % SpCount;
            } else if(app->mode == ModeSelect && ev.key == InputKeyRight) {
                app->select = (app->select + 1) % SpCount;
            } else if(app->mode == ModeMain && ev.key == InputKeyLeft) {
                app->action = (app->action + ActCount - 1) % ActCount;
            } else if(app->mode == ModeMain && ev.key == InputKeyRight) {
                app->action = (app->action + 1) % ActCount;
            } else if(ev.key == InputKeyOk && ev.type == InputTypeShort) {
                handle_ok(app);
            }
        }

        app->anim++;
        if(app->has_pet) world_update(app);
        if(app->mode == ModeTrain) {
            app->train_pos += app->train_dir;
            if(app->train_pos <= 0 || app->train_pos >= 96) app->train_dir = -app->train_dir;
        }
        if(app->has_pet && furi_get_tick() - app->last_tick_ms >= SECONDS_PER_TICK * 1000U) {
            app->last_tick_ms = furi_get_tick();
            pet_tick(&app->pet);
            app->pet.last_seen = furi_hal_rtc_get_timestamp();
            pet_save(&app->pet);
        }
        furi_mutex_release(app->mutex);
        view_port_update(vp);
    }

    if(app->has_pet) {
        app->pet.last_seen = furi_hal_rtc_get_timestamp();
        pet_save(&app->pet);
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
