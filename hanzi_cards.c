// Hanzi Cards - Mandarin flashcards for Flipper Zero.
//
// Shows simplified or traditional characters from a deck file on the SD card, with pinyin
// (drawn with real tone marks), English, and the tone contours played on the
// speaker. Cards are scheduled with a small Leitner box system.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#define GLYPH_SIZE 32
#define GLYPH_BYTES (GLYPH_SIZE * GLYPH_SIZE / 8)
#define GLYPH_GAP 2
#define MAX_HANZI 4
#define TEXT_LEN 24
#define MAX_CARDS 512

#define DECK_PATH_FORMAT APP_ASSETS_PATH("%s.deck")
#define SAVE_DIR EXT_PATH("apps_data/hanzi_cards")
#define SAVE_PATH_FORMAT SAVE_DIR "/%s.sav"
#define SETTINGS_PATH SAVE_DIR "/settings.bin"
#define PATH_LEN 64

typedef struct {
    const char* name;
    const char* file;
} DeckInfo;

static const DeckInfo decks[] = {
    {"HSK 1", "hsk1"},
    {"HSK 2", "hsk2"},
    {"HSK 3", "hsk3"},
};

// Leitner boxes: 0 is a card never seen, 1 is the most frequent review
#define BOX_NEW 0
#define BOX_MAX 5
#define BOX_KNOWN 3
#define BOX_LEARNING 2
// New cards are only introduced while fewer than this many are being learned
#define LEARNING_TARGET 6
#define SAVE_EVERY 10

// Screen layout (baselines)
#define PINYIN_Y 45
#define ENGLISH_Y 54
#define HINT_Y 63
// x-height of FontPrimary, which the tone marks sit above
#define PINYIN_XH 6
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

typedef struct {
    char magic[4];
    uint8_t reserved[2];
    uint16_t count;
} SaveHeader;

typedef struct {
    char magic[4];
    uint8_t deck;
    uint8_t english_front;
    uint8_t sound;
    uint8_t traditional;
} Settings;

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
    MenuScript,
    MenuFront,
    MenuSound,
    MenuReset,
    MenuCount,
} MenuItem;

typedef struct {
    FuriMutex* mutex;
    Storage* storage;
    File* deck;
    const char* error;

    uint16_t card_count;
    uint16_t glyph_count;
    uint8_t box[MAX_CARDS];

    uint16_t current;
    CardRecord card;
    uint8_t hanzi_count;
    uint8_t bitmaps[MAX_HANZI][GLYPH_BYTES];
    bool revealed;
    uint8_t unsaved;

    uint8_t deck_index;
    bool english_front;
    bool sound;
    bool traditional;

    bool menu_open;
    uint8_t menu_item;
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
}

// ---------------------------------------------------------------- progress

static void settings_load(App* app) {
    app->sound = true;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Settings settings;
        if(storage_file_read(file, &settings, sizeof(settings)) == sizeof(settings) &&
           memcmp(settings.magic, "HZC1", 4) == 0) {
            if(settings.deck < COUNT_OF(decks)) app->deck_index = settings.deck;
            app->english_front = settings.english_front;
            app->sound = settings.sound;
            app->traditional = settings.traditional;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void progress_load(App* app) {
    char path[PATH_LEN];
    snprintf(path, sizeof(path), SAVE_PATH_FORMAT, decks[app->deck_index].file);
    memset(app->box, BOX_NEW, sizeof(app->box));
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveHeader header;
        if(storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
           memcmp(header.magic, "HZS1", 4) == 0 && header.count == app->card_count) {
            storage_file_read(file, app->box, app->card_count);
            for(uint16_t i = 0; i < app->card_count; i++) {
                if(app->box[i] > BOX_MAX) app->box[i] = BOX_MAX;
            }
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
            .magic = {'H', 'Z', 'C', '1'},
            .deck = app->deck_index,
            .english_front = app->english_front,
            .sound = app->sound,
            .traditional = app->traditional,
        };
        storage_file_write(file, &settings, sizeof(settings));
    }
    storage_file_close(file);

    char path[PATH_LEN];
    snprintf(path, sizeof(path), SAVE_PATH_FORMAT, decks[app->deck_index].file);
    if(app->card_count > 0 && storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SaveHeader header = {
            .magic = {'H', 'Z', 'S', '1'},
            .count = app->card_count,
        };
        storage_file_write(file, &header, sizeof(header));
        storage_file_write(file, app->box, app->card_count);
    }
    storage_file_close(file);
    storage_file_free(file);
    app->unsaved = 0;
}

static uint16_t pick_next(const App* app, bool avoid_current);

// Opens the selected deck and deals its first card
static void deck_start(App* app) {
    if(deck_open(app)) {
        app->error = NULL;
        progress_load(app);
        card_load(app, pick_next(app, false));
    } else {
        app->error = "Deck file not found";
    }
}

static uint16_t known_count(const App* app) {
    uint16_t known = 0;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(app->box[i] >= BOX_KNOWN) known++;
    }
    return known;
}

// ---------------------------------------------------------------- scheduling

// Lower boxes come up more often
static const uint8_t box_weight[BOX_MAX + 1] = {0, 16, 8, 4, 2, 1};

static uint16_t pick_next(const App* app, bool avoid_current) {
    uint16_t seen = 0;
    uint16_t learning = 0;
    uint16_t first_new = app->card_count;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(app->box[i] == BOX_NEW) {
            if(first_new == app->card_count) first_new = i;
        } else {
            seen++;
            if(app->box[i] <= BOX_LEARNING) learning++;
        }
    }

    bool has_new = first_new < app->card_count;
    if(has_new && (learning < LEARNING_TARGET || seen < 2)) return first_new;

    bool skip_current = avoid_current && seen > 1;
    uint32_t total = 0;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(skip_current && i == app->current) continue;
        total += box_weight[app->box[i]];
    }
    if(total == 0) return app->current;

    uint32_t roll = furi_hal_random_get() % total;
    for(uint16_t i = 0; i < app->card_count; i++) {
        if(skip_current && i == app->current) continue;
        uint8_t weight = box_weight[app->box[i]];
        if(roll < weight) return i;
        roll -= weight;
    }
    return app->current;
}

static void rate(App* app, bool good) {
    uint8_t* box = &app->box[app->current];
    if(!good) {
        *box = 1;
    } else if(*box == BOX_NEW) {
        // Already knew it the first time round
        *box = BOX_KNOWN;
    } else if(*box < BOX_MAX) {
        (*box)++;
    }
    if(++app->unsaved >= SAVE_EVERY) progress_save(app);
    card_load(app, pick_next(app, true));
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

    player->count = 0;
    for(const char* s = app->card.pinyin; *s && player->count < COUNT_OF(player->tones);) {
        while(*s >= 'a' && *s <= 'z') s++;
        uint8_t tone = 0;
        if(*s >= '1' && *s <= '4') tone = *s++ - '0';
        player->tones[player->count++] = tone;
        while(*s && !(*s >= 'a' && *s <= 'z')) s++;
    }
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
// pinyin. Returns the width in pixels.
static int pinyin_layout(Canvas* canvas, const char* s, int x, int y, bool draw) {
    int start = x;
    while(*s) {
        int len = 0;
        while(s[len] >= 'a' && s[len] <= 'z') len++;
        int tone = (s[len] >= '1' && s[len] <= '4') ? s[len] - '0' : 0;
        int mark = tone ? tone_vowel(s, len) : -1;

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

static void draw_card(Canvas* canvas, const App* app) {
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
            snprintf(text, sizeof(text), "Known %u/%u", known_count(app), app->card_count);
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

    canvas_set_font(canvas, FontPrimary);
    int width = pinyin_layout(canvas, app->card.pinyin, 0, 0, false);
    pinyin_layout(canvas, app->card.pinyin, (128 - width) / 2, PINYIN_Y, true);
}

static void draw_menu(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Settings");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);
    char known[24];
    snprintf(known, sizeof(known), "%u/%u", known_count(app), app->card_count);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, known);

    const char* labels[MenuCount] = {
        "Deck",
        "Characters",
        "Front",
        "Tone sound",
        "Reset progress",
    };
    const char* values[MenuCount] = {
        decks[app->deck_index].name,
        app->traditional ? "Traditional" : "Simplified",
        app->english_front ? "English" : "Hanzi",
        app->sound ? "On" : "Off",
        app->reset_armed ? "Sure? >" : ">",
    };
    for(uint8_t i = 0; i < MenuCount; i++) {
        int y = 22 + i * 10;
        if(i == app->menu_item) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, labels[i]);
        canvas_draw_str_aligned(canvas, 124, y, AlignRight, AlignBottom, values[i]);
        canvas_set_color(canvas, ColorBlack);
    }
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

static void menu_change(App* app) {
    switch(app->menu_item) {
    case MenuDeck:
        progress_save(app);
        app->deck_index = (app->deck_index + 1) % COUNT_OF(decks);
        deck_start(app);
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
    case MenuReset:
        if(!app->reset_armed) {
            app->reset_armed = true;
            return;
        }
        memset(app->box, BOX_NEW, sizeof(app->box));
        card_load(app, pick_next(app, false));
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
    } else if(step && event->key == InputKeyUp) {
        app->menu_item = (app->menu_item + MenuCount - 1) % MenuCount;
        app->reset_armed = false;
    } else if(step && event->key == InputKeyDown) {
        app->menu_item = (app->menu_item + 1) % MenuCount;
        app->reset_armed = false;
    } else if(event->type == InputTypeShort &&
              (event->key == InputKeyLeft || event->key == InputKeyRight ||
               event->key == InputKeyOk)) {
        menu_change(app);
    }
}

static void card_input(App* app, const InputEvent* event) {
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        tones_stop(app);
        app->menu_open = true;
        app->menu_item = 0;
        return;
    }
    if(event->type != InputTypeShort || app->error) return;

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
