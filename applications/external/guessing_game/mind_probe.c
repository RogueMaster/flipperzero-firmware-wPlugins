#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // Required for uint32_t

#define DB_PATH        APP_ASSETS_PATH("database.csv")
#define INDEX_PATH     APP_ASSETS_PATH("database.idx")
#define MAX_LINE_LEN   256
#define MAX_LINE_WIDTH 120
#define LINE_HEIGHT    10
#define MAX_HISTORY    30

// --- Types ---
typedef struct {
    uint32_t id;
    uint32_t offset;
} IndexEntry;

typedef struct {
    int id;
    bool is_question;
    char* text;
    int yes_id;
    int no_id;
} GameNode;

typedef enum {
    SelectionYes,
    SelectionNo
} Selection;
typedef enum {
    GameStateStandaloneIntro,
    GameStatePlaying,
    GameStateEnd,
    GameStateError
} GameState;
typedef enum {
    StateSelectMode,
    StateStandalone,
    StateConnectedPlaceholder
} AppState;
typedef enum {
    ModeSelectionStandalone,
    ModeSelectionConnected
} ModeSelection;

typedef struct {
    AppState current_app_state;
    ModeSelection mode_selection;
    GameNode* current_node;
    Selection user_selection;
    GameState game_state;
    bool is_correct_guess;
    char* error_message;
    FuriMutex* mutex;
    int history_stack[MAX_HISTORY];
    int history_sp;
} GameAppState;

// --- Forward Declarations ---
static void game_render_callback(Canvas* canvas, void* context);
static void game_input_callback(InputEvent* input_event, void* context);
static GameNode* load_node_from_sd(int target_id);

// --- Draw Utilities ---
static void draw_multiline_text(Canvas* canvas, const char* text) {
    FuriString* word = furi_string_alloc();
    FuriString* line = furi_string_alloc();
    const char* ptr = text;
    int y = 10;

    while(*ptr) {
        furi_string_reset(word);
        while(*ptr && *ptr != ' ' && *ptr != '\n')
            furi_string_push_back(word, *ptr++);

        if(furi_string_size(line) > 0) furi_string_push_back(line, ' ');
        furi_string_cat(line, furi_string_get_cstr(word));

        if(canvas_string_width(canvas, furi_string_get_cstr(line)) > MAX_LINE_WIDTH) {
            furi_string_left(line, furi_string_size(line) - furi_string_size(word) - 1);
            canvas_draw_str_aligned(
                canvas, 64, y, AlignCenter, AlignTop, furi_string_get_cstr(line));
            y += LINE_HEIGHT;
            furi_string_set(line, furi_string_get_cstr(word));
        }

        if(*ptr == '\n') {
            canvas_draw_str_aligned(
                canvas, 64, y, AlignCenter, AlignTop, furi_string_get_cstr(line));
            y += LINE_HEIGHT;
            furi_string_reset(line);
            ptr++;
        }

        if(*ptr == ' ') ptr++;
    }

    if(furi_string_size(line) > 0)
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, furi_string_get_cstr(line));

    furi_string_free(word);
    furi_string_free(line);
}

static void free_current_node(GameAppState* state) {
    if(state->current_node) {
        free(state->current_node->text);
        free(state->current_node);
        state->current_node = NULL;
    }
}

static GameNode* load_node_from_sd(int target_id) {
    const char* TAG = "20Q_DEBUG";
    // NOTE: You can remove the FURI_LOG_D messages now if you wish, but it's fine to leave them.

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* index_file = storage_file_alloc(storage);
    File* db_file = storage_file_alloc(storage);
    GameNode* found_node = NULL;
    uint32_t target_offset = UINT32_MAX;

    // Part 1: Read the index file to find the byte offset
    if(!storage_file_open(index_file, INDEX_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "ERROR: Failed to open index file: %s", INDEX_PATH);
        storage_file_free(index_file);
        storage_file_free(db_file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    IndexEntry entry;
    while(storage_file_read(index_file, &entry, sizeof(IndexEntry)) == sizeof(IndexEntry)) {
        if(entry.id == (uint32_t)target_id) {
            target_offset = entry.offset;
            break;
        }
    }
    storage_file_close(index_file);

    if(target_offset == UINT32_MAX) {
        FURI_LOG_E(TAG, "ERROR: Target ID %d not found in the index file.", target_id);
        storage_file_free(index_file);
        storage_file_free(db_file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Part 2: Open the DB file, seek, and read the line character by character
    if(!storage_file_open(db_file, DB_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "ERROR: Failed to open database file: %s", DB_PATH);
        storage_file_free(index_file);
        storage_file_free(db_file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    storage_file_seek(db_file, target_offset, true);

    char line_buf[MAX_LINE_LEN] = {0};
    char char_buf;
    uint16_t line_pos = 0;
    // --- MODIFIED LOOP START ---
    while(line_pos < (MAX_LINE_LEN - 1) && storage_file_read(db_file, &char_buf, 1) == 1) {
        if(char_buf == '\n') { // Stop at newline
            break;
        }
        // THE FIX: Explicitly ignore carriage return characters
        if(char_buf != '\r') {
            line_buf[line_pos++] = char_buf;
        }
    }
    // --- MODIFIED LOOP END ---
    line_buf[line_pos] = '\0';
    storage_file_close(db_file);
    FURI_LOG_I(TAG, "Line read from DB: \"%s\"", line_buf);

    // Part 3: Parse the single line we just read
    char text_buf[MAX_LINE_LEN] = {0};
    char temp_buf[20] = {0};
    int id = 0, yes_id = 0, no_id = 0;
    char type = ' ';
    int temp_idx = 0;
    bool in_quotes = false;
    enum {
        S_ID,
        S_TYPE,
        S_TEXT,
        S_YES,
        S_NO,
        S_DONE
    } state = S_ID;
    for(uint16_t i = 0; i < strlen(line_buf); ++i) {
        char c = line_buf[i];
        switch(state) {
        case S_ID:
            if(c >= '0' && c <= '9')
                temp_buf[temp_idx++] = c;
            else if(c == ',') {
                id = atoi(temp_buf);
                temp_idx = 0;
                memset(temp_buf, 0, sizeof(temp_buf));
                state = S_TYPE;
            }
            break;
        case S_TYPE:
            if(c != ',')
                type = c;
            else
                state = S_TEXT;
            break;
        case S_TEXT:
            if(c == '"' && !in_quotes)
                in_quotes = true;
            else if(c == '"' && in_quotes)
                in_quotes = false;
            else if(c == ',' && !in_quotes) {
                text_buf[temp_idx] = '\0';
                temp_idx = 0;
                state = S_YES;
            } else if(in_quotes)
                text_buf[temp_idx++] = c;
            break;
        case S_YES:
            if(c >= '0' && c <= '9')
                temp_buf[temp_idx++] = c;
            else if(c == ',') {
                yes_id = atoi(temp_buf);
                temp_idx = 0;
                memset(temp_buf, 0, sizeof(temp_buf));
                state = S_NO;
            }
            break;
        case S_NO:
            if(c >= '0' && c <= '9') temp_buf[temp_idx++] = c;
            break;
        default:
            break;
        }
    }
    if(state == S_NO) {
        temp_buf[temp_idx] = '\0';
        no_id = atoi(temp_buf);
        found_node = malloc(sizeof(GameNode));
        found_node->id = id;
        found_node->is_question = (type == 'Q');
        found_node->text = strdup(text_buf);
        found_node->yes_id = yes_id;
        found_node->no_id = no_id;
        FURI_LOG_D(
            TAG,
            "Successfully parsed node: id=%d, yes=%d, no=%d, text='%s'",
            id,
            yes_id,
            no_id,
            text_buf);
    } else {
        FURI_LOG_E(TAG, "ERROR: Failed to parse line_buf. Final parser state was not S_NO.");
    }

    storage_file_free(index_file);
    storage_file_free(db_file);
    furi_record_close(RECORD_STORAGE);

    if(found_node == NULL) {
        FURI_LOG_E(TAG, "Function returning NULL for target_id %d", target_id);
    }
    return found_node;
}

static void reset_game_state(GameAppState* state) {
    free_current_node(state);
    state->current_node = load_node_from_sd(1);
    if(!state->current_node) {
        state->game_state = GameStateError;
        state->error_message = strdup("Failed to load node 1");
    } else {
        state->user_selection = SelectionYes;
        state->game_state = GameStateStandaloneIntro;
        state->history_sp = 0;
    }
}

static void handle_input(GameAppState* state, InputEvent* event, bool* running) {
    if(event->key == InputKeyBack) {
        if(state->game_state == GameStatePlaying && state->history_sp > 0) {
            int prev_id = state->history_stack[--state->history_sp];
            free_current_node(state);
            state->current_node = load_node_from_sd(prev_id);
        } else {
            *running = false;
        }
        return;
    }

    switch(state->game_state) {
    case GameStateStandaloneIntro:
        if(event->key == InputKeyOk) {
            state->game_state = GameStatePlaying;
        }
        break;
    case GameStateEnd:
        if(event->key == InputKeyOk) {
            reset_game_state(state);
        }
        break;
    case GameStatePlaying:
        if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            state->user_selection = (event->key == InputKeyLeft) ? SelectionYes : SelectionNo;
        } else if(event->key == InputKeyOk) {
            if(state->history_sp < MAX_HISTORY) {
                state->history_stack[state->history_sp++] = state->current_node->id;
            }
            int next_id = (state->user_selection == SelectionYes) ? state->current_node->yes_id :
                                                                    state->current_node->no_id;

            free_current_node(state);
            if(next_id <= 0) {
                state->game_state = GameStateEnd;
                state->is_correct_guess = false;
            } else {
                state->current_node = load_node_from_sd(next_id);
                if(!state->current_node) {
                    state->game_state = GameStateEnd;
                    state->is_correct_guess = false;
                } else if(!state->current_node->is_question) {
                    state->game_state = GameStateEnd;
                    state->is_correct_guess = true;
                }
            }
        }
        break;
    default:
        break;
    }
}

static void game_render_callback(Canvas* canvas, void* context) {
    GameAppState* app_state = context;
    furi_mutex_acquire(app_state->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontSecondary);
    switch(app_state->game_state) {
    case GameStateStandaloneIntro:
        draw_multiline_text(canvas, "Think of a person,\nplace, or thing...");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "OK to Start");
        break;
    case GameStatePlaying:
        draw_multiline_text(canvas, app_state->current_node->text);

        // Box layout
        const int box_w = 24;
        const int box_h = 12;
        const int box_y = 48;
        const int yes_x = 32;
        const int no_x = 96;

        // Draw labels (centered vertically & horizontally)
        canvas_draw_str_aligned(canvas, yes_x, box_y + box_h / 2, AlignCenter, AlignCenter, "Yes");
        canvas_draw_str_aligned(canvas, no_x, box_y + box_h / 2, AlignCenter, AlignCenter, "No");

        // Draw only the selected box
        if(app_state->user_selection == SelectionYes) {
            canvas_draw_frame(canvas, yes_x - box_w / 2, box_y, box_w, box_h);
        } else {
            canvas_draw_frame(canvas, no_x - box_w / 2, box_y, box_w, box_h);
        }
        break;
    case GameStateEnd:
        draw_multiline_text(
            canvas, app_state->is_correct_guess ? "I guessed it!" : "You win!\nI was stumped.");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "OK to Restart");
        break;
    case GameStateError:
        draw_multiline_text(canvas, app_state->error_message);
        break;
    }

    furi_mutex_release(app_state->mutex);
}

static void game_input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* queue = context;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(queue, input_event, FuriWaitForever);
    }
}

int32_t mind_probe_app(void* p) {
    UNUSED(p);
    GameAppState* state = malloc(sizeof(GameAppState));
    state->current_app_state = StateStandalone;
    state->mode_selection = ModeSelectionStandalone;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state->history_sp = 0;
    state->error_message = NULL;
    state->current_node = NULL;
    reset_game_state(state);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, game_render_callback, state);
    view_port_input_callback_set(vp, game_input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;
    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            furi_mutex_acquire(state->mutex, FuriWaitForever);
            handle_input(state, &event, &running);
            furi_mutex_release(state->mutex);
            view_port_update(vp);
        }
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free_current_node(state);
    if(state->error_message) free(state->error_message);
    free(state);
    return 0;
}
