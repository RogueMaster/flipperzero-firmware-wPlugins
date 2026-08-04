#include "FlipFlash.h"

typedef enum {
    FlashModeOrdered = 0,
    FlashModeRandom = 1,
} FlashMode;

typedef enum {
    FlashScreenDeck = 0,
    FlashScreenSettings = 1,
    FlashScreenRemoveConfirm = 2,
    FlashScreenHelp = 3,
} FlashScreen;

typedef enum {
    FlashSettingToggleMode = 0,
    FlashSettingResetDeck = 1,
    FlashSettingCycleDeck = 2,
    FlashSettingHelp = 3,
    FlashSettingBack = 4,
    FlashSettingCount = 5,
} FlashSetting;

typedef struct {
    char path[MAX_PATH_LENGTH];
    char name[MAX_TEXT_LENGTH];
} FlashDeckEntry;

typedef struct {
    char front[MAX_TEXT_LENGTH];
    char back[MAX_TEXT_LENGTH];
    bool removed;
} FlashCard;

typedef struct {
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    Gui* gui;
    NotificationApp* notifications;
    Storage* storage;
    File* file;
    File* state_file;
    FuriString* text_buffer;
    FlashCard cards[MAX_CARDS];
    size_t card_count;
    int current_card;
    bool showing_back;
    FlashMode mode;
    size_t deck_index;
    FlashScreen screen;
    FlashSetting settings_cursor;
    uint32_t flash_seed;
    FlashDeckEntry decks[MAX_DECKS];
    size_t deck_count;
    char current_deck_path[MAX_PATH_LENGTH];
    char current_state_path[MAX_PATH_LENGTH];
    size_t help_scroll;
    size_t settings_scroll;
} FlashApp;

static bool flash_write_line(File* file, const char* line);
static int flash_find_first_active(const FlashApp* app);
static void flash_refresh_deck_catalog(FlashApp* app);
static void flash_activate_deck(FlashApp* app, size_t deck_index, bool persist_selection);

static const char* const flash_sample_deck[] = {
    "hola|hello",
    "adios|goodbye",
    "gracias|thank you",
    "por favor|please",
    "si|yes",
    "no|no",
    "buenos dias|good morning",
    "buenas noches|good night",
};

static const char* const flash_verbs_deck[] = {
    "hablar|to speak",
    "comer|to eat",
    "vivir|to live",
    "ir|to go",
    "tener|to have",
    "hacer|to do/make",
    "querer|to want",
    "poder|can / to be able",
};

static const char* help_lines[] = {
    "Right: next card",
    "OK: flip front/back",
    "Down: remove card",
    "Up: settings",
    "Back: return/cancel/exit",
};
#define HELP_LINE_COUNT COUNT_OF(help_lines)

static void flash_trim(char* text) {
    if(!text) return;

    char* start = text;
    while(*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }

    if(start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    size_t len = strlen(text);
    while(len > 0) {
        char ch = text[len - 1];
        if(ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        text[len - 1] = '\0';
        len--;
    }
}

static void flash_reset_deck_state(FlashApp* app) {
    for(size_t i = 0; i < app->card_count; i++) {
        app->cards[i].removed = false;
    }

    app->mode = FlashModeOrdered;
    app->showing_back = false;
    app->screen = FlashScreenDeck;
    app->settings_cursor = FlashSettingToggleMode;
    app->current_card = flash_find_first_active(app);
}

static void flash_build_deck_stem(const char* filename, char* stem, size_t stem_size) {
    if(stem_size == 0) return;

    stem[0] = '\0';
    if(filename == NULL) return;

    strlcpy(stem, filename, stem_size);
    char* dot = strrchr(stem, '.');
    if(dot) {
        *dot = '\0';
    }
}

static void flash_build_state_path(const char* deck_path, char* state_path, size_t state_path_size) {
    char stem[MAX_TEXT_LENGTH];
    flash_build_deck_stem(strrchr(deck_path, '/') ? strrchr(deck_path, '/') + 1 : deck_path, stem, sizeof(stem));
    if(stem[0] == '\0') {
        strlcpy(stem, "deck", sizeof(stem));
    }

    // snprintf: format and safely copy text into a string buffer. 
    snprintf(state_path, state_path_size, "%s/%s.state", APP_DATA_DIR, stem);
}

static bool flash_is_deck_filename(const char* name) {
    if(name == NULL) return false;

    const char* dot = strrchr(name, '.');
    if(dot == NULL || strcmp(dot, ".txt") != 0) return false;

    return true;
}

static bool flash_write_default_deck(FlashApp* app, const char* filename, const char* const* lines, size_t line_count) {
    storage_common_mkdir(app->storage, APP_DATA_DIR);

    char deck_path[MAX_PATH_LENGTH];
    snprintf(deck_path, sizeof(deck_path), "%s/%s", APP_DATA_DIR, filename);

    if(!storage_file_open(app->file, deck_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Unable to create default deck file");
        storage_file_close(app->file);
        return false;
    }

    for(size_t i = 0; i < line_count; i++) {
        flash_write_line(app->file, lines[i]);
        flash_write_line(app->file, "\n");
    }

    storage_file_close(app->file);
    return true;
}

static bool flash_write_default_decks(FlashApp* app) {
    bool wrote_any = false;
    wrote_any |= flash_write_default_deck(app, "spanish_words.txt", flash_sample_deck, COUNT_OF(flash_sample_deck));
    wrote_any |= flash_write_default_deck(app, "spanish_verbs.txt", flash_verbs_deck, COUNT_OF(flash_verbs_deck));
    return wrote_any;
}

static bool flash_write_line(File* file, const char* line) {
    return storage_file_write(file, line, strlen(line));
}

static bool flash_read_line(File* file, char* buffer, size_t buffer_size) {
    if(buffer_size == 0) return false;

    size_t position = 0;
    bool saw_anything = false;
    bool saw_newline = false;

    while(!storage_file_eof(file)) {
        char ch = '\0';
        if(storage_file_read(file, &ch, 1) == 0) break;

        saw_anything = true;

        if(ch == '\r') continue;
        if(ch == '\n') {
            saw_newline = true;
            break;
        }

        if(position + 1 < buffer_size) {
            buffer[position++] = ch;
        }
    }

    buffer[position] = '\0';
    return saw_anything || saw_newline || position > 0;
}

static const FlashDeckEntry* flash_get_active_deck(const FlashApp* app) {
    if(app->deck_count == 0) return NULL;

    if(app->deck_index >= app->deck_count) {
        return &app->decks[0];
    }

    return &app->decks[app->deck_index];
}

static int flash_find_deck_index_by_path(const FlashApp* app, const char* path) {
    if(path == NULL) return -1;

    for(size_t i = 0; i < app->deck_count; i++) {
        if(strcmp(app->decks[i].path, path) == 0) return (int)i;
    }

    return -1;
}

static void flash_read_all_decks(FlashApp* app) {
    if(storage_dir_open(app->file, APP_DATA_DIR)) {
        FileInfo info;
        char name[MAX_TEXT_LENGTH];

        while(storage_dir_read(app->file, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!flash_is_deck_filename(name)) continue;
            if(app->deck_count >= MAX_DECKS) break;

            snprintf(app->decks[app->deck_count].path, sizeof(app->decks[app->deck_count].path), "%s/%s", APP_DATA_DIR, name);
            flash_build_deck_stem(name, app->decks[app->deck_count].name, sizeof(app->decks[app->deck_count].name));
            if(app->decks[app->deck_count].name[0] == '\0') {
                strlcpy(app->decks[app->deck_count].name, name, sizeof(app->decks[app->deck_count].name));
            }
            app->deck_count++;
        }

        storage_dir_close(app->file);
    }
}

static void flash_refresh_deck_catalog(FlashApp* app) {
    FURI_LOG_I(TAG, "refresh deck catalog start");
    app->deck_count = 0;
    storage_common_mkdir(app->storage, APP_DATA_DIR);

    flash_read_all_decks(app);

    if(app->deck_count == 0) {
        flash_write_default_decks(app);

        flash_read_all_decks(app);
    }

    for(size_t i = 1; i < app->deck_count; i++) {
        FlashDeckEntry entry = app->decks[i];
        size_t j = i;

        while(j > 0 && strcmp(entry.name, app->decks[j - 1].name) < 0) {
            app->decks[j] = app->decks[j - 1];
            j--;
        }

        app->decks[j] = entry;
    }

    FURI_LOG_I(TAG, "refresh deck catalog done count=%u", (unsigned)app->deck_count);
}

static bool flash_load_deck(FlashApp* app) {
    FURI_LOG_I(TAG, "load deck start path=%s", app->current_deck_path);
    app->card_count = 0;

    if(app->current_deck_path[0] == '\0') {
        return false;
    }

    if(!storage_file_open(app->file, app->current_deck_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(app->file);
        return false;
    }

    char line[MAX_LINE_LENGTH];
    while(flash_read_line(app->file, line, sizeof(line))) {
        if(line[0] == '\0' || line[0] == '#') continue;

        flash_trim(line);
        if(line[0] == '\0' || line[0] == '#') continue;

        char* separator = strchr(line, '|');
        if(!separator) separator = strchr(line, ';');
        if(!separator) continue;

        *separator = '\0';
        char* front = line;
        char* back = separator + 1;
        flash_trim(front);
        flash_trim(back);

        if(front[0] == '\0' || back[0] == '\0') continue;
        if(app->card_count >= MAX_CARDS) break;

        strlcpy(app->cards[app->card_count].front, front, sizeof(app->cards[app->card_count].front));
        strlcpy(app->cards[app->card_count].back, back, sizeof(app->cards[app->card_count].back));
        app->cards[app->card_count].removed = false;
        app->card_count++;
    }

    storage_file_close(app->file);

    FURI_LOG_I(TAG, "load deck done cards=%u", (unsigned)app->card_count);
    return true;
}

static const char* flash_get_active_deck_name(const FlashApp* app) {
    const FlashDeckEntry* deck = flash_get_active_deck(app);
    if(deck == NULL) return "No decks";

    return deck->name;
}

static void flash_save_state(FlashApp* app) {
    FURI_LOG_I(TAG, "save state start path=%s", app->current_state_path);

    storage_common_mkdir(app->storage, APP_DATA_DIR);

    if(app->current_state_path[0] == '\0') {
        return;
    }
    
    if(!storage_file_open(app->state_file, app->current_state_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Unable to save state file");
        storage_file_close(app->state_file);
        return;
    }

    char line[64];
    snprintf(line, sizeof(line), "mode=%s\n", app->mode == FlashModeRandom ? "random" : "ordered");
    flash_write_line(app->state_file, line);

    flash_write_line(app->state_file, "removed=");
    bool first_removed = true;
    for(size_t i = 0; i < app->card_count; i++) {
        if(!app->cards[i].removed) continue;
        if(!first_removed) flash_write_line(app->state_file, ",");
        snprintf(line, sizeof(line), "%zu", i);
        flash_write_line(app->state_file, line);
        first_removed = false;
    }
    flash_write_line(app->state_file, "\n");

    storage_file_close(app->state_file);
    return;
}

static void flash_apply_saved_state(FlashApp* app) {
    if(app->current_state_path[0] == '\0') {
        flash_reset_deck_state(app);
        return;
    }

    if(!storage_file_open(app->state_file, app->current_state_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(app->state_file);
        flash_reset_deck_state(app);
        return;
    }

    app->mode = FlashModeOrdered;
    for(size_t i = 0; i < app->card_count; i++) {
        app->cards[i].removed = false;
    }

    char line[MAX_LINE_LENGTH];
    while(flash_read_line(app->state_file, line, sizeof(line))) {
        flash_trim(line);

        if(strncmp(line, "mode=", 5) == 0) {
            const char* mode_value = line + 5;
            if(strcmp(mode_value, "random") == 0) {
                app->mode = FlashModeRandom;
            } else {
                app->mode = FlashModeOrdered;
            }
            continue;
        }

        if(strncmp(line, "removed=", 8) == 0) {
            const char* removed_value = line + 8;
            const char* cursor = removed_value;

            while(*cursor != '\0') {
                while(*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
                    cursor++;
                }

                if(*cursor == '\0') break;

                char* end = NULL;
                long index = strtol(cursor, &end, 10);
                if(index >= 0 && (size_t)index < app->card_count) {
                    app->cards[index].removed = true;
                }

                if(end == cursor) {
                    break;
                }

                cursor = end;
            }
        }
    }

    storage_file_close(app->state_file);
    return;
}

static int flash_count_active_cards(const FlashApp* app) {
    int active_count = 0;
    for(size_t i = 0; i < app->card_count; i++) {
        if(!app->cards[i].removed) active_count++;
    }
    return active_count;
}

static int flash_find_first_active(const FlashApp* app) {
    for(size_t i = 0; i < app->card_count; i++) {
        if(!app->cards[i].removed) return (int)i;
    }
    return -1;
}

static int flash_find_next_ordered(const FlashApp* app, int current_card) {
    if(app->card_count == 0) return -1;

    size_t start = 0;
    if(current_card >= 0) {
        start = (size_t)(current_card + 1);
    }

    for(size_t offset = 0; offset < app->card_count; offset++) {
        size_t index = (start + offset) % app->card_count;
        if(!app->cards[index].removed) return (int)index;
    }

    return -1;
}

static int flash_find_next_random(FlashApp* app, int current_card) {
    int active_count = flash_count_active_cards(app);
    if(active_count == 0) return -1;

    int pick = rand() % active_count;
    for(size_t i = 0; i < app->card_count; i++) {
        if(app->cards[i].removed) continue;
        if(pick == 0) {
            if((int)i == current_card && active_count > 1) {
                pick = rand() % active_count;
                i = (size_t)-1;
                continue;
            }
            return (int)i;
        }
        pick--;
    }

    return flash_find_first_active(app);
}

static int flash_find_next_card(FlashApp* app, int current_card) {
    if(app->card_count == 0) return -1;

    if(app->mode == FlashModeRandom) {
        return flash_find_next_random(app, current_card);
    }

    return flash_find_next_ordered(app, current_card);
}

static void flash_advance_to_next_card(FlashApp* app) {
    app->current_card = flash_find_next_card(app, app->current_card);
    app->showing_back = false;

    if(app->current_card < 0) {
        app->screen = FlashScreenDeck;
    }
}

static int flash_find_previous_card(FlashApp* app, int current_card) {
    if(app->card_count == 0) return -1;

    if(app->mode == FlashModeRandom) {
        return flash_find_next_random(app, current_card);
    }

    size_t start = app->card_count - 1;
    if(current_card >= 0) {
        start = (size_t)(current_card - 1);
    }

    for(size_t offset = 0; offset < app->card_count; offset++) {
        size_t index = (start + app->card_count - offset) % app->card_count;
        if(!app->cards[index].removed) return (int)index;
    }

    return -1;
}

static void flash_return_to_previous_card(FlashApp* app) {
    int previous_card = flash_find_previous_card(app, app->current_card);
    if(previous_card >= 0) {
        app->current_card = previous_card;
        app->showing_back = false;
    }
}

static void flash_reset_current_deck(FlashApp* app) {
    for(size_t i = 0; i < app->card_count; i++) {
        app->cards[i].removed = false;
    }

    app->mode = FlashModeOrdered;
    app->current_card = flash_find_first_active(app);
    app->showing_back = false;
    flash_save_state(app);
}

static void flash_draw_help(Canvas* canvas, FlashApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Help");

    canvas_set_font(canvas, FontSecondary);
    int start = app->help_scroll;
    int y = 26;
    for(size_t i = start; i < HELP_LINE_COUNT && y <= 64; i++) {
        canvas_draw_str(canvas, 4, y, help_lines[i]);
        y += HELP_LINE_HEIGHT;
    }
}

static void flash_cycle_deck(FlashApp* app) {
    if(app->deck_count == 0) return;

    int previous_index = app->deck_index;
    char previous_deck_path[MAX_PATH_LENGTH];
    strlcpy(previous_deck_path, app->current_deck_path, sizeof(previous_deck_path));

    flash_save_state(app);

    size_t next_index = (size_t)((previous_index + 1) % (int)app->deck_count);
    flash_activate_deck(app, next_index, true);
}

static void flash_activate_deck(FlashApp* app, size_t deck_index, bool persist_selection) {
    FURI_LOG_I(TAG, "activate deck start index=%u persist=%d count=%u", (unsigned)deck_index, persist_selection, (unsigned)app->deck_count);
    if(app->deck_count == 0) {
        app->current_deck_path[0] = '\0';
        app->current_state_path[0] = '\0';
        flash_reset_deck_state(app);
        FURI_LOG_E(TAG, "no active decks");
        return;
    }

    if(deck_index >= app->deck_count) {
        deck_index = 0;
    }

    app->deck_index = deck_index;
    strlcpy(app->current_deck_path, app->decks[app->deck_index].path, sizeof(app->current_deck_path));
    FURI_LOG_I(TAG, "activate deck path=%s", app->current_deck_path);
    flash_build_state_path(app->current_deck_path, app->current_state_path, sizeof(app->current_state_path));
    FURI_LOG_I(TAG, "activate deck state path=%s", app->current_state_path);

    if(!flash_load_deck(app)) {
        flash_reset_deck_state(app);
        FURI_LOG_E(TAG, "failed to load active deck");
        return;
    }

    flash_apply_saved_state(app);

    if(app->current_card < 0) {
        app->current_card = flash_find_first_active(app);
    }

    FURI_LOG_I(TAG, "activate deck done path=%s cards=%u current=%d", app->current_deck_path, (unsigned)app->card_count, app->current_card);
}

static void flash_remove_current_card(FlashApp* app) {
    if(app->current_card < 0 || (size_t)app->current_card >= app->card_count) return;

    app->cards[app->current_card].removed = true;
    flash_save_state(app);
    flash_advance_to_next_card(app);
}

static void flash_render_header(Canvas* canvas, const FlashApp* app) {
    char header[96];
    snprintf(
        header,
        sizeof(header),
        "%s | %.24s | %d/%zu",
        app->mode == FlashModeRandom ? "Random" : "In order",
        flash_get_active_deck_name(app),
        app->current_card >= 0 ? app->current_card + 1 : 0,
        app->card_count);
    canvas_draw_str(canvas, 2, 10, header);
}

static void flash_draw_deck(Canvas* canvas, FlashApp* app) {
    canvas_set_font(canvas, FontSecondary);
    flash_render_header(canvas, app);

    if(app->current_card < 0 || app->card_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No cards left");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Open settings for help");
        return;
    }

    const FlashCard* card = &app->cards[app->current_card];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, app->showing_back ? "Back" : "Front");

    canvas_set_font(canvas, FontPrimary);
    const char* card_text = app->showing_back ? card->back : card->front;
    elements_multiline_text_aligned(canvas, 8, 32, AlignLeft, AlignTop, card_text);
}

static bool flash_has_active_cards(const FlashApp* app) {
    return flash_count_active_cards(app) > 0;
}

static void flash_draw_settings(Canvas* canvas, FlashApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Settings");

    char deck_item[MAX_TEXT_LENGTH + 16];
    snprintf(deck_item, sizeof(deck_item), "Deck: %s", flash_get_active_deck_name(app));

    const char* items[FlashSettingCount] = {
        app->mode == FlashModeRandom ? "Order: random" : "Order: in order",
        "Reset current deck",
        deck_item,
        "Help / controls",
        "Back to deck",
    };

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = app->settings_scroll; i < FlashSettingCount && i < app->settings_scroll + SETTINGS_VISIBLE_ITEMS; i++) {
        const int y = 26 + (i - app->settings_scroll) * 12;

        if((FlashSetting)i == app->settings_cursor) {
            canvas_draw_box(canvas, 1, y - 9, 126, 11);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, items[i]);
        }
    }
}

static void flash_draw_remove_confirm(Canvas* canvas, const FlashApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Remove this card?");

    canvas_set_font(canvas, FontSecondary);
    if(app->current_card >= 0 && (size_t)app->current_card < app->card_count) {
        const FlashCard* card = &app->cards[app->current_card];
        elements_multiline_text_aligned(canvas, 8, 26, AlignLeft, AlignTop, card->front);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 54, "OK removes this card.");
    canvas_draw_str(canvas, 2, 64, "Back cancels.");
}

static void flash_draw_callback(Canvas* canvas, void* ctx) {
    FlashApp* app = ctx;
    canvas_clear(canvas);

    switch(app->screen) {
    case FlashScreenDeck:
        flash_draw_deck(canvas, app);
        break;
    case FlashScreenSettings:
        flash_draw_settings(canvas, app);
        break;
    case FlashScreenRemoveConfirm:
        flash_draw_remove_confirm(canvas, app);
        break;
    case FlashScreenHelp:
        flash_draw_help(canvas, app);
        break;
    }
}

static void flash_input_callback(InputEvent* event, void* ctx) {
    FlashApp* app = ctx;
    FURI_LOG_I(TAG, "Input callback %d %d", event->key, event->type);
    furi_message_queue_put(app->input_queue, event, 0);
}

static bool flash_handle_deck_input(FlashApp* app, InputEvent event) {
    if(event.type != InputTypeShort) return true;

    switch(event.key) {
    case InputKeyOk:
        if(app->current_card >= 0) {
            if(app->showing_back) {
                flash_advance_to_next_card(app);
            } else {
                app->showing_back = true;
            }
        }
        break;
    case InputKeyRight:
        if(app->current_card >= 0) {
            flash_advance_to_next_card(app);
        }
        break;
    case InputKeyLeft:
        if(app->current_card >= 0) {
            flash_return_to_previous_card(app);
        }
        break;
    case InputKeyUp:
        app->screen = FlashScreenSettings;
        app->settings_cursor = FlashSettingToggleMode;
        break;
    case InputKeyDown:
        if(app->current_card >= 0) {
            app->screen = FlashScreenRemoveConfirm;
        }
        break;
    case InputKeyBack:
        return false;
    default:
        break;
    }
    return true;
}

static void flash_handle_settings_input(FlashApp* app, InputEvent event) {
    if(event.type != InputTypeShort) return;

    switch(event.key) {
    case InputKeyUp:
        if(app->settings_cursor > 0) {
            app->settings_cursor--;

            if(app->settings_cursor < app->settings_scroll) {
                app->settings_scroll--;
            }
        }
        break;
    case InputKeyDown:
        if(app->settings_cursor + 1 < FlashSettingCount) {
            app->settings_cursor++;

            if(app->settings_cursor >=
            app->settings_scroll + SETTINGS_VISIBLE_ITEMS) {
                app->settings_scroll++;
            }
        }
        break;
    case InputKeyOk:
        switch(app->settings_cursor) {
        case FlashSettingToggleMode:
            app->mode = app->mode == FlashModeRandom ? FlashModeOrdered : FlashModeRandom;
            flash_save_state(app);
            break;
        case FlashSettingResetDeck:
            flash_reset_current_deck(app);
            break;
        case FlashSettingCycleDeck:
            flash_cycle_deck(app);
            break;
        case FlashSettingHelp:
            app->screen = FlashScreenHelp;
            break;
        case FlashSettingBack:
            app->settings_scroll = 0;
            app->settings_cursor = FlashSettingToggleMode;
            app->screen = FlashScreenDeck;
            break;
        default:
            break;
        }
        break;
    case InputKeyBack:
        app->screen = FlashScreenDeck;
        break;
    default:
        break;
    }
}

static void flash_handle_help_input(FlashApp* app, InputEvent event) {
    if(event.type != InputTypeShort) return;

    switch(event.key) {
    case InputKeyUp:
        if(app->help_scroll > 0) app->help_scroll--;
        break;
    case InputKeyDown:
        if(app->help_scroll + 1 < HELP_LINE_COUNT) app->help_scroll++;
        break;
    case InputKeyBack:
    case InputKeyOk:
        app->screen = FlashScreenSettings;
        break;
    default:
        break;
    }
}

static void flash_handle_remove_confirm_input(FlashApp* app, InputEvent event) {
    if(event.type != InputTypeShort) return;

    switch(event.key) {
    case InputKeyOk:
        flash_remove_current_card(app);
        app->screen = FlashScreenDeck;
        break;
    case InputKeyBack:
        app->screen = FlashScreenDeck;
        break;
    default:
        break;
    }
}

static void flash_load_or_create_deck(FlashApp* app) {
    FURI_LOG_I(TAG, "load or create deck start");
    flash_refresh_deck_catalog(app);

    size_t default_deck_index = 0;
    if(app->deck_count > 0 && app->current_deck_path[0] == '\0') {
        char default_deck_path[MAX_PATH_LENGTH];
        snprintf(default_deck_path, sizeof(default_deck_path), "%s/spanish_words.txt", APP_DATA_DIR);

        int default_index = flash_find_deck_index_by_path(app, default_deck_path);
        if(default_index < 0) {
            default_index = 0;
        }

        default_deck_index = (size_t)default_index;

        strlcpy(app->current_deck_path, app->decks[default_deck_index].path, sizeof(app->current_deck_path));
        flash_build_state_path(app->current_deck_path, app->current_state_path, sizeof(app->current_state_path));
        FURI_LOG_I(TAG, "default deck resolved index=%u path=%s", (unsigned)default_deck_index, app->current_deck_path);
    }

    FURI_LOG_I(TAG, "using default deck index=%u path=%s", (unsigned)default_deck_index, app->deck_count > 0 ? app->decks[default_deck_index].path : "<none>");
    flash_activate_deck(app, default_deck_index, false);
    FURI_LOG_I(TAG, "load or create deck done via default deck");
}

int32_t flipflash(void* p) {
    UNUSED(p);

    FlashApp* app = calloc(1, sizeof(FlashApp));
    app->view_port = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    // The Storage service reads your target path, parses the prefix (like /ext/), 
    // and dynamically routes your request to the correct file system driver.
    app->storage = furi_record_open(RECORD_STORAGE);
    // A virtual file handle that is used to read and write files on the storage service.
    app->file = storage_file_alloc(app->storage);
    app->state_file = storage_file_alloc(app->storage);
    app->text_buffer = furi_string_alloc();
    app->flash_seed = furi_get_tick();

    srand(app->flash_seed);
    app->card_count = 0;
    app->current_card = -1;
    app->showing_back = false;
    app->mode = FlashModeOrdered;
    app->deck_index = 0;
    app->deck_count = 0;
    app->current_deck_path[0] = '\0';
    app->current_state_path[0] = '\0';
    app->screen = FlashScreenDeck;
    app->settings_cursor = FlashSettingToggleMode;
    app->help_scroll = 0;
    app->settings_scroll = 0;

    view_port_draw_callback_set(app->view_port, flash_draw_callback, app);
    view_port_input_callback_set(app->view_port, flash_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_enabled_set(app->view_port, true);

    flash_load_or_create_deck(app);
    if(app->current_card < 0) {
        app->current_card = flash_find_first_active(app);
    }
    FURI_LOG_I(TAG, "Deck count=%d cards=%d path=%s",
                app->deck_count,
                app->card_count,
                app->current_deck_path);

    if(!flash_has_active_cards(app)) {
        app->screen = FlashScreenSettings;
        app->settings_cursor = FlashSettingResetDeck;
    }

    view_port_update(app->view_port);

    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        if(event.type == InputTypeShort && event.key == InputKeyBack && app->screen == FlashScreenDeck) {
            running = false;
            continue;
        }

        switch(app->screen) {
        case FlashScreenDeck:
            running = flash_handle_deck_input(app, event);
            break;
        case FlashScreenSettings:
            flash_handle_settings_input(app, event);
            break;
        case FlashScreenRemoveConfirm:
            flash_handle_remove_confirm_input(app, event);
            break;
        case FlashScreenHelp:
            flash_handle_help_input(app, event);
            break;
        }

        if(app->current_card < 0 && flash_count_active_cards(app) > 0) {
            app->current_card = flash_find_next_card(app, -1);
        }

        view_port_update(app->view_port);
    }

    flash_save_state(app);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_string_free(app->text_buffer);
    storage_file_free(app->file);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
