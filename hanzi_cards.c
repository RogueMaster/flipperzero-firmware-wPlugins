// Hanzi Cards - Mandarin flashcards for Flipper Zero.
//
// Shows simplified or traditional characters from a deck file on the SD card, with pinyin
// (drawn with real tone marks), English, and the tone contours played on the
// speaker. Cards are scheduled over days with a small Leitner box system, and
// can be answered by flipping them or by picking each syllable's tone.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#define GLYPH_SIZE    32
#define GLYPH_BYTES   (GLYPH_SIZE * GLYPH_SIZE / 8)
#define GLYPH_GAP     2
#define MAX_HANZI     4
#define TEXT_LEN      24
#define MAX_CARDS     640
#define MAX_SYLLABLES (MAX_HANZI + 1)
#define NO_CARD       0xFFFF

#define DECK_PATH_FORMAT APP_ASSETS_PATH("%s.deck")
#define SAVE_DIR         EXT_PATH("apps_data/hanzi_cards")
#define SAVE_PATH_FORMAT SAVE_DIR "/%s.sav"
#define SETTINGS_PATH    SAVE_DIR "/settings.bin"
#define PATH_LEN         64

typedef struct {
    const char* name;
    const char* file;
} DeckInfo;

static const DeckInfo decks[] = {
    {"HSK 1", "hsk1"},
    {"HSK 2", "hsk2"},
    {"HSK 3", "hsk3"},
    {"HSK 4", "hsk4"},
    {"Numbers", "numbers"},
    {"Food", "food"},
    {"Travel", "travel"},
    {"Measures", "measures"},
};

// Leitner boxes: 0 is a card never seen, 1 is a card still being learned today
#define BOX_NEW      0
#define BOX_LEARNING 1
#define BOX_KNOWN    3
#define BOX_MAX      7
// Days until a card in each box comes round again
static const uint8_t box_days[BOX_MAX + 1] = {0, 0, 1, 3, 7, 14, 30, 60};
// New cards are only introduced while fewer than this many are being learned
#define LEARNING_TARGET 6
#define SAVE_EVERY      10
// "Keep going" on the finished screen allows this many more new cards today
#define BONUS_NEW       10

static const uint8_t new_per_day[] = {5, 10, 20, 50};
#define NEW_PER_DAY_DEFAULT 1

// Screen layout (baselines)
#define PINYIN_Y     45
#define ENGLISH_Y    54
#define HINT_Y       63
// x-height of FontPrimary, which the tone marks sit above
#define PINYIN_XH    6
#define SYLLABLE_GAP 3

#define TONE_GAP_MS 70
#define TONE_VOLUME 0.6f

typedef struct {
    char magic[4];
    uint16_t cards;
    uint16_t glyphs;
    uint8_t glyph_size;
    uint8_t pad[3];
} DeckHeader;

typedef struct {
    uint16_t simplified[MAX_HANZI];
    uint16_t traditional[MAX_HANZI];
    char pinyin[TEXT_LEN];
    char english[TEXT_LEN];
} CardRecord;

// "HZS1" is followed by one box per card. "HZS2" is followed by a SaveDay,
// one box per card, then one due day per card.
typedef struct {
    char magic[4];
    uint8_t reserved[2];
    uint16_t count;
} SaveHeader;

typedef struct {
    uint16_t new_day;
    uint16_t new_today;
} SaveDay;

// The first version's settings are the first eight bytes of this
typedef struct {
    char magic[4];
    uint8_t deck;
    uint8_t english_front;
    uint8_t sound;
    uint8_t traditional;
    uint8_t tone_quiz;
    uint8_t new_limit;
    uint16_t stat_day;
    uint16_t reviewed;
    uint16_t streak;
    uint16_t streak_day;
} Settings;
#define SETTINGS_V1_SIZE 8

typedef struct {
    uint8_t tones[MAX_HANZI + 1];
    uint8_t count;
    uint8_t index;
    uint32_t started;
    bool active;
    bool speaker;
} TonePlayer;

typedef enum {
    MenuDeck,
    MenuMode,
    MenuScript,
    MenuFront,
    MenuSound,
    MenuNewLimit,
    MenuStats,
    MenuBrowse,
    MenuReset,
    MenuCount,
} MenuItem;
#define MENU_VISIBLE 5

typedef enum {
    ScreenCards,
    ScreenStats,
    ScreenBrowse,
} Screen;

typedef struct {
    FuriMutex* mutex;
    Storage* storage;
    File* deck;
    const char* error;

    uint16_t card_count;
    uint16_t glyph_count;
    uint8_t box[MAX_CARDS];
    // Day number each seen card is next due
    uint16_t due[MAX_CARDS];
    uint16_t new_day;
    uint16_t new_today;
    uint8_t bonus_new;
    // Nothing is due and no new cards are allowed: the day's work is done
    bool done;
    // Reviewing ahead of schedule after finishing
    bool ahead;

    // Reviews today and the run of days with at least one, across all decks
    uint16_t stat_day;
    uint16_t reviewed;
    uint16_t streak;
    uint16_t streak_day;

    uint16_t current;
    CardRecord card;
    uint8_t hanzi_count;
    uint8_t bitmaps[MAX_HANZI][GLYPH_BYTES];
    bool revealed;
    uint8_t unsaved;

    // The card's syllables, and the tones picked for them in the tone quiz
    uint8_t syllables;
    uint8_t tones[MAX_SYLLABLES];
    uint8_t guesses[MAX_SYLLABLES];
    uint8_t guessed;

    uint8_t deck_index;
    bool english_front;
    bool sound;
    bool traditional;
    bool tone_quiz;
    uint8_t new_limit;

    Screen screen;
    uint16_t study_card;

    bool menu_open;
    uint8_t menu_item;
    uint8_t menu_top;
    bool reset_armed;

    TonePlayer player;
} App;

// ---------------------------------------------------------------- deck

static bool deck_open(App* app) {
    char path[PATH_LEN];
    snprintf(path, sizeof(path), DECK_PATH_FORMAT, decks[app->deck_index].file);
    storage_file_close(app->deck);
    app->card_count = 0;
    if(!storage_file_open(app->deck, path, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    DeckHeader header;
    if(storage_file_read(app->deck, &header, sizeof(header)) != sizeof(header)) return false;
    if(memcmp(header.magic, "HZD2", 4) != 0 || header.glyph_size != GLYPH_SIZE) return false;
    if(header.cards == 0 || header.cards > MAX_CARDS) return false;
    app->card_count = header.cards;
    app->glyph_count = header.glyphs;
    return true;
}

// Reads the bitmaps for the current card in the chosen script
static void glyphs_load(App* app) {
    app->hanzi_count = 0;
    const uint16_t* glyphs = app->traditional ? app->card.traditional : app->card.simplified;
    uint32_t glyph_base = sizeof(DeckHeader) + (uint32_t)app->card_count * sizeof(CardRecord);
    for(uint8_t i = 0; i < MAX_HANZI; i++) {
        uint16_t glyph = glyphs[i];
        if(glyph >= app->glyph_count) break;
        if(!storage_file_seek(app->deck, glyph_base + (uint32_t)glyph * GLYPH_BYTES, true)) break;
        if(storage_file_read(app->deck, app->bitmaps[i], GLYPH_BYTES) != GLYPH_BYTES) break;
        app->hanzi_count++;
    }
}

static void card_load(App* app, uint16_t index) {
    app->current = index;
    app->revealed = false;
    app->hanzi_count = 0;
    memset(&app->card, 0, sizeof(app->card));

    uint32_t offset = sizeof(DeckHeader) + (uint32_t)index * sizeof(CardRecord);
    if(!storage_file_seek(app->deck, offset, true)) return;
    if(storage_file_read(app->deck, &app->card, sizeof(CardRecord)) != sizeof(CardRecord)) return;
    app->card.pinyin[TEXT_LEN - 1] = '\0';
    app->card.english[TEXT_LEN - 1] = '\0';
    glyphs_load(app);

    // One tone per syllable; 0 is the neutral tone
    app->syllables = 0;
    app->guessed = 0;
    for(const char* s = app->card.pinyin; *s && app->syllables < MAX_SYLLABLES;) {
        while(*s >= 'a' && *s <= 'z')
            s++;
        uint8_t tone = 0;
        if(*s >= '1' && *s <= '4') tone = *s++ - '0';
        app->tones[app->syllables++] = tone;
        while(*s && !(*s >= 'a' && *s <= 'z'))
            s++;
    }
}

static uint16_t today(void) {
    return (uint16_t)(furi_hal_rtc_get_timestamp() / 86400);
}

// ---------------------------------------------------------------- progress

static void settings_load(App* app) {
    app->sound = true;
    app->new_limit = NEW_PER_DAY_DEFAULT;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Settings settings = {0};
        size_t size = storage_file_read(file, &settings, sizeof(settings));
        bool v1 = size == SETTINGS_V1_SIZE && memcmp(settings.magic, "HZC1", 4) == 0;
        bool v2 = size == sizeof(settings) && memcmp(settings.magic, "HZC2", 4) == 0;
        if(v1 || v2) {
            if(settings.deck < COUNT_OF(decks)) app->deck_index = settings.deck;
            app->english_front = settings.english_front;
            app->sound = settings.sound;
            app->traditional = settings.traditional;
        }
        if(v2) {
            app->tone_quiz = settings.tone_quiz;
            if(settings.new_limit < COUNT_OF(new_per_day)) app->new_limit = settings.new_limit;
            app->stat_day = settings.stat_day;
            app->reviewed = settings.reviewed;
            app->streak = settings.streak;
            app->streak_day = settings.streak_day;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void progress_load(App* app) {
    char path[PATH_LEN];
    snprintf(path, sizeof(path), SAVE_PATH_FORMAT, decks[app->deck_index].file);
    memset(app->box, BOX_NEW, sizeof(app->box));
    memset(app->due, 0, sizeof(app->due));
    app->new_day = 0;
    app->new_today = 0;
    app->bonus_new = 0;
    app->ahead = false;

    uint16_t now = today();
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveHeader header;
        bool ok = storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
                  header.count == app->card_count;
        if(ok && memcmp(header.magic, "HZS2", 4) == 0) {
            SaveDay day;
            ok = storage_file_read(file, &day, sizeof(day)) == sizeof(day) &&
                 storage_file_read(file, app->box, app->card_count) == app->card_count &&
                 storage_file_read(file, app->due, app->card_count * sizeof(uint16_t)) ==
                     app->card_count * sizeof(uint16_t);
            if(ok) {
                app->new_day = day.new_day;
                app->new_today = day.new_today;
            } else {
                memset(app->box, BOX_NEW, sizeof(app->box));
            }
        } else if(ok && memcmp(header.magic, "HZS1", 4) == 0) {
            // The first version kept no dates. Spread what was learned over each
            // box's interval so that it does not all fall due on the same day.
            storage_file_read(file, app->box, app->card_count);
            for(uint16_t i = 0; i < app->card_count; i++) {
                if(app->box[i] > BOX_MAX) app->box[i] = BOX_MAX;
                app->due[i] = now + furi_hal_random_get() % (box_days[app->box[i]] + 1);
            }
        }
        for(uint16_t i = 0; i < app->card_count; i++) {
            if(app->box[i] > BOX_MAX) app->box[i] = BOX_MAX;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

// Saves the settings and, if a deck is loaded, its progress
static void progress_save(App* app) {
    storage_simply_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(app->storage, SAVE_DIR);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        Settings settings = {
            .magic = {'H', 'Z', 'C', '2'},
            .deck = app->deck_index,
            .english_front = app->english_front,
            .sound = app->sound,
            .traditional = app->traditional,
            .tone_quiz = app->tone_quiz,
            .new_limit = app->new_limit,
            .stat_day = app->stat_day,
            .reviewed = app->reviewed,
            .streak = app->streak,
            .streak_day = app->streak_day,
        };
        storage_file_write(file, &settings, sizeof(settings));
    }
    storage_file_close(file);

    char path[PATH_LEN];
    snprintf(path, sizeof(path), SAVE_PATH_FORMAT, decks[app->deck_index].file);
    if(app->card_count > 0 && storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SaveHeader header = {
            .magic = {'H', 'Z', 'S', '2'},
            .count = app->card_count,
        };
        SaveDay day = {.new_day = app->new_day, .new_today = app->new_today};
        storage_file_write(file, &header, sizeof(header));
        storage_file_write(file, &day, sizeof(day));
        storage_file_write(file, app->box, app->card_count);
        storage_file_write(file, app->due, app->card_count * sizeof(uint16_t));
    }
    storage_file_close(file);
    storage_file_free(file);
    app->unsaved = 0;
}

static void deal(App* app, bool avoid_current);

// Opens the selected deck and deals its first card
static void deck_start(App* app) {
    app->screen = ScreenCards;
    if(deck_open(app)) {
        app->error = NULL;
        progress_load(app);
        deal(app, false);
    } else {
        app->error = "Deck file not found";
    }
}

typedef struct {
    uint16_t fresh; // never seen
    uint16_t learning; // missed or just met, still coming round today
    uint16_t young; // seen, with a gap of a day
    uint16_t known; // a gap of three days or more
    uint16_t due; // seen cards to review today
    uint16_t tomorrow; // seen cards that fall due tomorrow
} Counts;

static Counts count_cards(const App* app) {
    Counts c = {0};
    uint16_t now = today();
    for(uint16_t i = 0; i < app->card_count; i++) {
        uint8_t box = app->box[i];
        if(box == BOX_NEW) {
            c.fresh++;
            continue;
        }
        if(box >= BOX_KNOWN) {
            c.known++;
        } else if(box == BOX_LEARNING) {
            c.learning++;
        } else {
            c.young++;
        }
        if(app->due[i] <= now) {
            c.due++;
        } else if(app->due[i] == now + 1) {
            c.tomorrow++;
        }
    }
    return c;
}

// ---------------------------------------------------------------- scheduling

// Among the cards that are due, lower boxes come up more often
static const uint8_t box_weight[BOX_MAX + 1] = {0, 16, 8, 4, 2, 1, 1, 1};

// The next card to show, or NO_CARD when the day's work is done
static uint16_t pick_next(App* app, bool avoid_current) {
    uint16_t now = today();
    if(app->new_day != now) {
        app->new_day = now;
        app->new_today = 0;
        app->bonus_new = 0;
        app->ahead = false;
    }

    uint16_t learning = 0;
    uint16_t due = 0;
    uint16_t first_new = app->card_count;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(app->box[i] == BOX_NEW) {
            if(first_new == app->card_count) first_new = i;
            continue;
        }
        if(app->box[i] == BOX_LEARNING) learning++;
        if(app->ahead || app->due[i] <= now) due++;
    }

    bool new_allowed = first_new < app->card_count &&
                       app->new_today < new_per_day[app->new_limit] + app->bonus_new;
    if(new_allowed && learning < LEARNING_TARGET) return first_new;

    bool skip_current = avoid_current && due > 1;
    uint32_t total = 0;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(app->box[i] == BOX_NEW || (skip_current && i == app->current)) continue;
        if(app->ahead || app->due[i] <= now) total += box_weight[app->box[i]];
    }
    if(total > 0) {
        uint32_t roll = furi_hal_random_get() % total;
        for(uint16_t i = 0; i < app->card_count; i++) {
            if(app->box[i] == BOX_NEW || (skip_current && i == app->current)) continue;
            if(!app->ahead && app->due[i] > now) continue;
            uint8_t weight = box_weight[app->box[i]];
            if(roll < weight) return i;
            roll -= weight;
        }
    }
    return new_allowed ? first_new : NO_CARD;
}

// Shows the next card, or the finished screen if there is none
static void deal(App* app, bool avoid_current) {
    uint16_t next = pick_next(app, avoid_current);
    app->done = next == NO_CARD;
    if(!app->done) card_load(app, next);
}

// From the finished screen: more new cards if there are any, otherwise review early
static void keep_going(App* app) {
    bool has_new = false;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(app->box[i] == BOX_NEW) has_new = true;
    }
    if(has_new) {
        app->bonus_new += BONUS_NEW;
    } else {
        app->ahead = true;
    }
    deal(app, false);
}

static void rate(App* app, bool good) {
    uint16_t now = today();
    uint8_t* box = &app->box[app->current];
    if(*box == BOX_NEW) app->new_today++;
    if(!good) {
        *box = BOX_LEARNING;
    } else if(*box == BOX_NEW) {
        // Already knew it the first time round
        *box = BOX_KNOWN;
    } else if(*box < BOX_MAX) {
        (*box)++;
    }
    app->due[app->current] = now + box_days[*box];

    if(app->stat_day != now) {
        app->stat_day = now;
        app->reviewed = 0;
    }
    app->reviewed++;
    if(app->streak_day != now) {
        app->streak = app->streak_day + 1 == now ? app->streak + 1 : 1;
        app->streak_day = now;
    }

    if(++app->unsaved >= SAVE_EVERY) progress_save(app);
    deal(app, true);
}

// ---------------------------------------------------------------- tones

// Pitch contours on the usual 1 (low) to 5 (high) scale
static const float level_freq[5] = {400.0f, 476.0f, 566.0f, 673.0f, 800.0f};
static const uint16_t tone_ms[5] = {140, 260, 280, 380, 220};

static float tone_level(uint8_t tone, float t) {
    switch(tone) {
    case 1:
        return 5.0f;
    case 2:
        return 3.0f + 2.0f * t;
    case 3:
        return t < 0.4f ? 2.0f - t / 0.4f : 1.0f + 3.0f * (t - 0.4f) / 0.6f;
    case 4:
        return 5.0f - 4.0f * t;
    default:
        return 3.0f - 0.5f * t;
    }
}

static float level_to_freq(float level) {
    level = CLAMP(level, 5.0f, 1.0f) - 1.0f;
    int low = (int)level;
    if(low >= 4) return level_freq[4];
    float frac = level - (float)low;
    return level_freq[low] + (level_freq[low + 1] - level_freq[low]) * frac;
}

static void tones_stop(App* app) {
    TonePlayer* player = &app->player;
    if(player->speaker) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        player->speaker = false;
    }
    player->active = false;
}

static void tones_play(App* app) {
    TonePlayer* player = &app->player;
    tones_stop(app);
    if(!app->sound || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;

    player->count = app->syllables;
    memcpy(player->tones, app->tones, sizeof(player->tones));
    if(player->count == 0 || !furi_hal_speaker_acquire(30)) return;

    player->speaker = true;
    player->active = true;
    player->index = 0;
    player->started = furi_get_tick();
}

static void tones_update(App* app) {
    TonePlayer* player = &app->player;
    if(!player->active) return;

    uint32_t now = furi_get_tick();
    uint32_t elapsed = now - player->started;
    uint32_t length = tone_ms[player->tones[player->index]];
    if(elapsed >= length + TONE_GAP_MS) {
        if(++player->index >= player->count) {
            tones_stop(app);
            return;
        }
        player->started = now;
        elapsed = 0;
        length = tone_ms[player->tones[player->index]];
    }
    if(elapsed >= length) {
        furi_hal_speaker_stop();
        return;
    }
    float level = tone_level(player->tones[player->index], (float)elapsed / (float)length);
    furi_hal_speaker_start(level_to_freq(level), TONE_VOLUME);
}

// ---------------------------------------------------------------- drawing

// Index of the letter that carries the tone mark: a or e if present, the o
// of "ou", otherwise the last vowel
static int tone_vowel(const char* s, int len) {
    for(int i = 0; i < len; i++) {
        if(s[i] == 'a' || s[i] == 'e') return i;
    }
    for(int i = 0; i + 1 < len; i++) {
        if(s[i] == 'o' && s[i + 1] == 'u') return i;
    }
    int last = -1;
    for(int i = 0; i < len; i++) {
        if(s[i] == 'i' || s[i] == 'o' || s[i] == 'u' || s[i] == 'v') last = i;
    }
    return last;
}

// A 5x3 tone mark centred on cx with its top row at y
static void draw_tone_mark(Canvas* canvas, int cx, int y, int tone) {
    switch(tone) {
    case 1:
        canvas_draw_line(canvas, cx - 2, y + 1, cx + 2, y + 1);
        break;
    case 2:
        canvas_draw_line(canvas, cx - 1, y + 2, cx + 1, y);
        break;
    case 3:
        canvas_draw_line(canvas, cx - 2, y, cx, y + 2);
        canvas_draw_line(canvas, cx, y + 2, cx + 2, y);
        break;
    case 4:
        canvas_draw_line(canvas, cx - 1, y, cx + 1, y + 2);
        break;
    }
}

// Draws (or just measures) numbered pinyin such as "nv3 er2" as marked
// pinyin. Returns the width in pixels. If spans is given it receives the left
// and right edge of each syllable.
static int
    pinyin_layout(Canvas* canvas, const char* s, int x, int y, bool draw, int16_t spans[][2]) {
    int start = x;
    int syllable = 0;
    while(*s) {
        int len = 0;
        while(s[len] >= 'a' && s[len] <= 'z')
            len++;
        int tone = (s[len] >= '1' && s[len] <= '4') ? s[len] - '0' : 0;
        int mark = tone ? tone_vowel(s, len) : -1;
        int syllable_x = x;

        for(int i = 0; i < len; i++) {
            char c = s[i] == 'v' ? 'u' : s[i];
            int width = canvas_glyph_width(canvas, c);
            if(draw) {
                canvas_draw_glyph(canvas, x, y, c);
                int cx = x + (width - 2) / 2;
                int mark_y = y - PINYIN_XH - 4;
                if(s[i] == 'v') {
                    canvas_draw_dot(canvas, cx - 1, y - PINYIN_XH - 2);
                    canvas_draw_dot(canvas, cx + 1, y - PINYIN_XH - 2);
                    mark_y -= 2;
                }
                if(i == mark) {
                    if(c == 'i') {
                        // The mark replaces the dot
                        canvas_set_color(canvas, ColorWhite);
                        canvas_draw_box(canvas, x, y - PINYIN_XH - 4, width, 4);
                        canvas_set_color(canvas, ColorBlack);
                    }
                    draw_tone_mark(canvas, cx, mark_y, tone);
                }
            }
            x += width;
        }
        if(spans && len && syllable < MAX_SYLLABLES) {
            spans[syllable][0] = syllable_x;
            spans[syllable][1] = x;
            syllable++;
        }

        s += len;
        if(tone) s++;
        if(*s == ' ') {
            s++;
            if(*s) x += SYLLABLE_GAP;
        } else if(*s && len == 0 && !tone) {
            s++;
        }
    }
    return x - start;
}

static void draw_hanzi(Canvas* canvas, const App* app) {
    // Four characters only fit edge to edge
    int gap = app->hanzi_count < MAX_HANZI ? GLYPH_GAP : 0;
    int width = app->hanzi_count * GLYPH_SIZE + (app->hanzi_count - 1) * gap;
    int x = (128 - width) / 2;
    for(uint8_t i = 0; i < app->hanzi_count; i++) {
        canvas_draw_xbm(canvas, x, 0, GLYPH_SIZE, GLYPH_SIZE, app->bitmaps[i]);
        x += GLYPH_SIZE + gap;
    }
}

static void draw_arrow(Canvas* canvas, int x, int y, bool right) {
    // 3 px wide, 5 px tall, tip at the given x
    int dir = right ? -1 : 1;
    canvas_draw_dot(canvas, x, y);
    canvas_draw_line(canvas, x + dir, y - 1, x + dir, y + 1);
    canvas_draw_line(canvas, x + 2 * dir, y - 2, x + 2 * dir, y + 2);
}

// Small filled triangle pointing up, right, down or left, about 5 px across, centred on x, y
static void draw_triangle(Canvas* canvas, int x, int y, int dir) {
    for(int i = 0; i < 3; i++) {
        switch(dir) {
        case 0:
            canvas_draw_line(canvas, x - i, y - 1 + i, x + i, y - 1 + i);
            break;
        case 1:
            canvas_draw_line(canvas, x + 1 - i, y - i, x + 1 - i, y + i);
            break;
        case 2:
            canvas_draw_line(canvas, x - i, y + 1 - i, x + i, y + 1 - i);
            break;
        default:
            canvas_draw_line(canvas, x - 1 + i, y - i, x - 1 + i, y + i);
            break;
        }
    }
}

static void draw_pinyin_centred(Canvas* canvas, const char* pinyin, int16_t spans[][2]) {
    canvas_set_font(canvas, FontPrimary);
    int width = pinyin_layout(canvas, pinyin, 0, 0, false, NULL);
    pinyin_layout(canvas, pinyin, (128 - width) / 2, PINYIN_Y, true, spans);
}

// The card's pinyin with the tones picked so far in place of the real ones
static void quiz_pinyin(const App* app, char* out, size_t size) {
    size_t n = 0;
    uint8_t syllable = 0;
    for(const char* s = app->card.pinyin; *s && n + 3 < size;) {
        while(*s >= 'a' && *s <= 'z' && n + 3 < size)
            out[n++] = *s++;
        if(*s >= '1' && *s <= '4') s++;
        if(syllable < app->guessed && app->guesses[syllable]) {
            out[n++] = '0' + app->guesses[syllable];
        }
        syllable++;
        if(*s == ' ') {
            out[n++] = ' ';
            s++;
        } else if(*s && !(*s >= 'a' && *s <= 'z')) {
            s++;
        }
    }
    out[n] = '\0';
}

static uint8_t quiz_correct(const App* app) {
    uint8_t correct = 0;
    for(uint8_t i = 0; i < app->syllables; i++) {
        if(app->guesses[i] == app->tones[i]) correct++;
    }
    return correct;
}

// Tone quiz: pick a tone for each syllable, then see how you did
static void draw_quiz(Canvas* canvas, const App* app) {
    int16_t spans[MAX_SYLLABLES][2] = {0};
    draw_hanzi(canvas, app);

    if(!app->revealed) {
        char text[TEXT_LEN + MAX_SYLLABLES];
        quiz_pinyin(app, text, sizeof(text));
        draw_pinyin_centred(canvas, text, spans);
        // Underline the syllable being asked
        if(app->guessed < app->syllables) {
            canvas_draw_line(
                canvas,
                spans[app->guessed][0],
                PINYIN_Y + 2,
                spans[app->guessed][1] - 2,
                PINYIN_Y + 2);
        }

        // Which button gives which tone
        static const char* const digits[4] = {"1", "2", "3", "4"};
        canvas_set_font(canvas, FontSecondary);
        for(int i = 0; i < 4; i++) {
            draw_triangle(canvas, 3 + i * 18, HINT_Y - 4, i);
            canvas_draw_str(canvas, 8 + i * 18, HINT_Y, digits[i]);
        }
        canvas_draw_str_aligned(canvas, 127, HINT_Y, AlignRight, AlignBottom, "OK: neutral");
        return;
    }

    draw_pinyin_centred(canvas, app->card.pinyin, spans);
    // Wrong syllables are shown inverted
    canvas_set_color(canvas, ColorXOR);
    for(uint8_t i = 0; i < app->syllables; i++) {
        if(app->guesses[i] != app->tones[i]) {
            canvas_draw_box(
                canvas, spans[i][0] - 1, PINYIN_Y - 12, spans[i][1] - spans[i][0] + 1, 13);
        }
    }
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, ENGLISH_Y, AlignCenter, AlignBottom, app->card.english);
    char text[24];
    uint8_t correct = quiz_correct(app);
    snprintf(text, sizeof(text), "Tones %u/%u", correct, app->syllables);
    canvas_draw_str(canvas, 0, HINT_Y, text);
    canvas_draw_str_aligned(canvas, 127, HINT_Y, AlignRight, AlignBottom, "OK: next");
}

static void draw_done(Canvas* canvas, const App* app) {
    Counts c = count_cards(app);
    char text[32];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Done for today!");
    canvas_set_font(canvas, FontSecondary);
    snprintf(text, sizeof(text), "%u reviews today", app->reviewed);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, text);
    snprintf(
        text,
        sizeof(text),
        "%s: %u due tomorrow",
        decks[app->deck_index].name,
        c.tomorrow + c.learning);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, text);
    canvas_draw_str_aligned(
        canvas,
        64,
        HINT_Y,
        AlignCenter,
        AlignBottom,
        c.fresh ? "OK: more new cards" : "OK: review ahead");
}

static void draw_card(Canvas* canvas, const App* app) {
    if(app->tone_quiz) {
        draw_quiz(canvas, app);
        return;
    }
    if(app->revealed || !app->english_front) {
        draw_hanzi(canvas, app);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, app->card.english);
    }

    canvas_set_font(canvas, FontSecondary);
    if(!app->revealed) {
        if(app->box[app->current] == BOX_NEW) {
            canvas_draw_str(canvas, 0, HINT_Y, "New card");
        } else {
            char text[24];
            snprintf(text, sizeof(text), "Due %u", count_cards(app).due);
            canvas_draw_str(canvas, 0, HINT_Y, text);
        }
        canvas_draw_str_aligned(canvas, 127, HINT_Y, AlignRight, AlignBottom, "OK: flip");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, ENGLISH_Y, AlignCenter, AlignBottom, app->card.english);
    draw_arrow(canvas, 0, HINT_Y - 4, false);
    canvas_draw_str(canvas, 6, HINT_Y, "Again");
    draw_arrow(canvas, 127, HINT_Y - 4, true);
    canvas_draw_str_aligned(canvas, 121, HINT_Y, AlignRight, AlignBottom, "Good");

    draw_pinyin_centred(canvas, app->card.pinyin, NULL);
}

// The whole deck, one card at a time, without touching progress
static void draw_browse(Canvas* canvas, const App* app) {
    draw_hanzi(canvas, app);
    draw_pinyin_centred(canvas, app->card.pinyin, NULL);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, ENGLISH_Y, AlignCenter, AlignBottom, app->card.english);

    char text[24];
    snprintf(text, sizeof(text), "%u/%u", app->current + 1, app->card_count);
    draw_arrow(canvas, 0, HINT_Y - 4, false);
    canvas_draw_str(canvas, 6, HINT_Y, text);
    draw_arrow(canvas, 127, HINT_Y - 4, true);

    uint8_t box = app->box[app->current];
    uint16_t now = today();
    if(box == BOX_NEW) {
        snprintf(text, sizeof(text), "new");
    } else if(app->due[app->current] <= now) {
        snprintf(text, sizeof(text), "due");
    } else {
        snprintf(text, sizeof(text), "in %ud", app->due[app->current] - now);
    }
    canvas_draw_str_aligned(canvas, 121, HINT_Y, AlignRight, AlignBottom, text);
}

static void draw_stats(Canvas* canvas, const App* app) {
    Counts c = count_cards(app);
    char text[32];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, decks[app->deck_index].name);
    canvas_set_font(canvas, FontSecondary);
    snprintf(text, sizeof(text), "%u cards", app->card_count);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, text);

    // One bar for the deck: known solid, in progress hatched, new empty
    const int bar_x = 2, bar_y = 13, bar_w = 124, bar_h = 7;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    if(app->card_count) {
        int known = (bar_w - 2) * c.known / app->card_count;
        int seen = (bar_w - 2) * (c.known + c.young + c.learning) / app->card_count;
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, known, bar_h - 2);
        for(int x = known; x < seen; x++) {
            for(int y = 0; y < bar_h - 2; y++) {
                if((x + y) % 2 == 0) canvas_draw_dot(canvas, bar_x + 1 + x, bar_y + 1 + y);
            }
        }
    }

    snprintf(text, sizeof(text), "Known %u", c.known);
    canvas_draw_str(canvas, 2, 30, text);
    snprintf(text, sizeof(text), "Learning %u", c.young + c.learning);
    canvas_draw_str(canvas, 66, 30, text);
    snprintf(text, sizeof(text), "New %u", c.fresh);
    canvas_draw_str(canvas, 2, 40, text);
    snprintf(text, sizeof(text), "Due today %u", c.due);
    canvas_draw_str(canvas, 66, 40, text);

    canvas_draw_line(canvas, 0, 43, 127, 43);
    uint16_t now = today();
    uint16_t reviewed = app->stat_day == now ? app->reviewed : 0;
    uint16_t streak = app->streak_day + 1 >= now ? app->streak : 0;
    snprintf(text, sizeof(text), "Reviews today: %u", reviewed);
    canvas_draw_str(canvas, 2, 53, text);
    snprintf(text, sizeof(text), "Day streak: %u", streak);
    canvas_draw_str(canvas, 2, 63, text);
}

static void draw_menu(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Settings");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);
    char known[24];
    snprintf(known, sizeof(known), "%u/%u", count_cards(app).known, app->card_count);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, known);

    char limit[8];
    snprintf(limit, sizeof(limit), "%u", new_per_day[app->new_limit]);
    const char* labels[MenuCount] = {
        [MenuDeck] = "Deck",
        [MenuMode] = "Mode",
        [MenuScript] = "Characters",
        [MenuFront] = "Front",
        [MenuSound] = "Tone sound",
        [MenuNewLimit] = "New cards a day",
        [MenuStats] = "Stats",
        [MenuBrowse] = "Browse deck",
        [MenuReset] = "Reset progress",
    };
    const char* values[MenuCount] = {
        [MenuDeck] = decks[app->deck_index].name,
        [MenuMode] = app->tone_quiz ? "Tone quiz" : "Flashcards",
        [MenuScript] = app->traditional ? "Traditional" : "Simplified",
        [MenuFront] = app->english_front ? "English" : "Hanzi",
        [MenuSound] = app->sound ? "On" : "Off",
        [MenuNewLimit] = limit,
        [MenuStats] = ">",
        [MenuBrowse] = ">",
        [MenuReset] = app->reset_armed ? "Sure? >" : ">",
    };
    for(uint8_t v = 0; v < MENU_VISIBLE; v++) {
        uint8_t i = app->menu_top + v;
        int y = 22 + v * 10;
        if(i == app->menu_item) {
            canvas_draw_box(canvas, 0, y - 8, 125, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, labels[i]);
        canvas_draw_str_aligned(canvas, 121, y, AlignRight, AlignBottom, values[i]);
        canvas_set_color(canvas, ColorBlack);
    }

    // Scroll bar
    const int track = 50;
    int thumb = track * MENU_VISIBLE / MenuCount;
    int offset = (track - thumb) * app->menu_top / (MenuCount - MENU_VISIBLE);
    canvas_draw_line(canvas, 127, 14 + offset, 127, 14 + offset + thumb);
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    if(app->menu_open) {
        draw_menu(canvas, app);
    } else if(app->error) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, app->error);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hold Back to quit");
    } else if(app->screen == ScreenStats) {
        draw_stats(canvas, app);
    } else if(app->screen == ScreenBrowse) {
        draw_browse(canvas, app);
    } else if(app->done) {
        draw_done(canvas, app);
    } else {
        draw_card(canvas, app);
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = context;
    furi_message_queue_put(queue, event, 0);
}

static void menu_change(App* app, int dir) {
    switch(app->menu_item) {
    case MenuDeck:
        progress_save(app);
        app->deck_index = (app->deck_index + COUNT_OF(decks) + dir) % COUNT_OF(decks);
        deck_start(app);
        break;
    case MenuMode:
        app->tone_quiz = !app->tone_quiz;
        app->revealed = false;
        app->guessed = 0;
        break;
    case MenuScript:
        app->traditional = !app->traditional;
        glyphs_load(app);
        break;
    case MenuFront:
        app->english_front = !app->english_front;
        break;
    case MenuSound:
        app->sound = !app->sound;
        if(!app->sound) tones_stop(app);
        break;
    case MenuNewLimit:
        app->new_limit = (app->new_limit + COUNT_OF(new_per_day) + dir) % COUNT_OF(new_per_day);
        break;
    case MenuStats:
        if(app->error) break;
        app->menu_open = false;
        app->screen = ScreenStats;
        break;
    case MenuBrowse:
        if(app->error) break;
        app->menu_open = false;
        app->screen = ScreenBrowse;
        app->study_card = app->current;
        card_load(app, app->done ? 0 : app->current);
        break;
    case MenuReset:
        if(!app->reset_armed) {
            app->reset_armed = true;
            return;
        }
        memset(app->box, BOX_NEW, sizeof(app->box));
        memset(app->due, 0, sizeof(app->due));
        app->new_today = 0;
        app->bonus_new = 0;
        app->ahead = false;
        deal(app, false);
        break;
    }
    app->reset_armed = false;
}

static void menu_input(App* app, const InputEvent* event) {
    bool step = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->menu_open = false;
        app->reset_armed = false;
        progress_save(app);
        // A new limit or deck may have changed whether there is anything to do
        if(app->done && !app->error && app->screen == ScreenCards) deal(app, false);
    } else if(step && event->key == InputKeyUp) {
        app->menu_item = (app->menu_item + MenuCount - 1) % MenuCount;
        app->reset_armed = false;
    } else if(step && event->key == InputKeyDown) {
        app->menu_item = (app->menu_item + 1) % MenuCount;
        app->reset_armed = false;
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        menu_change(app, -1);
    } else if(
        event->type == InputTypeShort &&
        (event->key == InputKeyRight || event->key == InputKeyOk)) {
        menu_change(app, 1);
    }
    if(app->menu_item < app->menu_top) app->menu_top = app->menu_item;
    if(app->menu_item >= app->menu_top + MENU_VISIBLE) {
        app->menu_top = app->menu_item - MENU_VISIBLE + 1;
    }
}

static void menu_show(App* app) {
    tones_stop(app);
    app->menu_open = true;
    app->menu_item = 0;
    app->menu_top = 0;
}

// Left/Right step through the deck, Up/Down jump ten cards
static void browse_input(App* app, const InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;
    int step = 0;
    switch(event->key) {
    case InputKeyLeft:
        step = -1;
        break;
    case InputKeyRight:
        step = 1;
        break;
    case InputKeyUp:
        step = -10;
        break;
    case InputKeyDown:
        step = 10;
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) tones_play(app);
        return;
    case InputKeyBack:
        if(event->type != InputTypeShort) return;
        tones_stop(app);
        app->screen = ScreenCards;
        if(!app->done) card_load(app, app->study_card);
        return;
    default:
        return;
    }
    tones_stop(app);
    int next = ((int)app->current + step) % (int)app->card_count;
    if(next < 0) next += app->card_count;
    card_load(app, (uint16_t)next);
}

static void quiz_input(App* app, const InputEvent* event) {
    if(app->revealed) {
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            tones_stop(app);
            rate(app, quiz_correct(app) == app->syllables);
        } else if(event->key == InputKeyUp) {
            tones_play(app);
        }
        return;
    }
    uint8_t tone;
    switch(event->key) {
    case InputKeyUp:
        tone = 1;
        break;
    case InputKeyRight:
        tone = 2;
        break;
    case InputKeyDown:
        tone = 3;
        break;
    case InputKeyLeft:
        tone = 4;
        break;
    case InputKeyOk:
        tone = 0;
        break;
    case InputKeyBack:
        // Take back the last pick
        if(app->guessed) app->guessed--;
        return;
    default:
        return;
    }
    app->guesses[app->guessed++] = tone;
    if(app->guessed >= app->syllables) {
        app->revealed = true;
        tones_play(app);
    }
}

static void card_input(App* app, const InputEvent* event) {
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        menu_show(app);
        return;
    }
    if(app->screen == ScreenBrowse) {
        browse_input(app, event);
        return;
    }
    if(event->type != InputTypeShort || app->error) return;

    if(app->screen == ScreenStats) {
        if(event->key == InputKeyBack || event->key == InputKeyOk) app->screen = ScreenCards;
        return;
    }
    if(app->done) {
        if(event->key == InputKeyOk) keep_going(app);
        return;
    }
    if(app->tone_quiz) {
        quiz_input(app, event);
        return;
    }

    if(!app->revealed) {
        if(event->key == InputKeyOk) {
            app->revealed = true;
            tones_play(app);
        }
        return;
    }
    switch(event->key) {
    case InputKeyOk:
    case InputKeyUp:
        tones_play(app);
        break;
    case InputKeyLeft:
        tones_stop(app);
        rate(app, false);
        break;
    case InputKeyRight:
        tones_stop(app);
        rate(app, true);
        break;
    case InputKeyBack:
        tones_stop(app);
        app->revealed = false;
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- main

int32_t hanzi_cards_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->deck = storage_file_alloc(app->storage);
    settings_load(app);
    deck_start(app);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent event;
        uint32_t timeout = app->player.active ? 10 : FuriWaitForever;
        if(furi_message_queue_get(queue, &event, timeout) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(event.type == InputTypeLong && event.key == InputKeyBack) {
                running = false;
            } else if(app->menu_open) {
                menu_input(app, &event);
            } else {
                card_input(app, &event);
            }
            furi_mutex_release(app->mutex);
            view_port_update(view_port);
        }
        tones_update(app);
    }

    tones_stop(app);
    progress_save(app);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    storage_file_close(app->deck);
    storage_file_free(app->deck);
    furi_record_close(RECORD_STORAGE);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
