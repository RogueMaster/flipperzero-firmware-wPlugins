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
#include <math.h>

#include "sprites.h"

#define SAVE_DIR   EXT_PATH("apps_data/yulia_cats")
#define SAVE_PATH  SAVE_DIR "/save.bin"
#define SAVE_MAGIC 0x31434C59 // "YLC1"

// Set to an hour (0-23) to test the day and night looks; -1 uses the clock.
#define DEBUG_HOUR -1

#define TICK_MS      100
#define STAT_MAX     1000
#define DECAY_PERIOD 15 // seconds per point of hunger / boredom
#define AWAY_CAP     (12 * 60 * 60)
#define AWAY_FLOOR   200 // time away never drops a stat below this

#define ROOM_X    43
#define ROOM_MINX 58
#define ROOM_MAXX 112
#define FLOOR_Y   52
#define BAR_Y     54
#define BOWL_X    80
#define PET_X     57

// Favourite places.
#define WINDOW_X    100
#define WINDOW_Y    5
#define WINDOW_W    23
#define WINDOW_H    21
#define SILL_SEAT_X 111 // where a cat sits on the window sill
#define SUN_X       102 // middle of the afternoon sun patch
#define TREE_SEAT_X 117
#define TREE_TOP_Y  33
#define BOWL_SEAT_X (BOWL_X - 9)

#define CAT_COUNT   2
#define NUGGET      0
#define BABY        1
#define HEART_COUNT 8

#define SLEEPY_BELOW 150
#define RESTED_ABOVE 800
#define HUNGRY_BELOW 250
#define FULL_ABOVE   850

#define REACT_TICKS 70 // how long everyone reacts to a new record
#define VOLUME_MAX  5
#define NOTE_GAP_MS 12

typedef enum {
    CatSit,
    CatWalk,
    CatSleep,
    CatEat,
    CatChase,
    CatComePet,
    CatZoom,
    CatSpot, // settled at one of the favourite places
} CatMode;

// Where a wandering cat is headed, and what it does once it gets there.
typedef enum {
    SpotNone,
    SpotWindow, // watch the birds from the sill
    SpotTree, // nap on the cat tree
    SpotSun, // nap in the afternoon sun
    SpotBowl, // sit by the empty dish and stare at Yulia
    SpotFriend, // sit next to the other cat
    SpotCompany, // keep Yulia company while she is busy
} Spot;

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
    Spot spot;
    uint16_t timer;
    uint8_t pet_timer;
    uint8_t bubble;
    const char* bubble_text;
    bool snooze; // napping by choice rather than from tiredness
    bool hurry;
    bool eat_treat;
} Cat;

// Yulia's outfit, chosen in the wardrobe.
typedef struct {
    uint8_t hair;
    uint8_t color;
    uint8_t glasses;
    uint8_t sweater;
} Look;

#define LOOK_ROWS 4

// Record player settings, chosen on the Vinyl screen.
typedef struct {
    uint8_t on;
    uint8_t volume; // 1..VOLUME_MAX
    uint8_t genre;
    uint8_t reserved;
} Sound;

#define VINYL_ROWS 3

// How the room is decorated.
typedef struct {
    uint8_t rug;
    uint8_t plant;
    uint8_t tree;
    uint8_t lights;
} Decor;

#define DECOR_ROWS  4
#define RUG_COUNT   4
#define PLANT_COUNT 4

typedef enum {
    GenreChill,
    GenreHipHop,
    GenrePop,
    GenreHouse,
    GenreParty,
    GenreMetal,
    GenreCount,
} GenreId;

typedef struct {
    uint8_t note; // MIDI note number, 0 = rest
    uint8_t steps;
} Note;

typedef struct {
    const char* name;
    const char* cheer;
    const Note* notes;
    uint16_t count;
    uint16_t step_ms;
} Genre;

// Photos for the album, each unlocked by a little milestone.
typedef enum {
    PhotoFirstCuddle,
    PhotoNap,
    PhotoDinner,
    PhotoTreats,
    PhotoDance,
    PhotoZoomies,
    PhotoBirds,
    PhotoFifty,
    PhotoWeek,
    PhotoPiggy,
    PhotoArtist,
    PhotoBumped,
    PhotoCount,
} Photo;

typedef struct {
    const char* title;
    const char* caption;
    const char* hint;
} PhotoInfo;

typedef struct {
    uint32_t magic;
    uint32_t first_ts;
    uint32_t last_ts;
    uint32_t love;
    int16_t cozy;
    CatStats cats[CAT_COUNT];
    Look look; // added later: older saves end before this field
    Sound sound;
    Decor decor;
    uint32_t photos;
    uint16_t treats;
    uint16_t arts;
} SaveData;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t life;
} Heart;

// The action bar has groups, each opening onto a few choices.
typedef enum {
    ActDinner,
    ActTreat,
    ActYarn,
    ActLaser,
    ActPetNugget,
    ActPetBaby,
    ActTea,
    ActRead,
    ActArt,
    ActKeys,
    ActWardrobe,
    ActDecor,
    ActVinyl,
    ActAlbum,
    ActNap,
} Act;

typedef struct {
    const char* name;
    uint8_t first;
    uint8_t count;
} Group;

static const Group groups[] = {
    {"Feed", ActDinner, 2},
    {"Play", ActYarn, 2},
    {"Pet", ActPetNugget, 2},
    {"Yulia", ActTea, 4},
    {"Home", ActWardrobe, 4},
    {"Nap time", ActNap, 1},
};
#define GROUP_COUNT COUNT_OF(groups)

typedef enum {
    ScreenRoom,
    ScreenStats,
    ScreenWardrobe,
    ScreenVinyl,
    ScreenDecor,
    ScreenAlbum,
} Screen;

typedef enum {
    ToyNone,
    ToyYarn,
    ToyLaser,
} Toy;

// What Yulia is busy with.
typedef enum {
    DoNothing,
    DoTea,
    DoRead,
    DoArt,
    DoKeys,
} Doing;

typedef enum {
    PhaseMorning, // 7-12
    PhaseAfternoon, // 12-18: sun on the rug
    PhaseEvening, // 18-22: lamp on
    PhaseNight, // 22-7: sleepy cats
} Phase;

typedef enum {
    FaceSmile,
    FaceHappy,
    FaceFlat,
    FaceSad,
    FaceSip,
    FaceAsleep,
    FaceDown, // looking down at a book or sketch
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
    Phase phase;

    Screen screen;
    uint8_t group;
    uint8_t sub[GROUP_COUNT];
    bool in_group;
    uint8_t row; // selected row on the wardrobe, vinyl and decor screens
    uint8_t album_page;

    Look look;
    Decor decor;
    Sound sound;
    Sound sound_before; // to tell whether the record was changed
    bool music_held; // we own the speaker
    bool music_gate;
    uint16_t music_pos;
    uint32_t music_next;
    uint8_t react_timer;
    uint8_t react_genre;

    uint32_t photos;
    uint16_t treats;
    uint16_t arts;

    bool lights_off;
    bool bowl_full;
    Toy toy;
    uint8_t toy_timer;
    int16_t toy_x;
    int16_t toy_y;
    int16_t toy_tx;
    int8_t toy_dx;
    Doing doing;
    uint8_t doing_timer;
    uint8_t art_timer; // the finished drawing is held up for a moment
    uint8_t art_cat;
    uint8_t speech_timer;
    uint8_t speech;
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

static const char* const act_names[] = {
    [ActDinner] = "Dinner",
    [ActTreat] = "Treat",
    [ActYarn] = "Yarn",
    [ActLaser] = "Laser",
    [ActPetNugget] = "Nugget",
    [ActPetBaby] = "Baby",
    [ActTea] = "Tea",
    [ActRead] = "Read",
    [ActArt] = "Make art",
    [ActKeys] = "Keyboard",
    [ActWardrobe] = "Wardrobe",
    [ActDecor] = "Decor",
    [ActVinyl] = "Vinyl",
    [ActAlbum] = "Album",
    [ActNap] = "Nap time",
};

static const char* const rug_names[RUG_COUNT] = {"Dots", "Stripes", "Oval", "None"};
static const char* const plant_names[PLANT_COUNT] = {"Sprout", "Cactus", "Flowers", "Fern"};
static const char* const tree_names[2] = {"No", "Yes"};
static const char* const lights_names[2] = {"None", "Fairy"};

static const PhotoInfo photo_info[PhotoCount] = {
    [PhotoFirstCuddle] = {"First cuddle", "The very first pet", "Pet a cat"},
    [PhotoNap] = {"Nap together", "Everyone fast asleep", "Take a nap"},
    [PhotoDinner] = {"Dinner for 2", "Side by side", "Feed both cats at once"},
    [PhotoTreats] = {"Treat fiend", "Baby never says no", "Give 5 treats"},
    [PhotoDance] = {"Dance party", "Paws in the air", "Play a Party record"},
    [PhotoZoomies] = {"Zoomies", "Baby goes brrr", "Play a Metal record"},
    [PhotoBirds] = {"Bird watch", "Nugget on patrol", "Wait for Nugget..."},
    [PhotoFifty] = {"50 cuddles", "So much love", "Pet the cats 50 times"},
    [PhotoWeek] = {"One week", "Seven cozy days", "Stay together 7 days"},
    [PhotoPiggy] = {"Piggy!", "Yulia's little piggy", "Yulia will say it..."},
    [PhotoArtist] = {"Artist", "Portrait of a cat", "Make art 3 times"},
    [PhotoBumped] = {"Bumped", "It's Baby's spot now", "Watch the food dish"},
};

// Short original loops for the piezo speaker, one per genre.
// Chill is lo-fi: lazy swung eighths (two steps, then one) over a jazzy
// Dm9 - G13 - Cmaj7 - Am7 turnaround, with room to breathe.
static const Note song_chill[] = {
    {62, 2}, {65, 1}, {69, 2}, {72, 1}, {76, 3}, {0, 3},  {71, 2}, {69, 1}, {67, 2}, {64, 1},
    {65, 3}, {0, 3},  {64, 2}, {67, 1}, {71, 2}, {67, 1}, {74, 3}, {71, 2}, {67, 1}, {69, 3},
    {0, 2},  {64, 1}, {67, 2}, {69, 1}, {60, 3}, {65, 2}, {69, 1}, {72, 2}, {76, 1}, {74, 3},
    {72, 2}, {69, 1}, {71, 2}, {67, 1}, {65, 2}, {62, 1}, {64, 3}, {0, 3},  {67, 2}, {64, 1},
    {60, 2}, {64, 1}, {67, 3}, {71, 3}, {69, 2}, {67, 1}, {64, 2}, {60, 1}, {57, 4}, {0, 2},
};
// Hip hop is boom bap: a low thump on the one, a high snap on two and four,
// and a short minor hook in between.
static const Note song_hiphop[] = {
    {53, 2}, {0, 1},  {53, 1}, {87, 1}, {0, 1},  {65, 1}, {68, 1}, {0, 1},  {53, 1}, {53, 1},
    {0, 1},  {87, 1}, {0, 1},  {63, 1}, {65, 1}, {51, 2}, {0, 1},  {51, 1}, {87, 1}, {0, 1},
    {63, 1}, {66, 1}, {0, 1},  {51, 1}, {51, 1}, {0, 1},  {87, 1}, {0, 1},  {68, 1}, {65, 1},
    {53, 2}, {0, 1},  {53, 1}, {87, 1}, {0, 1},  {72, 1}, {75, 1}, {72, 2}, {68, 1}, {0, 1},
    {87, 1}, {0, 1},  {70, 1}, {68, 1}, {51, 2}, {0, 1},  {51, 1}, {87, 1}, {0, 1},  {65, 1},
    {0, 1},  {51, 1}, {51, 1}, {63, 1}, {0, 1},  {87, 1}, {87, 1}, {0, 2},
};
static const Note song_pop[] = {
    {72, 1}, {72, 1}, {67, 1}, {69, 1}, {72, 2}, {76, 1}, {74, 1}, {72, 1}, {69, 1}, {67, 2},
    {0, 2},  {69, 1}, {69, 1}, {65, 1}, {67, 1}, {69, 2}, {72, 1}, {71, 1}, {67, 2}, {74, 2},
    {72, 1}, {72, 1}, {67, 1}, {69, 1}, {72, 2}, {76, 1}, {79, 1}, {77, 1}, {76, 1}, {74, 2},
    {0, 1},  {74, 1}, {76, 1}, {74, 1}, {72, 1}, {71, 1}, {72, 3}, {0, 3},
};
static const Note song_house[] = {
    {45, 1}, {0, 1}, {69, 1}, {0, 1}, {45, 1}, {0, 1}, {72, 1}, {0, 1},
    {45, 1}, {0, 1}, {69, 1}, {0, 1}, {45, 1}, {0, 1}, {76, 1}, {74, 1},
    {41, 1}, {0, 1}, {65, 1}, {0, 1}, {41, 1}, {0, 1}, {69, 1}, {0, 1},
    {43, 1}, {0, 1}, {67, 1}, {0, 1}, {43, 1}, {0, 1}, {71, 1}, {74, 1},
};
static const Note song_party[] = {
    {60, 1}, {64, 1}, {67, 1}, {72, 1}, {67, 1}, {64, 1}, {60, 2}, {62, 1}, {65, 1},
    {69, 1}, {74, 1}, {69, 1}, {65, 1}, {62, 2}, {64, 1}, {67, 1}, {71, 1}, {76, 1},
    {79, 2}, {76, 2}, {72, 1}, {72, 1}, {0, 1},  {72, 1}, {0, 1},  {79, 1}, {84, 2},
};
static const Note song_metal[] = {
    {52, 1}, {52, 1}, {64, 1}, {52, 1}, {52, 1}, {63, 1}, {52, 1}, {52, 1}, {62, 1},
    {52, 1}, {52, 1}, {60, 1}, {59, 2}, {55, 2}, {52, 2}, {52, 1}, {52, 1}, {52, 2},
    {52, 1}, {52, 1}, {55, 2}, {57, 2}, {58, 1}, {57, 1}, {55, 2}, {52, 1}, {52, 1},
    {64, 1}, {52, 1}, {52, 1}, {67, 1}, {66, 2}, {64, 4},
};

static const Genre genres[GenreCount] = {
    [GenreChill] = {"Chill", "So mellow...", song_chill, COUNT_OF(song_chill), 250},
    [GenreHipHop] = {"Hip hop", "Boom bap!", song_hiphop, COUNT_OF(song_hiphop), 165},
    [GenrePop] = {"Pop", "So catchy!", song_pop, COUNT_OF(song_pop), 170},
    [GenreHouse] = {"House", "Untz untz!", song_house, COUNT_OF(song_house), 120},
    [GenreParty] = {"Party", "Party time!", song_party, COUNT_OF(song_party), 110},
    [GenreMetal] = {"Metal", "METAL!!", song_metal, COUNT_OF(song_metal), 95},
};

// What Yulia plays on her keyboard: a gentle little waltz.
static const Note song_keys[] = {
    {60, 2}, {64, 2}, {67, 2}, {72, 4}, {71, 2}, {69, 2}, {65, 2}, {69, 2}, {67, 4}, {0, 2},
    {65, 2}, {64, 2}, {62, 2}, {67, 4}, {64, 2}, {62, 2}, {59, 2}, {62, 2}, {60, 6},
};
static const Genre keys_tune = {"Keyboard", "Bravo!", song_keys, COUNT_OF(song_keys), 200};
#define KEYS_TICKS 98 // one pass of the waltz

static int16_t clamp_stat(int32_t v) {
    return v < 0 ? 0 : v > STAT_MAX ? STAT_MAX : v;
}

static int16_t rand_range(int16_t lo, int16_t hi) {
    return lo + (int16_t)(furi_hal_random_get() % (uint32_t)(hi - lo + 1));
}

static bool chance(uint32_t percent) {
    return furi_hal_random_get() % 100 < percent;
}

static Phase phase_now(void) {
    int hour = DEBUG_HOUR;
    if(hour < 0) {
        DateTime now;
        furi_hal_rtc_get_datetime(&now);
        hour = now.hour;
    }
    if(hour >= 7 && hour < 12) return PhaseMorning;
    if(hour >= 12 && hour < 18) return PhaseAfternoon;
    if(hour >= 18 && hour < 22) return PhaseEvening;
    return PhaseNight;
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
    memset(&save, 0, sizeof(save));
    bool ok = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Fields after `look` were added over time; a shorter file leaves
        // them zeroed, which is each one's default.
        size_t got = storage_file_read(file, &save, sizeof(save));
        ok = got >= offsetof(SaveData, look) && save.magic == SAVE_MAGIC;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        game_new(app, now);
        app->sound.volume = 3;
        app->toast = "Hello, Yulia!";
        app->toast_timer = 30;
    } else {
        app->sound = (Sound){
            .on = save.sound.on ? 1 : 0,
            .volume =
                save.sound.volume >= 1 && save.sound.volume <= VOLUME_MAX ? save.sound.volume : 3,
            .genre = save.sound.genre % GenreCount,
        };
        app->first_ts = save.first_ts;
        app->love = save.love;
        app->look = (Look){
            .hair = save.look.hair % HAIR_STYLE_COUNT,
            .color = save.look.color % HAIR_COLOR_COUNT,
            .glasses = save.look.glasses % GLASSES_COUNT,
            .sweater = save.look.sweater % SWEATER_COUNT,
        };
        app->decor = (Decor){
            .rug = save.decor.rug % RUG_COUNT,
            .plant = save.decor.plant % PLANT_COUNT,
            .tree = save.decor.tree ? 1 : 0,
            .lights = save.decor.lights ? 1 : 0,
        };
        app->photos = save.photos;
        app->treats = save.treats;
        app->arts = save.arts;
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
        .look = app->look,
        .sound = app->sound,
        .decor = app->decor,
        .photos = app->photos,
        .treats = app->treats,
        .arts = app->arts,
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

// Adds a photo to the album the first time its milestone is reached.
static void unlock(App* app, Photo photo) {
    if(app->photos & (1u << photo)) return;
    app->photos |= 1u << photo;
    app->toast = "New photo!";
    app->toast_timer = 35;
}

// Yulia says something in Russian; the bubble shows it with a translation.
static void say(App* app, uint8_t phrase) {
    app->speech = phrase;
    app->speech_timer = 38;
    if(phrase == RU_PIGGY || phrase == RU_PIGLET) unlock(app, PhotoPiggy);
}

static void say_maybe(App* app, uint8_t phrase, uint32_t percent) {
    if(chance(percent)) say(app, phrase);
}

static void meow(const App* app) {
    // The record player has the speaker while music is on.
    if(app->music_held || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;
    if(!furi_hal_speaker_acquire(30)) return;
    furi_hal_speaker_start(880.0f, 0.4f);
    furi_delay_ms(45);
    furi_hal_speaker_start(1175.0f, 0.4f);
    furi_delay_ms(60);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

// ---------------------------------------------------------------- music

static void music_stop(App* app) {
    if(!app->music_held) return;
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    app->music_held = false;
}

static void music_restart(App* app) {
    if(app->music_held) furi_hal_speaker_stop();
    app->music_pos = 0;
    app->music_gate = false;
    app->music_next = furi_get_tick();
}

// Steps the music along. Called from the main loop whenever a note is due.
// Yulia's keyboard takes over from the record while she plays; everything
// rests during naps and in the Flipper's stealth mode.
static void music_update(App* app, uint32_t now) {
    static const float volumes[VOLUME_MAX] = {0.01f, 0.03f, 0.08f, 0.2f, 0.5f};

    const bool keys = app->doing == DoKeys;
    bool want = (app->sound.on || keys) && !app->lights_off &&
                !furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
    if(!want) {
        music_stop(app);
        return;
    }
    if(!app->music_held) {
        if(!furi_hal_speaker_acquire(10)) return;
        app->music_held = true;
        music_restart(app);
    }

    const Genre* genre = keys ? &keys_tune : &genres[app->sound.genre];
    while((int32_t)(now - app->music_next) >= 0) {
        if(app->music_gate) {
            // A sliver of silence so repeated notes are heard separately.
            furi_hal_speaker_stop();
            app->music_gate = false;
            app->music_pos = (app->music_pos + 1) % genre->count;
            app->music_next += NOTE_GAP_MS;
        } else {
            const Note* note = &genre->notes[app->music_pos % genre->count];
            if(note->note) {
                float freq = 440.0f * powf(2.0f, ((float)note->note - 69.0f) / 12.0f);
                if(!keys && app->sound.genre == GenreChill) {
                    // A little tape wobble.
                    freq *= (app->music_pos & 1) ? 1.004f : 0.997f;
                }
                furi_hal_speaker_start(freq, volumes[app->sound.volume - 1]);
            }
            app->music_gate = true;
            app->music_next += (uint32_t)note->steps * genre->step_ms - NOTE_GAP_MS;
        }
    }
}

static bool music_playing(const App* app) {
    return app->music_held;
}

// ---------------------------------------------------------------- cats

static void heart_spawn(App* app, int16_t x, int16_t y) {
    for(int i = 0; i < HEART_COUNT; i++) {
        if(app->hearts[i].life == 0) {
            app->hearts[i] = (Heart){.x = x + rand_range(-5, 5), .y = y, .life = 14};
            return;
        }
    }
}

static bool spot_is_nap(Spot spot) {
    return spot == SpotSun || spot == SpotTree;
}

static bool cat_asleep(const Cat* cat) {
    return cat->mode == CatSleep || (cat->mode == CatSpot && spot_is_nap(cat->spot));
}

// Idle and awake: free to be called over or to join in with something.
static bool cat_free(const Cat* cat) {
    if(cat->pet_timer) return false;
    if(cat->mode == CatSit || cat->mode == CatWalk) return true;
    return cat->mode == CatSpot && !spot_is_nap(cat->spot);
}

static bool cat_on_floor(const Cat* cat) {
    if(cat->mode == CatSit) return true;
    return cat->mode == CatSpot && cat->spot != SpotWindow && cat->spot != SpotTree;
}

static void cat_rest(Cat* cat) {
    cat->mode = CatSit;
    cat->spot = SpotNone;
    cat->snooze = false;
    cat->hurry = false;
    cat->timer = rand_range(15, 60);
}

static void cat_bubble(Cat* cat, const char* text, uint8_t ticks) {
    cat->bubble = ticks;
    cat->bubble_text = text;
}

static void cat_go(Cat* cat, Spot spot, int16_t target) {
    if(target < ROOM_MINX) target = ROOM_MINX;
    if(target > ROOM_MAXX) target = ROOM_MAXX;
    cat->mode = CatWalk;
    cat->spot = spot;
    cat->target = target;
}

static void cat_snooze(Cat* cat) {
    cat->mode = CatSleep;
    cat->spot = SpotNone;
    cat->snooze = true;
    cat->timer = rand_range(300, 600);
}

// A cat with nothing to do picks something in character. Nugget likes the
// window and keeps an eye on the dish; Baby follows the sun, naps on the cat
// tree and takes whatever spot Nugget has found.
static void cat_idle_pick(App* app, int i) {
    Cat* cat = &app->cats[i];
    Cat* other = &app->cats[1 - i];
    const uint32_t r = furi_hal_random_get() % 100;
    const bool day = app->phase == PhaseMorning || app->phase == PhaseAfternoon;
    const bool night = app->phase == PhaseNight;
    const bool sunny = app->phase == PhaseAfternoon;

    cat_go(cat, SpotNone, rand_range(ROOM_MINX, ROOM_MAXX));

    if(i == NUGGET) {
        if(r < (day ? 25u : 10u)) {
            cat_go(cat, SpotWindow, SILL_SEAT_X);
        } else if(r < 45 && !app->bowl_full && cat->s.full < 750) {
            cat_go(cat, SpotBowl, BOWL_SEAT_X);
        } else if(r < 57 && cat_on_floor(other)) {
            cat_go(cat, SpotFriend, other->x - 17);
        } else if(night && r < 85) {
            cat_snooze(cat);
        }
    } else {
        if(other->mode == CatSpot && other->spot == SpotBowl && r < 60) {
            cat_go(cat, SpotBowl, BOWL_SEAT_X - 3);
        } else if(sunny && r < 40) {
            cat_go(cat, SpotSun, SUN_X);
        } else if(app->decor.tree && r < (sunny ? 55u : 30u)) {
            cat_go(cat, SpotTree, ROOM_MAXX);
        } else if(r < 68 && cat_on_floor(other)) {
            cat_go(cat, SpotFriend, other->x + 17);
        } else if(night && r < 90) {
            cat_snooze(cat);
        }
    }
}

// The cat has reached wherever it was walking to.
static void cat_arrive(App* app, int i) {
    Cat* cat = &app->cats[i];
    cat->hurry = false;
    if(cat->spot == SpotNone) {
        cat_rest(cat);
        return;
    }
    cat->mode = CatSpot;
    switch(cat->spot) {
    case SpotWindow:
        cat->timer = rand_range(100, 220);
        unlock(app, PhotoBirds);
        break;
    case SpotTree:
    case SpotSun:
        cat->timer = rand_range(200, 400);
        break;
    case SpotBowl:
        cat->timer = rand_range(80, 160);
        if(i == NUGGET) {
            cat_bubble(cat, "meow?", 25);
            say_maybe(app, RU_HUNGRY, 40);
        } else {
            // Baby wants that spot. Nugget is nudged off and trots away.
            Cat* nugget = &app->cats[NUGGET];
            if(nugget->mode == CatSpot && nugget->spot == SpotBowl) {
                cat_go(nugget, SpotNone, ROOM_MAXX);
                nugget->hurry = true;
                cat_bubble(nugget, "!", 20);
                toast(app, "Baby took his spot");
                unlock(app, PhotoBumped);
                say_maybe(app, RU_PIGLET, 50);
            }
        }
        break;
    case SpotFriend:
        cat->timer = rand_range(60, 140);
        heart_spawn(app, cat->x, FLOOR_Y - 16);
        break;
    default:
        cat->timer = 70;
        cat->dir = -1;
        break;
    }
}

// Everyone has a moment with the new record. Cats that are busy or fast
// asleep carry on with what they were doing.
static void react_start(App* app) {
    app->react_timer = REACT_TICKS;
    app->react_genre = app->sound.genre;
    app->toast = genres[app->sound.genre].cheer;
    app->toast_timer = 30;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(!cat_free(cat)) continue;
        cat_rest(cat);
        cat->timer = REACT_TICKS;
        if(app->react_genre == GenreMetal) {
            if(i == NUGGET) {
                // Nugget has heard it all before.
                cat_bubble(cat, "...", 40);
            } else {
                // Baby gets the zoomies.
                cat->mode = CatZoom;
                cat->target = cat->x < 85 ? ROOM_MAXX : ROOM_MINX;
                unlock(app, PhotoZoomies);
            }
        }
    }
    if(app->react_genre == GenreParty) unlock(app, PhotoDance);
}

static bool busy(const App* app) {
    if(app->toy_timer || app->doing != DoNothing) return true;
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

static void do_dinner(App* app) {
    int hungry = 0;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat->s.full > FULL_ABOVE) continue;
        hungry++;
        cat->mode = CatEat;
        cat->spot = SpotNone;
        cat->eat_treat = false;
        cat->target = i == NUGGET ? BOWL_X - 10 : BOWL_X + 25;
        cat->timer = 30;
    }
    if(hungry) {
        app->bowl_full = true;
        app->happy_timer = 20;
        toast(app, "Dinner time!");
        say_maybe(app, RU_HUNGRY, 30);
        if(hungry == CAT_COUNT) unlock(app, PhotoDinner);
    } else {
        toast(app, "Not hungry");
    }
}

// A treat each. Baby is there before the bag is open; Nugget ambles over,
// unless he is too sleepy to bother.
static void do_treat(App* app) {
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(i == NUGGET && cat->s.energy < SLEEPY_BELOW) continue;
        cat->mode = CatEat;
        cat->spot = SpotNone;
        cat->eat_treat = true;
        cat->target = i == NUGGET ? 79 : 64;
        cat->timer = 18;
    }
    cat_bubble(&app->cats[BABY], "!!", 20);
    app->happy_timer = 30;
    app->treats++;
    toast(app, "Treats!");
    if(app->treats >= 5) unlock(app, PhotoTreats);
    say_maybe(app, chance(50) ? RU_PIGGY : RU_PIGLET, 60);
}

static void do_yarn(App* app) {
    int players = 0;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat->s.energy < SLEEPY_BELOW) continue;
        players++;
        cat->mode = CatChase;
        cat->spot = SpotNone;
    }
    if(players) {
        app->toy = ToyYarn;
        app->toy_timer = 80;
        app->toy_x = 86;
        app->toy_dx = furi_hal_random_get() & 1 ? 3 : -3;
        app->happy_timer = 80;
    } else {
        toast(app, "Too sleepy...");
    }
}

// The red dot. Baby cannot resist it; Nugget cannot be bothered.
static void do_laser(App* app) {
    Cat* baby = &app->cats[BABY];
    Cat* nugget = &app->cats[NUGGET];
    if(baby->s.energy < SLEEPY_BELOW) {
        toast(app, "Baby's too sleepy");
        return;
    }
    baby->mode = CatChase;
    baby->spot = SpotNone;
    if(cat_free(nugget)) {
        cat_rest(nugget);
        nugget->timer = 80;
        cat_bubble(nugget, "*yawn*", 30);
    }
    app->toy = ToyLaser;
    app->toy_timer = 80;
    app->toy_x = app->toy_tx = 86;
    app->toy_y = FLOOR_Y - 1;
    app->happy_timer = 80;
}

static void do_pet(App* app, int i) {
    Cat* cat = &app->cats[i];
    if(cat_asleep(cat)) {
        // A gentle pat where the cat is curled up.
        cat->pet_timer = 30;
    } else {
        cat->mode = CatComePet;
        cat->spot = SpotNone;
        cat->target = PET_X;
    }
    app->happy_timer = 50;
    if(i == NUGGET) {
        say_maybe(app, chance(50) ? RU_OLD_FELLOW : RU_SWEET, 45);
    } else {
        say_maybe(app, chance(50) ? RU_COME : RU_LITTLE_GIRL, 45);
    }
}

// Yulia settles down to something. A cat that is free comes to sit with her.
static void do_activity(App* app, Doing doing) {
    static const uint8_t ticks[] = {
        [DoTea] = 40, [DoRead] = 70, [DoArt] = 70, [DoKeys] = KEYS_TICKS};
    app->doing = doing;
    app->doing_timer = ticks[doing];
    app->cozy = clamp_stat(app->cozy + (doing == DoTea ? 300 : 250));
    if(doing == DoTea) {
        toast(app, "So cozy...");
        return;
    }
    if(doing == DoKeys) music_restart(app);
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        if(cat_free(cat)) {
            cat_go(cat, SpotCompany, PET_X + (i == BABY ? 3 : 0));
            break;
        }
    }
}

static void activity_done(App* app) {
    switch(app->doing) {
    case DoRead:
        toast(app, "A good chapter");
        break;
    case DoArt:
        app->arts++;
        app->art_cat = furi_hal_random_get() % CAT_COUNT;
        app->art_timer = 40;
        toast(app, app->art_cat == NUGGET ? "It's Nugget!" : "It's Baby!");
        if(app->arts >= 3) unlock(app, PhotoArtist);
        break;
    case DoKeys:
        toast(app, "Bravo!");
        for(int i = 0; i < CAT_COUNT; i++) {
            Cat* cat = &app->cats[i];
            cat->s.fun = clamp_stat(cat->s.fun + 60);
            if(!cat_asleep(cat)) heart_spawn(app, cat->x, FLOOR_Y - 16);
        }
        music_restart(app); // back to the record, if one is on
        break;
    default:
        break;
    }
    app->doing = DoNothing;
    app->happy_timer = 25;
}

static void do_nap(App* app) {
    if(app->lights_off) {
        lights_on(app);
        app->speech_timer = 0;
        if(chance(70)) {
            say(app, RU_GOOD_MORNING);
        } else {
            toast(app, "Good morning!");
        }
        return;
    }
    app->lights_off = true;
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        cat->mode = CatSleep;
        cat->spot = SpotNone;
        cat->snooze = false;
    }
    unlock(app, PhotoNap);
    say_maybe(app, RU_GOOD_NIGHT, 70);
}

static void open_screen(App* app, Screen screen) {
    app->screen = screen;
    app->row = 0;
}

static void do_action(App* app, Act act) {
    switch(act) {
    case ActWardrobe:
        open_screen(app, ScreenWardrobe);
        return;
    case ActDecor:
        open_screen(app, ScreenDecor);
        return;
    case ActVinyl:
        open_screen(app, ScreenVinyl);
        app->sound_before = app->sound;
        return;
    case ActAlbum:
        open_screen(app, ScreenAlbum);
        return;
    default:
        break;
    }
    if(busy(app)) return;
    if(act == ActNap) {
        do_nap(app);
        return;
    }
    lights_on(app);
    switch(act) {
    case ActDinner:
        do_dinner(app);
        break;
    case ActTreat:
        do_treat(app);
        break;
    case ActYarn:
        do_yarn(app);
        break;
    case ActLaser:
        do_laser(app);
        break;
    case ActPetNugget:
        do_pet(app, NUGGET);
        break;
    case ActPetBaby:
        do_pet(app, BABY);
        break;
    case ActTea:
        do_activity(app, DoTea);
        break;
    case ActRead:
        do_activity(app, DoRead);
        break;
    case ActArt:
        do_activity(app, DoArt);
        break;
    case ActKeys:
        do_activity(app, DoKeys);
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

    if(cat_asleep(cat) && app->tick % 10 == 0) cat->s.energy = clamp_stat(cat->s.energy + 4);

    switch(cat->mode) {
    case CatSit:
        if(cat->pet_timer) break;
        if(cat->s.energy < SLEEPY_BELOW) {
            cat->mode = CatSleep;
            cat->snooze = false;
        } else if(cat->timer) {
            cat->timer--;
            if(cat->s.full < HUNGRY_BELOW && !cat->bubble && furi_hal_random_get() % 250 == 0) {
                cat_bubble(cat, "meow!", 25);
                meow(app);
            }
        } else {
            cat_idle_pick(app, i);
        }
        break;
    case CatWalk:
        // Nugget is an older gentleman and takes his time.
        if(i == NUGGET && !cat->hurry && (app->tick & 1)) break;
        if(cat_step(cat, cat->hurry ? 2 : 1)) cat_arrive(app, i);
        break;
    case CatSpot:
        if(cat->pet_timer) break;
        if(cat->timer) {
            cat->timer--;
            const bool day = app->phase == PhaseMorning || app->phase == PhaseAfternoon;
            if(cat->spot == SpotWindow && day && !cat->bubble && furi_hal_random_get() % 90 == 0) {
                cat_bubble(cat, "ek ek!", 20);
            }
            // The sun has moved on.
            if(cat->spot == SpotSun && app->phase != PhaseAfternoon) cat->timer = 0;
        } else {
            cat_rest(cat);
        }
        break;
    case CatSleep:
        if(app->lights_off) break;
        if(cat->snooze) {
            if(cat->timer) {
                cat->timer--;
            } else {
                cat_rest(cat);
            }
        } else if(cat->s.energy > RESTED_ABOVE) {
            cat_rest(cat);
        }
        break;
    case CatEat: {
        int16_t speed = cat->eat_treat ? (i == BABY ? 3 : 1) : 2;
        if(cat_step(cat, speed)) {
            cat->dir = i == NUGGET ? 1 : -1;
            if(cat->timer) {
                cat->timer--;
            } else {
                if(cat->eat_treat) {
                    cat->s.full = clamp_stat(cat->s.full + 120);
                    cat->s.fun = clamp_stat(cat->s.fun + 100);
                } else {
                    cat->s.full = clamp_stat(cat->s.full + 450);
                }
                heart_spawn(app, cat->x, FLOOR_Y - 14);
                cat_rest(cat);
            }
        }
        break;
    }
    case CatChase: {
        const bool laser = app->toy == ToyLaser;
        cat->target = laser ? app->toy_x : app->toy_x + (i == NUGGET ? -8 : 8);
        if(cat->target < ROOM_MINX) cat->target = ROOM_MINX;
        if(cat->target > ROOM_MAXX) cat->target = ROOM_MAXX;
        cat_step(cat, laser ? 3 : (i == NUGGET ? 1 : 2));
        if(app->toy_timer == 0) {
            cat->s.fun = clamp_stat(cat->s.fun + (laser ? 400 : 350));
            cat->s.energy = clamp_stat(cat->s.energy - (laser ? 80 : 60));
            heart_spawn(app, cat->x, FLOOR_Y - 14);
            cat_rest(cat);
        }
        break;
    }
    case CatZoom:
        if(cat_step(cat, 3)) {
            if(app->react_timer) {
                cat->target = cat->x < 85 ? ROOM_MAXX : ROOM_MINX;
            } else {
                cat_rest(cat);
            }
        }
        break;
    case CatComePet:
        if(cat_step(cat, 2)) {
            cat_rest(cat);
            cat->dir = -1;
            cat->pet_timer = 35;
            cat->timer = 50;
        }
        break;
    }
}

static void toy_tick(App* app) {
    if(!app->toy_timer) return;
    app->toy_timer--;
    if(app->toy == ToyYarn) {
        app->toy_x += app->toy_dx;
        if(app->toy_x < ROOM_MINX - 4 || app->toy_x > ROOM_MAXX + 6) {
            app->toy_dx = -app->toy_dx;
            app->toy_x += app->toy_dx * 2;
        }
        if(app->toy_timer == 0) toast(app, "So much fun!");
    } else {
        // The dot darts about, sometimes up the wall where Baby can't reach.
        if(app->toy_timer % 8 == 0) {
            app->toy_tx = rand_range(ROOM_MINX - 4, ROOM_MAXX + 6);
            app->toy_y = chance(25) ? rand_range(30, 42) : FLOOR_Y - 1;
        }
        int16_t d = app->toy_tx - app->toy_x;
        if(d > 5) d = 5;
        if(d < -5) d = -5;
        app->toy_x += d;
        if(app->toy_timer == 0) toast(app, "Almost got it!");
    }
    // The cats notice the toy is gone on their own tick, then it is cleared.
}

static void check_milestones(App* app) {
    uint32_t now = furi_hal_rtc_get_timestamp();
    if(app->love >= 1) unlock(app, PhotoFirstCuddle);
    if(app->love >= 50) unlock(app, PhotoFifty);
    if(now > app->first_ts && (now - app->first_ts) / 86400 >= 6) unlock(app, PhotoWeek);
}

static void game_tick(App* app) {
    app->tick++;
    if(app->tick % 10 == 1) {
        app->phase = phase_now();
        check_milestones(app);
    }

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
            // Nugget tires sooner, everyone tires at night, and Baby is
            // always ready for a snack.
            if(!cat_asleep(cat)) {
                int drain = (i == NUGGET ? 2 : 1) + (app->phase == PhaseNight ? 1 : 0);
                cat->s.energy = clamp_stat(cat->s.energy - drain);
            }
            if(i == BABY && (app->decay_ts / DECAY_PERIOD) % 2)
                cat->s.full = clamp_stat(cat->s.full - 1);
        }
    }

    toy_tick(app);

    bool dinner = false;
    bool purring = false;
    for(int i = 0; i < CAT_COUNT; i++) {
        cat_tick(app, i);
        const Cat* cat = &app->cats[i];
        if(cat->mode == CatEat && !cat->eat_treat) dinner = true;
        if(cat->pet_timer) purring = true;
    }
    if(!dinner) app->bowl_full = false;
    if(!app->toy_timer) app->toy = ToyNone;

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

    if(app->react_timer) {
        app->react_timer--;
        if(app->react_genre == GenreParty && app->tick % 7 == 0) {
            heart_spawn(app, rand_range(ROOM_MINX, ROOM_MAXX), FLOOR_Y - 18);
        }
    }

    if(app->doing != DoNothing) {
        if(app->doing_timer) app->doing_timer--;
        if(app->doing_timer == 0) activity_done(app);
    }

    // Now and then Yulia just tells a cat how good it is.
    if(!app->lights_off && !app->speech_timer && !busy(app) && furi_hal_random_get() % 900 == 0) {
        say(app, RU_GOOD_KITTY);
    }

    if(app->art_timer) app->art_timer--;
    if(app->speech_timer) app->speech_timer--;
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

// canvas_draw_xbm paints the whole rectangle, so sprites with transparent
// pixels are plotted dot by dot instead.
static void draw_bits(Canvas* canvas, int32_t x, int32_t y, const Sprite* sprite, bool white) {
    const uint8_t* data = white ? sprite->white : sprite->black;
    const size_t stride = (sprite->w + 7) / 8;
    canvas_set_color(canvas, white ? ColorWhite : ColorBlack);
    for(size_t row = 0; row < sprite->h; row++) {
        for(size_t col = 0; col < stride; col++) {
            uint8_t bits = data[row * stride + col];
            for(int32_t bit = 0; bits; bit++, bits >>= 1) {
                if(bits & 1) canvas_draw_dot(canvas, x + col * 8 + bit, y + row);
            }
        }
    }
}

static void draw_sprite(Canvas* canvas, int32_t x, int32_t y, const Sprite* sprite) {
    draw_bits(canvas, x, y, sprite, true);
    draw_bits(canvas, x, y, sprite, false);
}

// Draws a sprite standing on `ground`, centred on `cx`.
static void draw_standing(Canvas* canvas, int32_t cx, int32_t ground, const Sprite* sprite) {
    draw_sprite(canvas, cx - sprite->w / 2, ground - sprite->h + 1, sprite);
}

static Face yulia_face(const App* app) {
    if(app->lights_off) return FaceAsleep;
    switch(app->doing) {
    case DoTea:
        return FaceSip;
    case DoRead:
    case DoArt:
        return FaceDown;
    case DoKeys:
        return FaceHappy;
    default:
        break;
    }
    if(app->react_timer) {
        switch(app->react_genre) {
        case GenreChill:
            return FaceAsleep; // eyes closed, soaking it in
        case GenreHouse:
        case GenreHipHop:
            return FaceSmile;
        default:
            return FaceHappy;
        }
    }
    if(app->happy_timer || app->speech_timer) return FaceHappy;
    int16_t m = mood(app);
    if(m > 550) return FaceSmile;
    if(m > 300) return FaceFlat;
    return FaceSad;
}

// Yulia moves to the music for a moment: a nod, a bounce or a headbang.
static int32_t yulia_bounce(const App* app) {
    if(app->doing == DoKeys) return (app->tick / 4) & 1;
    if(!app->react_timer || app->doing != DoNothing) return 0;
    switch(app->react_genre) {
    case GenrePop:
    case GenreHipHop:
        return (app->tick / 3) & 1;
    case GenreHouse:
        return (app->tick / 2) & 1;
    case GenreParty:
        return (app->tick / 2) & 1 ? -1 : 1;
    case GenreMetal:
        return (app->tick & 1) * 3;
    default:
        return 0;
    }
}

// Whatever Yulia is holding, drawn in front of her.
static void draw_activity(Canvas* canvas, const App* app, int32_t dy) {
    switch(app->doing) {
    case DoTea:
        draw_sprite(canvas, 17, 35 + dy, &spr_mug);
        // Steam curls up from the mug.
        canvas_set_color(canvas, ColorBlack);
        for(int i = 0; i < 2; i++) {
            int32_t x = 19 + i * 3;
            int32_t phase = (app->tick / 3 + i) & 3;
            for(int k = 0; k < 3; k++) {
                int32_t wobble = ((k + phase) & 3) == 0 ? 1 : 0;
                canvas_draw_dot(canvas, x + wobble, 33 - k + dy);
            }
        }
        break;
    case DoRead:
        draw_sprite(canvas, 10, 44, &spr_book);
        // A page turns now and then.
        if(app->doing_timer % 24 < 3) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_line(canvas, 20, 44, 23, 41);
        }
        break;
    case DoArt: {
        draw_sprite(canvas, 14, 43, &spr_pad);
        // The pencil scribbles away.
        int32_t wig = (app->tick / 2) % 4;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 27 + wig, 47 + (wig & 1), 33 + wig, 53);
        canvas_draw_line(canvas, 28 + wig, 47 + (wig & 1), 34 + wig, 53);
        break;
    }
    case DoKeys: {
        draw_sprite(canvas, 2, 45, &spr_keyboard);
        // Two keys go down at a time.
        canvas_set_color(canvas, ColorXOR);
        int32_t k = (app->tick / 2 * 3) % 9;
        canvas_draw_box(canvas, 3 + k * 4, 50, 3, 3);
        canvas_draw_box(canvas, 3 + ((k + 4) % 9) * 4, 50, 3, 3);
        canvas_set_color(canvas, ColorBlack);
        break;
    }
    default:
        break;
    }
}

static void draw_yulia(Canvas* canvas, const App* app) {
    const Look* look = &app->look;
    const int32_t dy = yulia_bounce(app);
    draw_sprite(canvas, 0, dy, yulia_sweater[look->sweater]);
    draw_sprite(canvas, 0, dy, &spr_yulia_face);
    draw_sprite(canvas, 0, dy, yulia_hair[look->hair][look->color]);
    draw_sprite(canvas, 0, dy, yulia_glasses[look->glasses]);
    Face face = yulia_face(app);
    canvas_set_color(canvas, ColorBlack);

    // Eyes sit behind the two lenses.
    const bool shades = app->react_timer && app->react_genre == GenreHipHop && !app->lights_off &&
                        app->doing == DoNothing;
    for(int i = 0; i < 2; i++) {
        int32_t x = i ? 26 : 14;
        if(shades) {
            // Too cool: the lenses go dark.
            canvas_draw_disc(canvas, x, 28 + dy, 4);
            canvas_draw_disc(canvas, x + 1, 28 + dy, 4);
        } else if(face == FaceHappy || face == FaceSip) {
            canvas_draw_line(canvas, x - 2, 29 + dy, x, 27 + dy);
            canvas_draw_line(canvas, x + 1, 27 + dy, x + 3, 29 + dy);
        } else if(face == FaceAsleep || app->blink) {
            canvas_draw_line(canvas, x - 1, 29 + dy, x + 2, 29 + dy);
        } else if(face == FaceDown) {
            canvas_draw_box(canvas, x, 28 + dy, 2, 3);
        } else {
            canvas_draw_box(canvas, x, 26 + dy, 2, 4);
        }
    }

    switch(face) {
    case FaceHappy:
        canvas_draw_line(canvas, 18, 37 + dy, 23, 37 + dy);
        canvas_draw_line(canvas, 19, 38 + dy, 22, 38 + dy);
        canvas_draw_line(canvas, 20, 39 + dy, 21, 39 + dy);
        break;
    case FaceFlat:
        canvas_draw_line(canvas, 19, 38 + dy, 22, 38 + dy);
        break;
    case FaceSad:
        canvas_draw_line(canvas, 19, 37 + dy, 22, 37 + dy);
        canvas_draw_dot(canvas, 18, 38 + dy);
        canvas_draw_dot(canvas, 23, 38 + dy);
        break;
    case FaceSip:
        break; // hidden behind the mug
    default:
        canvas_draw_dot(canvas, 18, 37 + dy);
        canvas_draw_dot(canvas, 23, 37 + dy);
        canvas_draw_line(canvas, 19, 38 + dy, 22, 38 + dy);
        break;
    }

    draw_activity(canvas, app, dy);
}

static void draw_window(Canvas* canvas, const App* app) {
    const int32_t x = WINDOW_X, y = WINDOW_Y, w = WINDOW_W, h = WINDOW_H;
    const bool dark = app->phase == PhaseEvening || app->phase == PhaseNight;

    canvas_set_color(canvas, ColorBlack);
    if(dark) {
        // Night sky with a moon; more stars come out late.
        canvas_draw_box(canvas, x, y, w, h);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, x + 16, y + 6, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, x + 18, y + 5, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_dot(canvas, x + 4, y + 4);
        canvas_draw_dot(canvas, x + 8, y + 15);
        if(app->phase == PhaseNight) {
            canvas_draw_dot(canvas, x + 6, y + 8);
            canvas_draw_dot(canvas, x + 18, y + 16);
            canvas_draw_dot(canvas, x + 14, y + 13);
            canvas_draw_dot(canvas, x + 3, y + 17);
        }
    } else {
        canvas_draw_frame(canvas, x, y, w, h);
        // The sun crosses the window through the day.
        int32_t sx = app->phase == PhaseMorning ? x + 6 : x + 16;
        int32_t sy = y + 6;
        canvas_draw_disc(canvas, sx, sy, 2);
        canvas_draw_dot(canvas, sx, sy - 4);
        canvas_draw_dot(canvas, sx - 4, sy);
        canvas_draw_dot(canvas, sx + 4, sy);
        canvas_draw_dot(canvas, sx, sy + 4);
        canvas_draw_dot(canvas, sx - 3, sy - 3);
        canvas_draw_dot(canvas, sx + 3, sy - 3);
        canvas_draw_dot(canvas, sx - 3, sy + 3);
        canvas_draw_dot(canvas, sx + 3, sy + 3);

        // Birds go by, which is the whole point of a window.
        canvas_set_color(canvas, ColorBlack);
        for(int i = 0; i < 2; i++) {
            int32_t bx = x + 2 + (app->tick / 3 + i * 11) % (w - 6);
            int32_t by = y + 13 + i * 4 + ((app->tick / 4 + i) & 1);
            canvas_draw_dot(canvas, bx, by);
            canvas_draw_dot(canvas, bx + 1, by + 1);
            canvas_draw_dot(canvas, bx + 2, by);
        }
    }

    // Glazing bars and the sill.
    canvas_set_color(canvas, dark ? ColorWhite : ColorBlack);
    canvas_draw_line(canvas, x + w / 2, y + 1, x + w / 2, y + h - 2);
    canvas_draw_line(canvas, x + 1, y + h / 2, x + w - 2, y + h / 2);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, x - 2, y + h, x + w + 1, y + h);
}

static void draw_plant(Canvas* canvas, uint8_t plant) {
    canvas_draw_frame(canvas, 55, 17, 6, 5); // pot
    switch(plant) {
    case 1: // cactus
        canvas_draw_box(canvas, 57, 10, 3, 7);
        canvas_draw_dot(canvas, 58, 9);
        canvas_draw_line(canvas, 55, 13, 55, 11);
        canvas_draw_dot(canvas, 56, 13);
        canvas_draw_line(canvas, 61, 14, 61, 12);
        canvas_draw_dot(canvas, 60, 14);
        break;
    case 2: // flowers
        canvas_draw_line(canvas, 58, 16, 55, 11);
        canvas_draw_line(canvas, 58, 16, 58, 9);
        canvas_draw_line(canvas, 58, 16, 61, 12);
        canvas_draw_circle(canvas, 54, 10, 1);
        canvas_draw_circle(canvas, 58, 8, 1);
        canvas_draw_circle(canvas, 62, 11, 1);
        break;
    case 3: // fern
        canvas_draw_line(canvas, 58, 16, 53, 11);
        canvas_draw_line(canvas, 53, 11, 52, 13);
        canvas_draw_line(canvas, 58, 16, 55, 8);
        canvas_draw_line(canvas, 58, 16, 58, 7);
        canvas_draw_line(canvas, 58, 16, 61, 8);
        canvas_draw_line(canvas, 58, 16, 63, 11);
        canvas_draw_line(canvas, 63, 11, 64, 13);
        break;
    default: // sprout
        canvas_draw_line(canvas, 58, 16, 58, 12);
        canvas_draw_line(canvas, 58, 14, 55, 11);
        canvas_draw_line(canvas, 58, 13, 61, 10);
        canvas_draw_dot(canvas, 58, 11);
        break;
    }
}

static void draw_rug(Canvas* canvas, uint8_t rug) {
    switch(rug) {
    case 0:
        for(int32_t x = 60; x < 118; x += 2)
            canvas_draw_dot(canvas, x, FLOOR_Y + 1);
        break;
    case 1:
        for(int32_t x = 58; x < 118; x++) {
            if((x - 58) % 6 < 4) canvas_draw_dot(canvas, x, FLOOR_Y + 1);
            if((x - 58) % 6 == 1) canvas_draw_dot(canvas, x, FLOOR_Y);
        }
        break;
    case 2:
        for(int32_t x = 60; x <= 116; x++) {
            float u = ((float)x - 88.0f) / 28.0f;
            int32_t dy = (int32_t)(3.0f * sqrtf(1.0f - u * u) + 0.5f);
            canvas_draw_dot(canvas, x, 50 - dy);
            canvas_draw_dot(canvas, x, 50 + dy);
        }
        break;
    default:
        break;
    }
}

static void draw_lamp(Canvas* canvas, bool lit) {
    canvas_draw_line(canvas, 47, 31, 47, 51); // pole
    canvas_draw_line(canvas, 44, 52, 50, 52); // foot
    // Shade: dark when off, glowing when on.
    for(int32_t i = 0; i <= 6; i++) {
        int32_t half = 2 + (i + 1) / 2;
        if(lit && i > 0 && i < 6) {
            canvas_draw_dot(canvas, 47 - half, 24 + i);
            canvas_draw_dot(canvas, 47 + half, 24 + i);
        } else {
            canvas_draw_line(canvas, 47 - half, 24 + i, 47 + half, 24 + i);
        }
    }
    if(lit) {
        static const int8_t glow[][2] = {
            {-4, 33},
            {0, 33},
            {4, 33},
            {-2, 36},
            {2, 36},
            {-6, 36},
            {6, 36},
            {-4, 39},
            {4, 39},
            {0, 39}};
        for(size_t i = 0; i < COUNT_OF(glow); i++) {
            if(glow[i][0] == 0) continue; // the pole is there
            canvas_draw_dot(canvas, 47 + glow[i][0], glow[i][1]);
        }
    }
}

static void draw_fairy_lights(Canvas* canvas, const App* app) {
    for(int32_t x = ROOM_X + 1; x < 128; x++) {
        int32_t p = (x - ROOM_X) % 12;
        canvas_draw_dot(canvas, x, (p < 6 ? p : 12 - p) / 3);
        // A bulb hangs at the bottom of each swag, and they twinkle in turn.
        if(p == 6 && ((x / 12 + app->tick / 6) % 3 != 0 || app->lights_off)) {
            canvas_draw_box(canvas, x - 1, 3, 2, 2);
        }
    }
}

static void draw_cat_tree(Canvas* canvas) {
    canvas_draw_box(canvas, 113, TREE_TOP_Y + 1, 15, 2); // perch
    canvas_draw_box(canvas, 120, TREE_TOP_Y + 3, 3, 16); // scratching post
    canvas_draw_box(canvas, 115, FLOOR_Y - 1, 13, 2); // base
    canvas_set_color(canvas, ColorWhite);
    for(int32_t y = TREE_TOP_Y + 5; y < FLOOR_Y - 2; y += 3)
        canvas_draw_dot(canvas, 121, y);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 115, TREE_TOP_Y + 3, 115, TREE_TOP_Y + 7); // dangling toy
    canvas_draw_box(canvas, 114, TREE_TOP_Y + 8, 3, 2);
}

static void draw_room(Canvas* canvas, const App* app) {
    const Decor* decor = &app->decor;
    draw_window(canvas, app);
    canvas_set_color(canvas, ColorBlack);

    // Shelf with a plant and a couple of books.
    canvas_draw_line(canvas, 52, 22, 78, 22);
    draw_plant(canvas, decor->plant);
    canvas_draw_frame(canvas, 66, 14, 3, 8);
    canvas_draw_frame(canvas, 69, 12, 3, 10);
    canvas_draw_line(canvas, 73, 21, 76, 13);

    const bool lamp_lit = !app->lights_off &&
                          (app->phase == PhaseEvening || app->phase == PhaseNight);
    draw_lamp(canvas, lamp_lit);
    if(decor->lights) draw_fairy_lights(canvas, app);

    // Skirting board, rug and whatever is standing on the floor.
    canvas_draw_line(canvas, ROOM_X, 43, 127, 43);
    draw_rug(canvas, decor->rug);
    if(decor->tree) draw_cat_tree(canvas);

    if(app->phase == PhaseAfternoon) {
        // A patch of afternoon sun slants across the floor.
        for(int32_t y = 45; y <= FLOOR_Y; y++) {
            for(int32_t x = SUN_X - 9 - (y - 45); x <= SUN_X + 9 - (y - 45); x++) {
                if((x + y * 2) % 4 == 0) canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

static void draw_bubble(Canvas* canvas, int32_t cx, int32_t bottom, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    int32_t bw = canvas_string_width(canvas, text) + 6;
    int32_t bx = cx - bw / 2;
    if(bx < ROOM_X + 1) bx = ROOM_X + 1;
    if(bx + bw > 127) bx = 127 - bw;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, bottom - 11, bw, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, bottom - 11, bw, 11, 2);
    canvas_draw_str(canvas, bx + 3, bottom - 2, text);
}

static void draw_cat(Canvas* canvas, const App* app, int i) {
    const Cat* cat = &app->cats[i];
    const Sprite* sprite;
    int32_t bob = 0;
    int32_t cx = cat->x;
    int32_t ground = FLOOR_Y;
    bool zzz = false;

    switch(cat->mode) {
    case CatSleep:
        sprite = cat_sleep[i];
        zzz = true;
        break;
    case CatSit:
        sprite = cat_sit[i];
        break;
    case CatSpot:
        sprite = cat_sit[i];
        if(cat->spot == SpotWindow) {
            ground = WINDOW_Y + WINDOW_H - 1;
        } else if(cat->spot == SpotTree) {
            sprite = cat_sleep[i];
            cx = TREE_SEAT_X;
            ground = TREE_TOP_Y;
            zzz = true;
        } else if(cat->spot == SpotSun) {
            sprite = cat_sleep[i];
            zzz = true;
        }
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

    // A sitting cat joins in with the new record.
    int32_t sway = 0;
    if(app->react_timer && cat->mode == CatSit && !cat->pet_timer) {
        switch(app->react_genre) {
        case GenreChill:
            sprite = cat_sleep[i]; // settles into a loaf
            break;
        case GenrePop:
            bob = -(int32_t)((app->tick / 3 + i) & 1);
            break;
        case GenreHipHop:
            bob = -(int32_t)((app->tick / 3) & 1);
            sway = ((app->tick / 6 + i) & 1) ? 1 : -1;
            break;
        case GenreHouse:
            sway = ((app->tick / 2 + i) & 1) ? 2 : -2;
            break;
        case GenreParty:
            bob = ((app->tick / 2 + i) & 1) ? -4 : 0;
            break;
        default:
            break;
        }
    }

    int32_t x = cx - sprite->w / 2 + sway;
    int32_t y = ground - sprite->h + 1 + bob;
    draw_sprite(canvas, x, y, sprite);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    if(zzz && !cat->pet_timer) {
        int32_t phase = (app->tick / 6) % 3;
        canvas_draw_str(canvas, cx + 6 + phase, y - 1 - phase * 2, "z");
    }
    if(cat->bubble) draw_bubble(canvas, cx, y - 1, cat->bubble_text ? cat->bubble_text : "meow!");
}

// Yulia's words: the Russian as she says it, with the English underneath.
static void draw_speech(Canvas* canvas, const App* app) {
    const Sprite* ru = ru_sprites[app->speech];
    const char* english = ru_english[app->speech];
    canvas_set_font(canvas, FontSecondary);
    int32_t w = canvas_string_width(canvas, english);
    if(ru->w > w) w = ru->w;
    w += 8;
    int32_t x = ROOM_X + 1;
    if(x + w > 127) x = 127 - w;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, 0, w, 23);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, 0, w, 23, 3);
    canvas_draw_line(canvas, x, 14, x - 3, 17); // tail, towards Yulia
    canvas_draw_line(canvas, x - 3, 17, x, 17);
    draw_sprite(canvas, x + 4, 12 - ru->h, ru);
    canvas_draw_str(canvas, x + 4, 21, english);
}

static void draw_scene(Canvas* canvas, const App* app) {
    draw_room(canvas, app);

    draw_sprite(
        canvas,
        BOWL_X,
        FLOOR_Y - (app->bowl_full ? 6 : 4),
        app->bowl_full ? &spr_bowl_full : &spr_bowl);

    // A treat waits in front of each cat that is on its way to one.
    for(int i = 0; i < CAT_COUNT; i++) {
        const Cat* cat = &app->cats[i];
        if(cat->mode == CatEat && cat->eat_treat) {
            draw_sprite(canvas, i == NUGGET ? 90 : 46, FLOOR_Y - 2, &spr_treat);
        }
    }

    for(int i = 0; i < CAT_COUNT; i++)
        draw_cat(canvas, app, i);

    if(app->toy == ToyYarn) {
        // The ball hops along the floor.
        static const int8_t hop[] = {0, 3, 5, 6, 5, 3};
        int32_t y = FLOOR_Y - 6 - hop[app->tick % 6];
        draw_sprite(canvas, app->toy_x - 3, y, &spr_yarn);
    } else if(app->toy == ToyLaser) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, app->toy_x, app->toy_y, 1);
        if(app->tick & 1) canvas_draw_circle(canvas, app->toy_x, app->toy_y, 3);
    }

    for(int i = 0; i < HEART_COUNT; i++) {
        const Heart* heart = &app->hearts[i];
        if(heart->life) draw_sprite(canvas, heart->x - 3, heart->y, &spr_heart);
    }

    if(music_playing(app)) {
        // Notes drift up from the shelf while music is playing.
        for(int i = 0; i < 2; i++) {
            int32_t phase = (app->tick / 2 + i * 8) % 16;
            draw_sprite(canvas, 82 + i * 7 + ((phase / 4) & 1), 22 - phase, &spr_note);
        }
    }

    if(app->art_timer) {
        // The finished drawing, held up for everyone to admire.
        draw_sprite(canvas, 60, 26, &spr_pad);
    }

    if(app->lights_off) {
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_box(canvas, ROOM_X, 0, 128 - ROOM_X, BAR_Y);
    }

    draw_yulia(canvas, app);

    if(app->speech_timer) {
        draw_speech(canvas, app);
    } else if(app->toast_timer && app->toast) {
        canvas_set_font(canvas, FontSecondary);
        int32_t w = canvas_string_width(canvas, app->toast);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 45, 0, w + 3, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 46, 8, app->toast);
    }
}

static const char* bar_label(const App* app) {
    const Group* group = &groups[app->group];
    if(!app->in_group) {
        if(group->first == ActNap && app->lights_off) return "Wake up";
        return group->name;
    }
    return act_names[group->first + app->sub[app->group]];
}

static void draw_bar_frame(Canvas* canvas) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, BAR_Y, 128, 64 - BAR_Y);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, BAR_Y, 127, BAR_Y);
    canvas_set_font(canvas, FontSecondary);
}

static void draw_bar(Canvas* canvas, const App* app) {
    draw_bar_frame(canvas);

    char buf[24];
    if(app->in_group) {
        // Inside a group: its name sits where the heart count was.
        snprintf(buf, sizeof(buf), "%s:", groups[app->group].name);
        canvas_draw_str(canvas, 2, 63, buf);
    } else {
        draw_sprite(canvas, 2, 57, &spr_heart);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->love);
        canvas_draw_str(canvas, 12, 63, buf);
    }

    canvas_draw_str(canvas, 46, 63, "<");
    canvas_draw_str(canvas, 122, 63, ">");
    canvas_draw_str_aligned(canvas, 86, 63, AlignCenter, AlignBottom, bar_label(app));
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

static void draw_wardrobe(Canvas* canvas, const App* app) {
    static const char* const labels[LOOK_ROWS] = {"Hair", "Color", "Glasses", "Sweater"};
    const Look* look = &app->look;
    const char* const values[LOOK_ROWS] = {
        hair_style_names[look->hair],
        hair_color_names[look->color],
        glasses_names[look->glasses],
        sweater_names[look->sweater],
    };

    draw_yulia(canvas, app);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 48, 10, "Wardrobe");
    canvas_draw_line(canvas, 0, BAR_Y, 44, BAR_Y);

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < LOOK_ROWS; i++) {
        int32_t y = 24 + i * 12;
        canvas_draw_str(canvas, 48, y, labels[i]);
        canvas_draw_str_aligned(canvas, 106, y, AlignCenter, AlignBottom, values[i]);
        if(i == app->row) {
            canvas_draw_str(canvas, 86, y, "<");
            canvas_draw_str(canvas, 123, y, ">");
            canvas_draw_line(canvas, 48, y + 2, 80, y + 2);
        }
    }
}

static void draw_vinyl(Canvas* canvas, const App* app) {
    static const char* const labels[VINYL_ROWS] = {"Music", "Volume", "Genre"};
    const Sound* sound = &app->sound;

    // The record: it turns while the music is on.
    const int32_t cx = 22, cy = 32;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, cx, cy, 20);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_circle(canvas, cx, cy, 16);
    canvas_draw_circle(canvas, cx, cy, 12);
    canvas_draw_disc(canvas, cx, cy, 7);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, cx, cy, 1);
    static const int8_t spoke[8][2] = {
        {5, 0}, {4, 4}, {0, 5}, {-4, 4}, {-5, 0}, {-4, -4}, {0, -5}, {4, -4}};
    uint32_t turn = sound->on ? (app->tick / 2) % 8 : 0;
    canvas_draw_dot(canvas, cx + spoke[turn][0], cy + spoke[turn][1]);
    canvas_draw_dot(canvas, cx - spoke[turn][0], cy - spoke[turn][1]);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 50, 12, "Vinyl");

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < VINYL_ROWS; i++) {
        int32_t y = 27 + i * 13;
        canvas_draw_str(canvas, 50, y, labels[i]);
        if(i == 1) {
            for(int32_t v = 0; v < VOLUME_MAX; v++) {
                int32_t h = 2 + v;
                if(v < sound->volume) {
                    canvas_draw_box(canvas, 95 + v * 5, y - h, 3, h);
                } else {
                    canvas_draw_dot(canvas, 96 + v * 5, y - 1);
                }
            }
        } else {
            const char* value = i == 0 ? (sound->on ? "On" : "Off") : genres[sound->genre].name;
            canvas_draw_str_aligned(canvas, 106, y, AlignCenter, AlignBottom, value);
        }
        if(i == app->row) {
            canvas_draw_str(canvas, 84, y, "<");
            canvas_draw_str(canvas, 124, y, ">");
            canvas_draw_line(canvas, 50, y + 2, 80, y + 2);
        }
    }
}

// Decorating happens in the room itself, with the choice shown in the bar.
static void draw_decor(Canvas* canvas, const App* app) {
    static const char* const labels[DECOR_ROWS] = {"Rug", "Plant", "Cat tree", "Lights"};
    const Decor* decor = &app->decor;
    const char* const values[DECOR_ROWS] = {
        rug_names[decor->rug],
        plant_names[decor->plant],
        tree_names[decor->tree],
        lights_names[decor->lights],
    };

    draw_scene(canvas, app);
    draw_bar_frame(canvas);
    // Up and down arrows: there are more rows.
    canvas_draw_line(canvas, 2, 58, 4, 56);
    canvas_draw_line(canvas, 4, 56, 6, 58);
    canvas_draw_line(canvas, 2, 60, 4, 62);
    canvas_draw_line(canvas, 4, 62, 6, 60);
    canvas_draw_str(canvas, 10, 63, labels[app->row]);
    canvas_draw_str(canvas, 56, 63, "<");
    canvas_draw_str(canvas, 122, 63, ">");
    canvas_draw_str_aligned(canvas, 91, 63, AlignCenter, AlignBottom, values[app->row]);
}

// The picture for one album page, inside the frame at (fx, fy).
static void draw_photo(Canvas* canvas, const App* app, Photo photo, int32_t fx, int32_t fy) {
    const int32_t ground = fy + 34;
    const Sprite* nugget = &spr_nugget_sit;
    const Sprite* baby = &spr_baby_sit;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    switch(photo) {
    case PhotoFirstCuddle:
        draw_standing(canvas, fx + 27, ground, nugget);
        draw_sprite(canvas, fx + 24, fy + 6, &spr_heart);
        break;
    case PhotoNap:
        draw_standing(canvas, fx + 14, ground, &spr_nugget_sleep);
        draw_standing(canvas, fx + 39, ground, &spr_baby_sleep);
        canvas_draw_str(canvas, fx + 18, fy + 20, "z");
        canvas_draw_str(canvas, fx + 44, fy + 17, "z");
        canvas_draw_str(canvas, fx + 49, fy + 12, "z");
        break;
    case PhotoDinner:
        draw_standing(canvas, fx + 27, ground, &spr_bowl_full);
        draw_standing(canvas, fx + 10, ground, &spr_nugget_walk_a);
        draw_standing(canvas, fx + 42, ground, &spr_baby_walk_a_l);
        break;
    case PhotoTreats:
        draw_standing(canvas, fx + 22, ground, baby);
        draw_sprite(canvas, fx + 40, ground - 3, &spr_treat);
        canvas_draw_str(canvas, fx + 38, fy + 16, "!!");
        break;
    case PhotoDance:
        draw_standing(canvas, fx + 14, ground - 5, nugget);
        draw_standing(canvas, fx + 39, ground, baby);
        draw_sprite(canvas, fx + 24, fy + 4, &spr_note);
        draw_sprite(canvas, fx + 46, fy + 6, &spr_note);
        break;
    case PhotoZoomies:
        draw_standing(canvas, fx + 36, ground, &spr_baby_walk_b);
        for(int32_t k = 0; k < 3; k++)
            canvas_draw_line(
                canvas, fx + 3 + k * 2, ground - 2 - k * 4, fx + 16, ground - 2 - k * 4);
        break;
    case PhotoBirds:
        canvas_draw_frame(canvas, fx + 22, fy + 3, 28, 22);
        canvas_draw_line(canvas, fx + 36, fy + 3, fx + 36, fy + 24);
        canvas_draw_line(canvas, fx + 20, fy + 25, fx + 51, fy + 25);
        canvas_draw_line(canvas, fx + 40, fy + 9, fx + 42, fy + 11);
        canvas_draw_line(canvas, fx + 42, fy + 11, fx + 44, fy + 9);
        canvas_draw_line(canvas, fx + 26, fy + 13, fx + 28, fy + 15);
        canvas_draw_line(canvas, fx + 28, fy + 15, fx + 30, fy + 13);
        draw_standing(canvas, fx + 31, fy + 24, nugget);
        break;
    case PhotoFifty:
        draw_standing(canvas, fx + 14, ground, nugget);
        draw_standing(canvas, fx + 39, ground, baby);
        draw_sprite(canvas, fx + 10, fy + 8, &spr_heart);
        draw_sprite(canvas, fx + 24, fy + 3, &spr_heart);
        draw_sprite(canvas, fx + 38, fy + 8, &spr_heart);
        break;
    case PhotoWeek:
        draw_standing(canvas, fx + 14, ground, nugget);
        draw_standing(canvas, fx + 39, ground, baby);
        canvas_draw_str_aligned(canvas, fx + 27, fy + 12, AlignCenter, AlignBottom, "7 days");
        break;
    case PhotoPiggy: {
        const Sprite* word = ru_sprites[RU_PIGGY_SHORT];
        draw_standing(canvas, fx + 18, ground, baby);
        canvas_draw_rframe(canvas, fx + 8, fy + 2, word->w + 6, 14, 2);
        draw_sprite(canvas, fx + 11, fy + 5, word);
        break;
    }
    case PhotoArtist:
        draw_sprite(canvas, fx + 20, fy + 8, &spr_pad);
        canvas_draw_line(canvas, fx + 23, fy + 18, fx + 18, ground);
        canvas_draw_line(canvas, fx + 31, fy + 18, fx + 36, ground);
        canvas_draw_line(canvas, fx + 27, fy + 18, fx + 27, ground);
        break;
    case PhotoBumped:
        draw_standing(canvas, fx + 13, ground, baby);
        draw_standing(canvas, fx + 30, ground, &spr_bowl);
        draw_standing(canvas, fx + 44, ground, &spr_nugget_walk_a);
        canvas_draw_str(canvas, fx + 44, fy + 20, "!");
        break;
    default:
        break;
    }
    UNUSED(app);
}

static uint32_t photo_count(const App* app) {
    uint32_t n = 0;
    for(int i = 0; i < PhotoCount; i++)
        if(app->photos & (1u << i)) n++;
    return n;
}

static void draw_album(Canvas* canvas, const App* app) {
    const Photo photo = app->album_page;
    const PhotoInfo* info = &photo_info[photo];
    const bool have = app->photos & (1u << photo);
    const int32_t fx = 3, fy = 3;

    // A print with a white border, like an instant photo.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, fx - 2, fy - 2, 59, 46, 2);
    canvas_draw_frame(canvas, fx, fy, 55, 37);
    if(have) {
        draw_photo(canvas, app, photo, fx, fy);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, fx + 21, fy + 9, 13, 19);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, fx + 27, fy + 23, AlignCenter, AlignBottom, "?");
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 64, 11, "Album");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 64, 24, have ? info->title : "Not yet...");

    char buf[24];
    snprintf(buf, sizeof(buf), "< %u/%u >", (unsigned)photo + 1, (unsigned)PhotoCount);
    canvas_draw_str(canvas, 64, 36, buf);
    snprintf(buf, sizeof(buf), "%lu found", (unsigned long)photo_count(app));
    canvas_draw_str(canvas, 64, 46, buf);

    canvas_draw_line(canvas, 0, 50, 127, 50);
    canvas_draw_str(canvas, 2, 61, have ? info->caption : info->hint);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(app->screen) {
    case ScreenStats:
        draw_stats(canvas, app);
        break;
    case ScreenWardrobe:
        draw_wardrobe(canvas, app);
        break;
    case ScreenVinyl:
        draw_vinyl(canvas, app);
        break;
    case ScreenDecor:
        draw_decor(canvas, app);
        break;
    case ScreenAlbum:
        draw_album(canvas, app);
        break;
    default:
        draw_scene(canvas, app);
        draw_bar(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

// Up/Down moves between rows; returns true if the key was a row change.
static bool row_input(App* app, InputKey key, uint8_t rows) {
    if(key == InputKeyUp) {
        app->row = (app->row + rows - 1) % rows;
        return true;
    }
    if(key == InputKeyDown) {
        app->row = (app->row + 1) % rows;
        return true;
    }
    return false;
}

static void cycle(uint8_t* value, uint8_t count, InputKey key) {
    *value = (*value + (key == InputKeyRight ? 1 : count - 1)) % count;
}

static void wardrobe_input(App* app, InputKey key) {
    Look* look = &app->look;
    uint8_t* const fields[LOOK_ROWS] = {&look->hair, &look->color, &look->glasses, &look->sweater};
    static const uint8_t counts[LOOK_ROWS] = {
        HAIR_STYLE_COUNT, HAIR_COLOR_COUNT, GLASSES_COUNT, SWEATER_COUNT};

    if(row_input(app, key, LOOK_ROWS)) return;
    if(key == InputKeyLeft || key == InputKeyRight) {
        cycle(fields[app->row], counts[app->row], key);
    } else if(key == InputKeyOk || key == InputKeyBack) {
        app->screen = ScreenRoom;
        app->happy_timer = 20;
    }
}

static void decor_input(App* app, InputKey key) {
    Decor* decor = &app->decor;
    uint8_t* const fields[DECOR_ROWS] = {&decor->rug, &decor->plant, &decor->tree, &decor->lights};
    static const uint8_t counts[DECOR_ROWS] = {RUG_COUNT, PLANT_COUNT, 2, 2};

    if(row_input(app, key, DECOR_ROWS)) return;
    if(key == InputKeyLeft || key == InputKeyRight) {
        cycle(fields[app->row], counts[app->row], key);
        // A cat napping on a tree that has just been put away hops down.
        if(!decor->tree) {
            for(int i = 0; i < CAT_COUNT; i++) {
                Cat* cat = &app->cats[i];
                if(cat->spot == SpotTree) cat_rest(cat);
            }
        }
    } else if(key == InputKeyOk || key == InputKeyBack) {
        app->screen = ScreenRoom;
        app->happy_timer = 20;
    }
}

static void vinyl_input(App* app, InputKey key) {
    Sound* sound = &app->sound;

    if(row_input(app, key, VINYL_ROWS)) return;
    if(key == InputKeyLeft || key == InputKeyRight) {
        if(app->row == 0) {
            sound->on = !sound->on;
        } else if(app->row == 1) {
            int volume = sound->volume + (key == InputKeyRight ? 1 : -1);
            sound->volume = volume < 1 ? 1 : volume > VOLUME_MAX ? VOLUME_MAX : volume;
        } else {
            cycle(&sound->genre, GenreCount, key);
            music_restart(app);
        }
    } else if(key == InputKeyOk || key == InputKeyBack) {
        app->screen = ScreenRoom;
        // A different record, or the player just switched on: time to react.
        if(sound->on && (!app->sound_before.on || app->sound_before.genre != sound->genre)) {
            lights_on(app);
            react_start(app);
        }
    }
}

static void album_input(App* app, InputKey key) {
    if(key == InputKeyLeft || key == InputKeyRight) {
        cycle(&app->album_page, PhotoCount, key);
    } else if(key == InputKeyOk || key == InputKeyBack) {
        app->screen = ScreenRoom;
    }
}

// Returns false when the app should quit.
static bool room_input(App* app, const InputEvent* event) {
    const Group* group = &groups[app->group];
    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight:
        if(app->in_group) {
            cycle(&app->sub[app->group], group->count, event->key);
        } else {
            cycle(&app->group, GROUP_COUNT, event->key);
        }
        break;
    case InputKeyOk:
        if(event->type != InputTypeShort) break;
        if(!app->in_group && group->count > 1) {
            app->in_group = true;
        } else {
            do_action(app, group->first + (app->in_group ? app->sub[app->group] : 0));
        }
        break;
    case InputKeyUp:
    case InputKeyDown:
        app->screen = ScreenStats;
        break;
    case InputKeyBack:
        if(!app->in_group) return false;
        app->in_group = false;
        break;
    default:
        break;
    }
    return true;
}

static bool handle_input(App* app, const InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    switch(app->screen) {
    case ScreenStats:
        app->screen = ScreenRoom;
        break;
    case ScreenWardrobe:
        wardrobe_input(app, event->key);
        break;
    case ScreenVinyl:
        vinyl_input(app, event->key);
        break;
    case ScreenDecor:
        decor_input(app, event->key);
        break;
    case ScreenAlbum:
        album_input(app, event->key);
        break;
    default:
        return room_input(app, event);
    }
    return true;
}

int32_t yulia_cats_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    game_load(app);
    app->phase = phase_now();
    for(int i = 0; i < CAT_COUNT; i++) {
        Cat* cat = &app->cats[i];
        cat->x = i == NUGGET ? 68 : 104;
        cat->dir = i == NUGGET ? 1 : -1;
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
        // Sleep until the next game tick or the next note, whichever is first.
        uint32_t now = furi_get_tick();
        uint32_t deadline = next_tick;
        if(app->music_held && (int32_t)(app->music_next - deadline) < 0) {
            deadline = app->music_next;
        }
        uint32_t wait = (int32_t)(deadline - now) > 0 ? deadline - now : 0;

        bool redraw = false;
        InputEvent event;
        FuriStatus status = furi_message_queue_get(queue, &event, wait);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(status == FuriStatusOk) {
            running = handle_input(app, &event);
            redraw = true;
        }
        now = furi_get_tick();
        if((int32_t)(now - next_tick) >= 0) {
            game_tick(app);
            next_tick += furi_ms_to_ticks(TICK_MS);
            redraw = true;
        }
        music_update(app, now);
        furi_mutex_release(app->mutex);
        if(redraw) view_port_update(view_port);
    }

    music_stop(app);
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
