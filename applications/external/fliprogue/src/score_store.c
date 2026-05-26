#include "score_store.h"
#include "game_core.h"
#include "run_report.h"
#include "ui_draw.h"

#include <storage/storage.h>

#define FR_SCORE_DIR EXT_PATH("apps_data/fliprogue")
#define FR_SCORE_PATH EXT_PATH("apps_data/fliprogue/hiscores.txt")
#define FR_SETTINGS_PATH EXT_PATH("apps_data/fliprogue/settings.txt")

static void ensure_score_dir(Storage* storage) {
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, FR_SCORE_DIR);
}

static bool read_line(File* file, char* out, size_t out_size) {
    if(out_size == 0) return false;
    size_t pos = 0;
    char c = '\0';
    bool any = false;
    while(storage_file_read(file, &c, 1) == 1) {
        any = true;
        if(c == '\n') break;
        if(pos + 1u < out_size) out[pos++] = c;
    }
    out[pos] = '\0';
    return any;
}

static void write_text(File* file, const char* text) {
    storage_file_write(file, text, strlen(text));
}

static void save_scores(AppContext* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    ensure_score_dir(storage);
    File* file = storage_file_alloc(storage);
    if(file && storage_file_open(file, FR_SCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < app->score_count; i++) {
            char line[32];
            char log_line[48];
            fr_score_format_main(&app->scores[i], (uint8_t)(i + 1), line, sizeof(line));
            fr_score_format_log(&app->scores[i], (uint8_t)(i + 1), log_line, sizeof(log_line));
            write_text(file, line);
            write_text(file, "\n");
            write_text(file, log_line);
            write_text(file, "\n\n");
        }
        storage_file_sync(file);
    }
    if(file) storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void load_scores(AppContext* app, Storage* storage) {
    File* file = storage_file_alloc(storage);
    if(!file) return;
    if(storage_file_open(file, FR_SCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char main_line[32];
        char log_line[48];
        memset(app->scores, 0, sizeof(app->scores));
        app->score_count = 0;
        while(app->score_count < SCORE_CAP && read_line(file, main_line, sizeof(main_line))) {
            if(main_line[0] == '\0') continue;
            ScoreEntry entry = {0};
            if(!fr_score_parse_main(main_line, &entry)) continue;
            if(read_line(file, log_line, sizeof(log_line))) fr_score_parse_log(log_line, &entry);
            fr_score_insert(app->scores, &app->score_count, &entry);
        }
    }
    storage_file_free(file);
}

static void load_settings(AppContext* app, Storage* storage) {
    File* file = storage_file_alloc(storage);
    if(!file) return;
    if(storage_file_open(file, FR_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[16];
        if(read_line(file, line, sizeof(line))) {
            if(strcmp(line, "sound=0") == 0) app->sound_enabled = false;
            else if(strcmp(line, "sound=1") == 0) app->sound_enabled = true;
        }
    }
    storage_file_free(file);
}

static void save_settings(AppContext* app, Storage* storage) {
    ensure_score_dir(storage);
    File* file = storage_file_alloc(storage);
    if(file && storage_file_open(file, FR_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        write_text(file, app->sound_enabled ? "sound=1\n" : "sound=0\n");
        storage_file_sync(file);
    }
    if(file) storage_file_free(file);
}

void load_score_store(AppContext* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    load_scores(app, storage);
    load_settings(app, storage);
    save_settings(app, storage);
    furi_record_close(RECORD_STORAGE);
}

void save_sound_setting(AppContext* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;
    save_settings(app, storage);
    furi_record_close(RECORD_STORAGE);
}

void record_score_if_done(AppContext* app) {
    if(app->score_recorded) return;
    if(app->game->mode != FR_MODE_GAME_OVER && app->game->mode != FR_MODE_VICTORY) return;

    ScoreEntry entry = {0};
    entry.victory = app->game->mode == FR_MODE_VICTORY ? 1 : 0;
    entry.floor = app->game->floor;
    entry.level = app->game->player.level;
    entry.has_orb = app->game->player.has_orb;
    entry.class_id = app->game->player.class_id;
    entry.gold = app->game->player.gold;
    const char* log = app->game->mode == FR_MODE_VICTORY ? "Escaped with the Orb." : fr_death_log(app->game);
    snprintf(entry.log, sizeof(entry.log), "%s", fr_last_log_phrases(log, 2));
    fr_score_insert(app->scores, &app->score_count, &entry);
    app->score_recorded = true;
    save_scores(app);
}

void draw_scores(Canvas* canvas, AppContext* app) {
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 4, "High Scores");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 2, 15, 124, 37);

    if(app->score_count == 0) {
        draw_text_center(canvas, 35, "No runs yet.");
        canvas_draw_str(canvas, 5, 61, "Back");
        return;
    }

    FrUiPage page = fr_ui_page(app->scroll, app->score_count, SCORE_VISIBLE_ROWS);
    app->scroll = page.first;

    for(uint8_t row = 0; row < SCORE_VISIBLE_ROWS && row + app->scroll < app->score_count; row++) {
        const ScoreEntry* score = &app->scores[row + app->scroll];
        char line[32];
        char log_line[48];
        uint8_t rank = (uint8_t)(row + app->scroll + 1);
        fr_score_format_main(score, rank, line, sizeof(line));
        fr_score_format_log(score, rank, log_line, sizeof(log_line));
        canvas_draw_str(canvas, 8, (uint8_t)(24 + row * 16), line);
        canvas_draw_str(canvas, 8, (uint8_t)(32 + row * 16), log_line);
    }

    char footer[24];
    snprintf(footer, sizeof(footer), "%u/%u", page.page, page.pages);
    canvas_draw_str(canvas, 5, 61, "Back");
    if(page.pages > 1) canvas_draw_str(canvas, 105, 61, footer);
}
