#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "flippar_icons.h"

#define FLIPPAR_MAX_PLAYERS 10
#define FLIPPAR_MAX_HOLES 27
#define FLIPPAR_NAME_LEN 32
#define FLIPPAR_PLAYER_NAME_DISPLAY_LEN 6
#define FLIPPAR_SAVE_DIR "/ext/apps_data/flippar"
#define FLIPPAR_SAVE_BASENAME "FlipPar"
#define FLIPPAR_STATE_PATH FLIPPAR_SAVE_DIR "/current_round.bin"
#define FLIPPAR_STATE_MAGIC 0x46504C52UL
#define FLIPPAR_STATE_VERSION 3
#define FLIPPAR_MAX_STROKES 99
#define FLIPPAR_SCORE_CARD_HOLE_WIDTH 4
#define FLIPPAR_SCORE_CARD_PAR_WIDTH 3
#define FLIPPAR_SCORE_CARD_PLAYER_WIDTH 10
#define FLIPPAR_GRID_NAME_CHAR_WIDTH 6
#define FLIPPAR_GRID_NAME_COL_PADDING 4
#define FLIPPAR_GRID_MAX_NAME_CHARS FLIPPAR_PLAYER_NAME_DISPLAY_LEN
#define FLIPPAR_GRID_SCORE_CELL_WIDTH 16
#define FLIPPAR_GRID_SCORE_COL_STEP 19

typedef enum {
    FlipParScreenSplash,
    FlipParScreenSetup,
    FlipParScreenGrid,
    FlipParScreenConfirmNewGame,
    FlipParScreenSaveResult,
} FlipParScreen;

typedef enum {
    FlipParFieldHoles,
    FlipParFieldPlayers,
    FlipParFieldNames,
    FlipParFieldStart,
    FlipParFieldNewGame,
    FlipParFieldGridLines,
    FlipParFieldSave,
} FlipParSetupField;

typedef enum {
    FlipParViewMain,
    FlipParViewTextInput,
} FlipParViewId;

typedef enum {
    FlipParCustomEventSplashDone = 1,
} FlipParCustomEvent;

typedef struct {
    uint8_t dummy;
} FlipParViewModel;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t holes;
    uint8_t players;
    char player_names[FLIPPAR_MAX_PLAYERS][FLIPPAR_NAME_LEN];
    uint8_t pars[FLIPPAR_MAX_HOLES];
    uint8_t scores[FLIPPAR_MAX_PLAYERS][FLIPPAR_MAX_HOLES];
    FlipParScreen screen;
    FlipParSetupField setup_field;
    uint8_t setup_name_index;
    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t scroll_hole_offset;
    uint8_t scroll_row_offset;
} FlipParPersistedState;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t holes;
    uint8_t players;
    char player_names[FLIPPAR_MAX_PLAYERS][FLIPPAR_NAME_LEN];
    uint8_t pars[FLIPPAR_MAX_HOLES];
    uint8_t scores[FLIPPAR_MAX_PLAYERS][FLIPPAR_MAX_HOLES];
    FlipParScreen screen;
    FlipParSetupField setup_field;
    uint8_t setup_name_index;
    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t scroll_hole_offset;
    uint8_t scroll_row_offset;
    bool show_grid_lines;
} FlipParPersistedStateV3;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    TextInput* text_input;
    FuriTimer* splash_timer;

    uint8_t holes;
    uint8_t players;
    char player_names[FLIPPAR_MAX_PLAYERS][FLIPPAR_NAME_LEN];
    uint8_t pars[FLIPPAR_MAX_HOLES];
    uint8_t scores[FLIPPAR_MAX_PLAYERS][FLIPPAR_MAX_HOLES];

    FlipParScreen screen;
    FlipParSetupField setup_field;
    uint8_t setup_name_index;

    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t scroll_hole_offset;
    uint8_t scroll_row_offset;

    char text_input_buffer[FLIPPAR_NAME_LEN];
    uint8_t editing_player_index;
    bool confirm_new_game_yes;
    bool last_save_success;
    char last_save_filename[48];
    bool show_grid_lines;
    FlipParScreen splash_target_screen;

    bool running;
} FlipParApp;

static FlipParApp* g_flippar_app = NULL;

static void flippar_finish_splash(FlipParApp* app) {
    if(!app || app->screen != FlipParScreenSplash) return;

    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
    }

    app->screen = app->splash_target_screen;
}

static void flippar_set_save_result(FlipParApp* app, bool success, const char* path) {
    app->last_save_success = success;
    memset(app->last_save_filename, 0, sizeof(app->last_save_filename));

    if(path) {
        const char* filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;
        strncpy(app->last_save_filename, filename, sizeof(app->last_save_filename) - 1);
    }

    app->screen = FlipParScreenSaveResult;
}

static void flippar_splash_timer_callback(void* context) {
    FlipParApp* app = context;

    if(app && app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipParCustomEventSplashDone);
    }
}

static bool flippar_custom_event_callback(void* context, uint32_t event) {
    FlipParApp* app = context;

    if(event == FlipParCustomEventSplashDone) {
        flippar_finish_splash(app);
        return true;
    }

    return false;
}

static void flippar_init_data(FlipParApp* app) {
    app->holes = 18;
    app->players = 2;

    memset(app->player_names, 0, sizeof(app->player_names));
    for(uint8_t p = 0; p < FLIPPAR_MAX_PLAYERS; p++) {
        snprintf(app->player_names[p], FLIPPAR_NAME_LEN, "P%u", p + 1);
    }

    for(uint8_t h = 0; h < FLIPPAR_MAX_HOLES; h++) {
        app->pars[h] = 3;
        for(uint8_t p = 0; p < FLIPPAR_MAX_PLAYERS; p++) {
            app->scores[p][h] = 0;
        }
    }

    app->screen = FlipParScreenSplash;
    app->setup_field = FlipParFieldHoles;
    app->setup_name_index = 0;
    app->selected_row = 1;
    app->selected_col = 0;
    app->scroll_hole_offset = 0;
    app->scroll_row_offset = 0;
    app->editing_player_index = 0;
    memset(app->text_input_buffer, 0, sizeof(app->text_input_buffer));
    memset(app->last_save_filename, 0, sizeof(app->last_save_filename));
    app->last_save_success = false;
    app->show_grid_lines = false;
    app->splash_target_screen = FlipParScreenSetup;
    app->running = true;
}

static void flippar_normalize_state(FlipParApp* app) {
    if(app->players < 1) {
        app->players = 1;
    } else if(app->players > FLIPPAR_MAX_PLAYERS) {
        app->players = FLIPPAR_MAX_PLAYERS;
    }

    if(app->holes < 1) {
        app->holes = 1;
    } else if(app->holes > FLIPPAR_MAX_HOLES) {
        app->holes = FLIPPAR_MAX_HOLES;
    }

    if(app->setup_name_index >= app->players) {
        app->setup_name_index = app->players - 1;
    }
    if(app->selected_row > app->players) {
        app->selected_row = app->players;
    }
    if(app->selected_col >= app->holes) {
        app->selected_col = app->holes - 1;
    }
}

static int16_t flippar_total_for_player(FlipParApp* app, uint8_t player) {
    int16_t total = 0;
    for(uint8_t h = 0; h < app->holes; h++) {
        total += app->scores[player][h];
    }
    return total;
}

static int16_t flippar_total_par(FlipParApp* app) {
    int16_t total = 0;
    for(uint8_t h = 0; h < app->holes; h++) {
        total += app->pars[h];
    }
    return total;
}

static void flippar_format_relative_score(int16_t score, int16_t par_total, char* out, size_t out_size) {
    int16_t delta = score - par_total;

    if(delta == 0) {
        snprintf(out, out_size, "E");
    } else {
        snprintf(out, out_size, "%+d", delta);
    }
}

static uint8_t flippar_find_winner(FlipParApp* app) {
    uint8_t winner = 0;
    int16_t best_score = flippar_total_for_player(app, 0);

    for(uint8_t p = 1; p < app->players; p++) {
        int16_t score = flippar_total_for_player(app, p);
        if(score < best_score) {
            best_score = score;
            winner = p;
        }
    }

    return winner;
}

static uint8_t flippar_longest_player_name_length(FlipParApp* app) {
    uint8_t longest = strlen("Par");

    for(uint8_t p = 0; p < app->players; p++) {
        uint8_t length = strlen(app->player_names[p]);
        if(length > longest) {
            longest = length;
        }
    }

    return longest;
}

static bool flippar_write_text(File* file, const char* text) {
    size_t len = strlen(text);
    return storage_file_write(file, text, len) == len;
}

static bool flippar_write_char_repeat(File* file, char ch, size_t count) {
    char buffer[32];
    if(count >= sizeof(buffer)) return false;

    for(size_t i = 0; i < count; i++) {
        buffer[i] = ch;
    }
    buffer[count] = '\0';

    return flippar_write_text(file, buffer);
}

static bool flippar_write_score_card_separator(File* file, uint8_t players) {
    if(!flippar_write_text(file, "+")) return false;
    if(!flippar_write_char_repeat(file, '-', FLIPPAR_SCORE_CARD_HOLE_WIDTH + 2)) return false;
    if(!flippar_write_text(file, "+")) return false;
    if(!flippar_write_char_repeat(file, '-', FLIPPAR_SCORE_CARD_PAR_WIDTH + 2)) return false;
    if(!flippar_write_text(file, "+")) return false;
    for(uint8_t column = 0; column < players; column++) {
        UNUSED(column);
        if(!flippar_write_char_repeat(file, '-', FLIPPAR_SCORE_CARD_PLAYER_WIDTH + 2)) return false;
        if(!flippar_write_text(file, "+")) return false;
    }

    return flippar_write_text(file, "\r\n");
}

static bool flippar_write_score_card_cell(
    File* file,
    const char* text,
    size_t width,
    bool left_align) {
    char format[16];
    char buffer[32];

    snprintf(format, sizeof(format), left_align ? " %%-%us |" : " %%%us |", (unsigned)width);
    snprintf(buffer, sizeof(buffer), format, text);

    return flippar_write_text(file, buffer);
}

static bool flippar_write_score_card_number(File* file, int16_t value, size_t width) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return flippar_write_score_card_cell(file, buffer, width, false);
}

static bool flippar_save_current_state(FlipParApp* app) {
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_simply_mkdir(storage, FLIPPAR_SAVE_DIR)) {
        goto cleanup;
    }

    if(!storage_file_open(file, FLIPPAR_STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        goto cleanup;
    }

    FlipParScreen persisted_screen =
        (app->screen == FlipParScreenSplash) ? app->splash_target_screen : app->screen;

    FlipParPersistedStateV3 state = {
        .magic = FLIPPAR_STATE_MAGIC,
        .version = FLIPPAR_STATE_VERSION,
        .holes = app->holes,
        .players = app->players,
        .screen = persisted_screen == FlipParScreenConfirmNewGame ? FlipParScreenSetup :
                                                                   persisted_screen,
        .setup_field = app->setup_field,
        .setup_name_index = app->setup_name_index,
        .selected_row = app->selected_row,
        .selected_col = app->selected_col,
        .scroll_hole_offset = app->scroll_hole_offset,
        .scroll_row_offset = app->scroll_row_offset,
        .show_grid_lines = app->show_grid_lines,
    };

    memcpy(state.player_names, app->player_names, sizeof(state.player_names));
    memcpy(state.pars, app->pars, sizeof(state.pars));
    memcpy(state.scores, app->scores, sizeof(state.scores));

    if(storage_file_write(file, &state, sizeof(state)) != sizeof(state)) {
        goto close_file;
    }

    if(!storage_file_sync(file)) {
        goto close_file;
    }

    success = true;

close_file:
    storage_file_close(file);

cleanup:
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static bool flippar_load_current_state(FlipParApp* app) {
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FlipParPersistedState state_v2;
    FlipParPersistedStateV3 state_v3;
    uint32_t magic = 0;
    uint16_t version = 0;

    memset(&state_v2, 0, sizeof(state_v2));
    memset(&state_v3, 0, sizeof(state_v3));

    if(!storage_file_exists(storage, FLIPPAR_STATE_PATH)) {
        goto cleanup;
    }

    if(!storage_file_open(file, FLIPPAR_STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        goto cleanup;
    }

    if(storage_file_read(file, &magic, sizeof(magic)) != sizeof(magic)) {
        goto close_file;
    }

    if(storage_file_read(file, &version, sizeof(version)) != sizeof(version)) {
        goto close_file;
    }

    if(magic != FLIPPAR_STATE_MAGIC) {
        goto close_file;
    }

    storage_file_seek(file, 0, true);

    if(version == 2) {
        if(storage_file_read(file, &state_v2, sizeof(state_v2)) != sizeof(state_v2)) {
            goto close_file;
        }

        if(state_v2.holes < 1 || state_v2.holes > FLIPPAR_MAX_HOLES) goto close_file;
        if(state_v2.players < 1 || state_v2.players > FLIPPAR_MAX_PLAYERS) goto close_file;
        if(state_v2.setup_field > FlipParFieldNewGame + 1) goto close_file;
        if(state_v2.setup_name_index >= state_v2.players) goto close_file;
        if(state_v2.selected_row > state_v2.players) goto close_file;
        if(state_v2.selected_col >= state_v2.holes) goto close_file;
        if(state_v2.scroll_hole_offset > state_v2.holes) goto close_file;
        if(state_v2.scroll_row_offset > state_v2.players) goto close_file;

        app->holes = state_v2.holes;
        app->players = state_v2.players;
        memcpy(app->player_names, state_v2.player_names, sizeof(app->player_names));
        memcpy(app->pars, state_v2.pars, sizeof(app->pars));
        memcpy(app->scores, state_v2.scores, sizeof(app->scores));
        app->splash_target_screen =
            (state_v2.screen == FlipParScreenGrid) ? FlipParScreenGrid : FlipParScreenSetup;
        app->setup_field =
            (state_v2.setup_field == (FlipParFieldNewGame + 1)) ? FlipParFieldSave :
                                                                  state_v2.setup_field;
        app->setup_name_index = state_v2.setup_name_index;
        app->selected_row = state_v2.selected_row;
        app->selected_col = state_v2.selected_col;
        app->scroll_hole_offset = state_v2.scroll_hole_offset;
        app->scroll_row_offset = state_v2.scroll_row_offset;
        app->show_grid_lines = false;
    } else if(version == FLIPPAR_STATE_VERSION) {
        if(storage_file_read(file, &state_v3, sizeof(state_v3)) != sizeof(state_v3)) {
            goto close_file;
        }

        if(state_v3.holes < 1 || state_v3.holes > FLIPPAR_MAX_HOLES) goto close_file;
        if(state_v3.players < 1 || state_v3.players > FLIPPAR_MAX_PLAYERS) goto close_file;
        if(state_v3.setup_field > FlipParFieldSave) goto close_file;
        if(state_v3.setup_name_index >= state_v3.players) goto close_file;
        if(state_v3.selected_row > state_v3.players) goto close_file;
        if(state_v3.selected_col >= state_v3.holes) goto close_file;
        if(state_v3.scroll_hole_offset > state_v3.holes) goto close_file;
        if(state_v3.scroll_row_offset > state_v3.players) goto close_file;

        app->holes = state_v3.holes;
        app->players = state_v3.players;
        memcpy(app->player_names, state_v3.player_names, sizeof(app->player_names));
        memcpy(app->pars, state_v3.pars, sizeof(app->pars));
        memcpy(app->scores, state_v3.scores, sizeof(app->scores));
        app->splash_target_screen =
            (state_v3.screen == FlipParScreenGrid) ? FlipParScreenGrid : FlipParScreenSetup;
        app->setup_field = state_v3.setup_field;
        app->setup_name_index = state_v3.setup_name_index;
        app->selected_row = state_v3.selected_row;
        app->selected_col = state_v3.selected_col;
        app->scroll_hole_offset = state_v3.scroll_hole_offset;
        app->scroll_row_offset = state_v3.scroll_row_offset;
        app->show_grid_lines = state_v3.show_grid_lines;
    } else {
        goto close_file;
    }

    app->confirm_new_game_yes = false;
    flippar_normalize_state(app);
    success = true;

close_file:
    storage_file_close(file);

cleanup:
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static void flippar_reset_scorecard(FlipParApp* app) {
    for(uint8_t h = 0; h < FLIPPAR_MAX_HOLES; h++) {
        app->pars[h] = 3;
        for(uint8_t p = 0; p < FLIPPAR_MAX_PLAYERS; p++) {
            app->scores[p][h] = 0;
        }
    }

    app->screen = FlipParScreenSetup;
    app->setup_field = FlipParFieldStart;
    app->selected_row = 1;
    app->selected_col = 0;
    app->scroll_hole_offset = 0;
    app->scroll_row_offset = 0;
    app->confirm_new_game_yes = false;
    flippar_normalize_state(app);
}

static bool flippar_build_save_path(Storage* storage, char* path, size_t path_size) {
    DateTime datetime = {0};
    furi_hal_rtc_get_datetime(&datetime);

    char base_name[64];
    if(datetime.year > 0) {
        snprintf(
            base_name,
            sizeof(base_name),
            "%s_%u-%u-%u_%02u-%02u",
            FLIPPAR_SAVE_BASENAME,
            datetime.year,
            datetime.month,
            datetime.day,
            datetime.hour,
            datetime.minute);
    } else {
        snprintf(base_name, sizeof(base_name), "%s_unknown-date", FLIPPAR_SAVE_BASENAME);
    }

    snprintf(path, path_size, "%s/%s.txt", FLIPPAR_SAVE_DIR, base_name);
    if(!storage_file_exists(storage, path)) {
        return true;
    }

    for(uint8_t index = 2; index < 100; index++) {
        snprintf(path, path_size, "%s/%s_%02u.txt", FLIPPAR_SAVE_DIR, base_name, index);
        if(!storage_file_exists(storage, path)) {
            return true;
        }
    }

    path[0] = '\0';
    return false;
}

static bool flippar_save_score_sheet(FlipParApp* app) {
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char path[128] = {0};

    if(!storage_simply_mkdir(storage, FLIPPAR_SAVE_DIR)) {
        goto cleanup;
    }

    if(!flippar_build_save_path(storage, path, sizeof(path))) {
        goto cleanup;
    }

    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        goto cleanup;
    }

    char line[128];
    int16_t par_total = flippar_total_par(app);

    snprintf(line, sizeof(line), "FlipPar by ConsultingJoe\r\n");
    if(!flippar_write_text(file, line)) goto close_file;

    snprintf(line, sizeof(line), "Players:\t%u\r\nHoles:\t%u\r\n", app->players, app->holes);
    if(!flippar_write_text(file, line)) goto close_file;

    snprintf(line, sizeof(line), "Par Total:\t%d\r\n\r\n", par_total);
    if(!flippar_write_text(file, line)) goto close_file;

    if(!flippar_write_score_card_separator(file, app->players)) goto close_file;
    if(!flippar_write_text(file, "|")) goto close_file;
    if(!flippar_write_score_card_cell(file, "Hole", FLIPPAR_SCORE_CARD_HOLE_WIDTH, true)) {
        goto close_file;
    }
    if(!flippar_write_score_card_cell(file, "Par", FLIPPAR_SCORE_CARD_PAR_WIDTH, true)) {
        goto close_file;
    }
    for(uint8_t p = 0; p < app->players; p++) {
        if(!flippar_write_score_card_cell(
               file, app->player_names[p], FLIPPAR_SCORE_CARD_PLAYER_WIDTH, true)) {
            goto close_file;
        }
    }
    if(!flippar_write_text(file, "\r\n")) goto close_file;
    if(!flippar_write_score_card_separator(file, app->players)) goto close_file;

    for(uint8_t h = 0; h < app->holes; h++) {
        if(!flippar_write_text(file, "|")) goto close_file;
        if(!flippar_write_score_card_number(file, h + 1, FLIPPAR_SCORE_CARD_HOLE_WIDTH)) {
            goto close_file;
        }
        if(!flippar_write_score_card_number(file, app->pars[h], FLIPPAR_SCORE_CARD_PAR_WIDTH)) {
            goto close_file;
        }
        for(uint8_t p = 0; p < app->players; p++) {
            if(!flippar_write_score_card_number(
                   file, app->scores[p][h], FLIPPAR_SCORE_CARD_PLAYER_WIDTH)) {
                goto close_file;
            }
        }
        if(!flippar_write_text(file, "\r\n")) goto close_file;
        if(!flippar_write_score_card_separator(file, app->players)) goto close_file;
    }

    if(!flippar_write_text(file, "|")) goto close_file;
    if(!flippar_write_score_card_cell(file, "Total", FLIPPAR_SCORE_CARD_HOLE_WIDTH, true)) {
        goto close_file;
    }
    if(!flippar_write_score_card_number(file, par_total, FLIPPAR_SCORE_CARD_PAR_WIDTH)) {
        goto close_file;
    }
    for(uint8_t p = 0; p < app->players; p++) {
        if(!flippar_write_score_card_number(
               file, flippar_total_for_player(app, p), FLIPPAR_SCORE_CARD_PLAYER_WIDTH)) {
            goto close_file;
        }
    }
    if(!flippar_write_text(file, "\r\n")) goto close_file;
    if(!flippar_write_score_card_separator(file, app->players)) goto close_file;

    if(!storage_file_sync(file)) goto close_file;
    success = true;

close_file:
    storage_file_close(file);

cleanup:
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    flippar_set_save_result(app, success, success ? path : NULL);
    return success;
}

static void flippar_adjust_selected_value(FlipParApp* app, int8_t delta) {
    uint8_t* target = NULL;

    if(app->selected_col >= app->holes) return;
    if(app->selected_row > app->players) return;

    if(app->selected_row == 0) {
        target = &app->pars[app->selected_col];
    } else {
        target = &app->scores[app->selected_row - 1][app->selected_col];
    }

    int16_t value = *target;
    value += delta;

    if(value < 0) value = 0;
    if(value > FLIPPAR_MAX_STROKES) value = FLIPPAR_MAX_STROKES;

    *target = (uint8_t)value;
    flippar_save_current_state(app);
}

static void flippar_toggle_grid_lines(FlipParApp* app) {
    app->show_grid_lines = !app->show_grid_lines;
    flippar_save_current_state(app);
}

static void flippar_draw_scorecard_grid_lines(
    Canvas* canvas,
    uint8_t start_x,
    uint8_t start_y,
    uint8_t name_col_w,
    uint8_t visible_cols,
    uint8_t visible_rows,
    uint8_t col_w,
    uint8_t row_h) {
    const uint8_t grid_left = start_x - 1;
    const uint8_t grid_top = start_y + 1;
    const uint8_t header_h = row_h;
    const uint8_t grid_right = grid_left + name_col_w + (visible_cols * col_w);
    const uint8_t grid_bottom = grid_top + header_h + (visible_rows * row_h);

    canvas_draw_frame(canvas, grid_left, grid_top, grid_right - grid_left + 1, grid_bottom - grid_top + 1);

    const uint8_t name_divider_x = grid_left + name_col_w;
    canvas_draw_line(canvas, name_divider_x, grid_top, name_divider_x, grid_bottom);

    for(uint8_t i = 1; i < visible_cols; i++) {
        const uint8_t x = name_divider_x + (i * col_w);
        canvas_draw_line(canvas, x, grid_top, x, grid_bottom);
    }

    const uint8_t header_divider_y = grid_top + header_h;
    canvas_draw_line(canvas, grid_left, header_divider_y, grid_right, header_divider_y);

    for(uint8_t i = 1; i < visible_rows; i++) {
        const uint8_t y = header_divider_y + (i * row_h);
        canvas_draw_line(canvas, grid_left, y, grid_right, y);
    }
}

static void flippar_draw_setup(Canvas* canvas, FlipParApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "FlipPar Setup");

    canvas_set_font(canvas, FontSecondary);

    char line[64];

    const uint8_t visible_items = 5;
    uint8_t scroll_offset = 0;
    if(app->setup_field >= visible_items) {
        scroll_offset = app->setup_field - visible_items + 1;
    }

    for(uint8_t i = 0; i < visible_items; i++) {
        uint8_t field = scroll_offset + i;
        if(field > FlipParFieldSave) break;

        uint8_t y = 18 + (i * 8);
        if(field == FlipParFieldHoles) {
            snprintf(
                line,
                sizeof(line),
                "%c Holes: %u",
                app->setup_field == field ? '>' : ' ',
                app->holes);
        } else if(field == FlipParFieldPlayers) {
            snprintf(
                line,
                sizeof(line),
                "%c Players: %u",
                app->setup_field == field ? '>' : ' ',
                app->players);
        } else if(field == FlipParFieldNames) {
            snprintf(
                line,
                sizeof(line),
                "%c Name %u: %s",
                app->setup_field == field ? '>' : ' ',
                app->setup_name_index + 1,
                app->player_names[app->setup_name_index]);
        } else if(field == FlipParFieldStart) {
            snprintf(
                line,
                sizeof(line),
                "%c Start Round",
                app->setup_field == field ? '>' : ' ');
        } else if(field == FlipParFieldNewGame) {
            snprintf(
                line,
                sizeof(line),
                "%c New Game",
                app->setup_field == field ? '>' : ' ');
        } else if(field == FlipParFieldGridLines) {
            snprintf(
                line,
                sizeof(line),
                "%c Grid Lines %s",
                app->setup_field == field ? '>' : ' ',
                app->show_grid_lines ? "ON" : "OFF");
        } else {
            snprintf(
                line,
                sizeof(line),
                "%c Save Score Sheet",
                app->setup_field == field ? '>' : ' ');
        }

        canvas_draw_str(canvas, 2, y, line);
    }

    canvas_draw_line(canvas, 2, 54, 126, 54);
    canvas_draw_str(canvas, 2, 62, "By ConsultingJoe.com");
}

static void flippar_draw_splash(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_splash_128x64);
}

static void flippar_draw_new_game_confirm(Canvas* canvas, FlipParApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "New Game?");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, "Scorecard will be");
    canvas_draw_str(canvas, 2, 36, "cleared. Continue?");

    const char* yes_label = app->confirm_new_game_yes ? "> Yes <" : "  Yes  ";
    const char* no_label = app->confirm_new_game_yes ? "  No  " : "> No <";
    canvas_draw_str(canvas, 20, 56, yes_label);
    canvas_draw_str(canvas, 74, 56, no_label);
}

static void flippar_draw_save_result(Canvas* canvas, FlipParApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, app->last_save_success ? "Scorecard Saved" : "Save Failed");

    canvas_set_font(canvas, FontSecondary);
    if(app->last_save_success) {
        char first_line[21] = {0};
        char second_line[21] = {0};
        size_t filename_len = strlen(app->last_save_filename);

        strncpy(first_line, app->last_save_filename, sizeof(first_line) - 1);
        if(filename_len > sizeof(first_line) - 1) {
            strncpy(
                second_line,
                app->last_save_filename + (sizeof(first_line) - 1),
                sizeof(second_line) - 1);
        }

        canvas_draw_str(canvas, 2, 28, "Saved as:");
        canvas_draw_str(canvas, 2, 40, first_line);
        if(second_line[0] != '\0') {
            canvas_draw_str(canvas, 2, 50, second_line);
        }
    } else {
        canvas_draw_str(canvas, 2, 28, "Unable to write the");
        canvas_draw_str(canvas, 2, 40, "scorecard file.");
    }

    canvas_draw_str(canvas, 2, 62, "Press any key");
}

static void flippar_draw_scroll_arrow(Canvas* canvas, uint8_t x, uint8_t y, bool up) {
    if(up) {
        canvas_draw_line(canvas, x, y + 3, x + 2, y + 1);
        canvas_draw_line(canvas, x + 2, y + 1, x + 4, y + 3);
        canvas_draw_line(canvas, x + 2, y + 1, x + 2, y + 5);
    } else {
        canvas_draw_line(canvas, x, y + 1, x + 2, y + 3);
        canvas_draw_line(canvas, x + 2, y + 3, x + 4, y + 1);
        canvas_draw_line(canvas, x + 2, y + 3, x + 2, y - 1);
    }
}

static void flippar_draw_grid(Canvas* canvas, FlipParApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "FlipPar");

    const int16_t par_total = flippar_total_par(app);
    canvas_set_font(canvas, FontSecondary);
    if(app->players > 0) {
        uint8_t winner = flippar_find_winner(app);
        int16_t winner_score = flippar_total_for_player(app, winner);
        char relative_score[8];
        char summary[32];
        char short_name[7];

        memset(short_name, 0, sizeof(short_name));
        strncpy(short_name, app->player_names[winner], sizeof(short_name) - 1);
        flippar_format_relative_score(winner_score, par_total, relative_score, sizeof(relative_score));
        snprintf(summary, sizeof(summary), "%s  %s", short_name, relative_score);
        canvas_draw_str(canvas, 78, 10, summary);
    }

    const uint8_t grid_right_x = 120;
    const uint8_t visible_rows = 4;
    const uint8_t total_col = app->holes;
    const uint8_t total_columns = app->holes + 1;
    const uint8_t total_rows = app->players + 1;
    const uint8_t longest_name_len = flippar_longest_player_name_length(app);
    const uint8_t capped_name_len =
        (longest_name_len > FLIPPAR_GRID_MAX_NAME_CHARS) ? FLIPPAR_GRID_MAX_NAME_CHARS :
                                                           longest_name_len;
    uint8_t max_scroll_offset = 0;
    uint8_t max_row_scroll_offset = 0;

    const uint8_t start_x = 2;
    const uint8_t start_y = 14;
    const uint8_t col_w = FLIPPAR_GRID_SCORE_COL_STEP;
    const uint8_t row_h = 9;
    const uint8_t name_col_w =
        (capped_name_len * FLIPPAR_GRID_NAME_CHAR_WIDTH) + FLIPPAR_GRID_NAME_COL_PADDING;
    uint8_t visible_cols = 1;
    const uint8_t score_area_x = start_x + name_col_w;
    if(grid_right_x > (score_area_x + FLIPPAR_GRID_SCORE_CELL_WIDTH)) {
        visible_cols =
            1 + ((grid_right_x - (score_area_x + FLIPPAR_GRID_SCORE_CELL_WIDTH)) / col_w);
    }

    if(total_columns > visible_cols) {
        max_scroll_offset = total_columns - visible_cols;
    }
    if(total_rows > visible_rows) {
        max_row_scroll_offset = total_rows - visible_rows;
    }

    if(app->selected_col < app->scroll_hole_offset) {
        app->scroll_hole_offset = app->selected_col;
    }
    if(app->selected_col >= app->scroll_hole_offset + visible_cols) {
        app->scroll_hole_offset = app->selected_col - visible_cols + 1;
    }
    if(app->selected_col == (app->holes - 1) && app->scroll_hole_offset < max_scroll_offset) {
        app->scroll_hole_offset = max_scroll_offset;
    }
    if(app->scroll_hole_offset > max_scroll_offset) {
        app->scroll_hole_offset = max_scroll_offset;
    }

    if(app->selected_row < app->scroll_row_offset) {
        app->scroll_row_offset = app->selected_row;
    }
    if(app->selected_row >= app->scroll_row_offset + visible_rows) {
        app->scroll_row_offset = app->selected_row - visible_rows + 1;
    }
    if(app->scroll_row_offset > max_row_scroll_offset) {
        app->scroll_row_offset = max_row_scroll_offset;
    }

    if(app->show_grid_lines) {
        flippar_draw_scorecard_grid_lines(
            canvas,
            start_x,
            start_y,
            name_col_w,
            visible_cols,
            visible_rows,
            col_w,
            row_h);
    }

    char par_total_label[8];
    snprintf(par_total_label, sizeof(par_total_label), "%d", par_total);
    canvas_draw_str(canvas, start_x + 1, start_y + 9, par_total_label);

    for(uint8_t i = 0; i < visible_cols; i++) {
        uint8_t col = app->scroll_hole_offset + i;
        if(col >= total_columns) break;

        char header_label[8];
        if(col == total_col) {
            snprintf(header_label, sizeof(header_label), "TOT");
        } else {
            snprintf(header_label, sizeof(header_label), "H%u", col + 1);
        }
        canvas_draw_str(canvas, start_x + name_col_w + (i * col_w) + 1, start_y + 9, header_label);
    }

    for(uint8_t i = 0; i < visible_rows; i++) {
        uint8_t row = app->scroll_row_offset + i;
        uint8_t y = start_y + 12 + (i * row_h);

        if(row >= total_rows) break;

        if(row == 0) {
            canvas_draw_str(canvas, start_x + 1, y + 7, "Par");
        } else {
            char short_name[FLIPPAR_GRID_MAX_NAME_CHARS + 1];
            memset(short_name, 0, sizeof(short_name));
            strncpy(short_name, app->player_names[row - 1], sizeof(short_name) - 1);
            canvas_draw_str(canvas, start_x + 1, y + 7, short_name);
        }

        for(uint8_t i = 0; i < visible_cols; i++) {
            uint8_t col = app->scroll_hole_offset + i;
            if(col >= total_columns) break;

            uint8_t x = start_x + name_col_w + (i * col_w);

            char value[8];
            if(col == total_col) {
                int16_t total = (row == 0) ? par_total : flippar_total_for_player(app, row - 1);
                snprintf(value, sizeof(value), "%d", total);
            } else {
                uint8_t v = (row == 0) ? app->pars[col] : app->scores[row - 1][col];
                snprintf(value, sizeof(value), "%u", v);
            }

            bool selected = (col < app->holes) && (app->selected_row == row) &&
                            (app->selected_col == col);
            if(selected) {
                const uint8_t frame_x = x - 1;
                const uint8_t frame_y = y - 1;
                const uint8_t frame_w = col_w;
                const uint8_t frame_h = row_h + 1;
                const uint8_t frame_right = frame_x + frame_w - 1;
                const uint8_t frame_bottom = frame_y + frame_h - 1;

                canvas_draw_line(canvas, frame_x + 1, frame_y, frame_right, frame_y);
                canvas_draw_line(canvas, frame_x + 1, frame_y, frame_x + 1, frame_bottom);
                canvas_draw_line(canvas, frame_right, frame_y, frame_right, frame_bottom);
                canvas_draw_line(canvas, frame_x + 1, frame_bottom, frame_right, frame_bottom);
            }

            canvas_draw_str(canvas, x + 2, y + 7, value);
        }
    }

    if(app->scroll_row_offset > 0) {
        flippar_draw_scroll_arrow(canvas, 122, 14, true);
    }
    if(app->scroll_row_offset < max_row_scroll_offset) {
        flippar_draw_scroll_arrow(canvas, 122, 60, false);
    }
}

static void flippar_main_view_draw(Canvas* canvas, void* model) {
    UNUSED(model);

    FlipParApp* app = g_flippar_app;
    if(!app) return;

    if(app->screen == FlipParScreenSplash) {
        flippar_draw_splash(canvas);
    } else if(app->screen == FlipParScreenSetup) {
        flippar_draw_setup(canvas, app);
    } else if(app->screen == FlipParScreenConfirmNewGame) {
        flippar_draw_new_game_confirm(canvas, app);
    } else if(app->screen == FlipParScreenSaveResult) {
        flippar_draw_save_result(canvas, app);
    } else {
        flippar_draw_grid(canvas, app);
    }
}

static uint32_t flippar_main_view_previous(void* context) {
    FlipParApp* app = context;

    if(app->screen == FlipParScreenGrid) {
        app->screen = FlipParScreenSetup;
        flippar_save_current_state(app);
        return FlipParViewMain;
    }

    if(app->screen == FlipParScreenConfirmNewGame) {
        app->screen = FlipParScreenSetup;
        app->confirm_new_game_yes = false;
        return FlipParViewMain;
    }

    if(app->screen == FlipParScreenSaveResult) {
        app->screen = FlipParScreenSetup;
        return FlipParViewMain;
    }

    flippar_save_current_state(app);
    app->running = false;
    view_dispatcher_stop(app->view_dispatcher);
    return VIEW_NONE;
}

static void flippar_name_input_done(void* context) {
    FlipParApp* app = context;

    strncpy(
        app->player_names[app->editing_player_index],
        app->text_input_buffer,
        FLIPPAR_PLAYER_NAME_DISPLAY_LEN);
    app->player_names[app->editing_player_index][FLIPPAR_PLAYER_NAME_DISPLAY_LEN] = '\0';

    if(app->player_names[app->editing_player_index][0] == '\0') {
        snprintf(
            app->player_names[app->editing_player_index],
            FLIPPAR_NAME_LEN,
            "P%u",
            app->editing_player_index + 1);
    }

    flippar_save_current_state(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipParViewMain);
}

static void flippar_open_name_editor(FlipParApp* app, uint8_t player_index) {
    app->editing_player_index = player_index;

    memset(app->text_input_buffer, 0, sizeof(app->text_input_buffer));
    strncpy(
        app->text_input_buffer,
        app->player_names[player_index],
        FLIPPAR_NAME_LEN - 1);

    text_input_set_header_text(app->text_input, "Player Name");
    text_input_set_result_callback(
        app->text_input,
        flippar_name_input_done,
        app,
        app->text_input_buffer,
        FLIPPAR_PLAYER_NAME_DISPLAY_LEN + 1,
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipParViewTextInput);
}

static bool flippar_main_view_input(InputEvent* event, void* context) {
    FlipParApp* app = context;

    if(!app) return false;

    if(app->screen == FlipParScreenSplash) {
        if(event->type == InputTypeShort || event->type == InputTypeRepeat ||
           event->type == InputTypeLong) {
            flippar_finish_splash(app);
            return true;
        }
        return false;
    }

    if(app->screen == FlipParScreenSaveResult) {
        if(event->type == InputTypeShort || event->type == InputTypeRepeat ||
           event->type == InputTypeLong) {
            app->screen = FlipParScreenSetup;
            return true;
        }
        return false;
    }

    if(app->screen == FlipParScreenSetup) {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

        if(event->key == InputKeyUp) {
            if(app->setup_field > 0) app->setup_field--;
            flippar_save_current_state(app);
            return true;
        } else if(event->key == InputKeyDown) {
            if(app->setup_field < FlipParFieldSave) app->setup_field++;
            flippar_save_current_state(app);
            return true;
        } else if(event->key == InputKeyLeft) {
            if(app->setup_field == FlipParFieldHoles && app->holes > 1) {
                app->holes--;
            } else if(app->setup_field == FlipParFieldPlayers && app->players > 1) {
                app->players--;
                if(app->setup_name_index >= app->players) {
                    app->setup_name_index = app->players - 1;
                }
            } else if(app->setup_field == FlipParFieldNames && app->setup_name_index > 0) {
                app->setup_name_index--;
            } else if(app->setup_field == FlipParFieldGridLines) {
                flippar_toggle_grid_lines(app);
                return true;
            }
            flippar_normalize_state(app);
            flippar_save_current_state(app);
            return true;
        } else if(event->key == InputKeyRight) {
            if(app->setup_field == FlipParFieldHoles && app->holes < FLIPPAR_MAX_HOLES) {
                app->holes++;
            } else if(app->setup_field == FlipParFieldPlayers && app->players < FLIPPAR_MAX_PLAYERS) {
                app->players++;
            } else if(
                app->setup_field == FlipParFieldNames &&
                (app->setup_name_index + 1) < app->players) {
                app->setup_name_index++;
            } else if(app->setup_field == FlipParFieldGridLines) {
                flippar_toggle_grid_lines(app);
                return true;
            }
            flippar_normalize_state(app);
            flippar_save_current_state(app);
            return true;
        } else if(event->key == InputKeyOk) {
            if(app->setup_field == FlipParFieldNames) {
                flippar_open_name_editor(app, app->setup_name_index);
            } else if(app->setup_field == FlipParFieldStart) {
                app->screen = FlipParScreenGrid;
                app->selected_row = 1;
                app->selected_col = 0;
                app->scroll_row_offset = 0;
                app->scroll_hole_offset = 0;
                flippar_save_current_state(app);
            } else if(app->setup_field == FlipParFieldNewGame) {
                app->screen = FlipParScreenConfirmNewGame;
                app->confirm_new_game_yes = false;
            } else if(app->setup_field == FlipParFieldGridLines) {
                flippar_toggle_grid_lines(app);
            } else if(app->setup_field == FlipParFieldSave) {
                flippar_save_score_sheet(app);
            }
            return true;
        }
    } else if(app->screen == FlipParScreenConfirmNewGame) {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

        if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            app->confirm_new_game_yes = !app->confirm_new_game_yes;
            return true;
        } else if(event->key == InputKeyUp) {
            app->confirm_new_game_yes = true;
            return true;
        } else if(event->key == InputKeyDown) {
            app->confirm_new_game_yes = false;
            return true;
        } else if(event->key == InputKeyOk) {
            if(app->confirm_new_game_yes) {
                flippar_reset_scorecard(app);
                flippar_save_current_state(app);
            } else {
                app->screen = FlipParScreenSetup;
                app->confirm_new_game_yes = false;
            }
            return true;
        }
    } else {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
           event->type != InputTypeLong) {
            return false;
        }

        if(event->key == InputKeyLeft) {
            if(app->selected_col > 0) app->selected_col--;
            return true;
        } else if(event->key == InputKeyRight) {
            if(app->selected_col + 1 < app->holes) app->selected_col++;
            return true;
        } else if(event->key == InputKeyUp) {
            if(app->selected_row > 0) app->selected_row--;
            return true;
        } else if(event->key == InputKeyDown) {
            if(app->selected_row < app->players) app->selected_row++;
            return true;
        } else if(event->key == InputKeyOk) {
            if(event->type == InputTypeLong) {
                flippar_adjust_selected_value(app, -1);
                return true;
            } else if(event->type == InputTypeShort) {
                flippar_adjust_selected_value(app, 1);
                return true;
            }
            return false;
        }
    }

    return false;
}

int32_t flippar_app(void* p) {
    UNUSED(p);

    FlipParApp* app = malloc(sizeof(FlipParApp));
    if(!app) return -1;
    memset(app, 0, sizeof(FlipParApp));

    g_flippar_app = app;
    flippar_init_data(app);
    flippar_load_current_state(app);

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flippar_custom_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(FlipParViewModel));
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, flippar_main_view_draw);
    view_set_input_callback(app->main_view, flippar_main_view_input);
    view_set_previous_callback(app->main_view, flippar_main_view_previous);
    view_dispatcher_add_view(app->view_dispatcher, FlipParViewMain, app->main_view);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipParViewTextInput,
        text_input_get_view(app->text_input));

    app->splash_timer = furi_timer_alloc(flippar_splash_timer_callback, FuriTimerTypeOnce, app);
    if(app->splash_timer) {
        furi_timer_start(app->splash_timer, furi_ms_to_ticks(1700));
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipParViewMain);
    view_dispatcher_run(app->view_dispatcher);

    flippar_save_current_state(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipParViewTextInput);
    text_input_free(app->text_input);

    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
        furi_timer_free(app->splash_timer);
    }

    view_dispatcher_remove_view(app->view_dispatcher, FlipParViewMain);
    view_free(app->main_view);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    g_flippar_app = NULL;
    free(app);

    return 0;
}
