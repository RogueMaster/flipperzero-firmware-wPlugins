#include "settings.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"

#define SETTINGS_FILE "/ext/apps_data/osm_logger/settings.txt"
#define SETTINGS_MAX_SIZE 1024

const uint32_t BAUD_RATES[BAUD_RATE_COUNT] = {4800, 9600, 19200, 38400, 57600, 115200};
const char* const BAUD_LABELS[BAUD_RATE_COUNT] = {"4800", "9600", "19200", "38400", "57600", "115200"};

const uint8_t TRACK_INTERVALS_S[TRACK_INTERVAL_COUNT] = {1, 5, 10, 30, 60};
const char* const TRACK_INTERVAL_LABELS[TRACK_INTERVAL_COUNT] = {"1s", "5s", "10s", "30s", "60s"};

const uint8_t TRACK_MIN_DISTS_M[TRACK_MIN_DIST_COUNT] = {0, 2, 5, 10};
const char* const TRACK_MIN_DIST_LABELS[TRACK_MIN_DIST_COUNT] = {"off", "2m", "5m", "10m"};

const uint8_t DUPLICATE_CHECK_M[DUPLICATE_CHECK_COUNT] = {0, 5, 10, 25};
const char* const DUPLICATE_CHECK_LABELS[DUPLICATE_CHECK_COUNT] = {"off", "5m", "10m", "25m"};

const uint8_t AVG_SECONDS[AVG_SECONDS_COUNT] = {0, 5, 10, 30, 60};
const char* const AVG_SECONDS_LABELS[AVG_SECONDS_COUNT] = {"off", "5s", "10s", "30s", "60s"};

const uint8_t HDOP_MAX_X10[HDOP_MAX_COUNT] = {0, 25, 30, 50, 100};
const char* const HDOP_MAX_LABELS[HDOP_MAX_COUNT] = {"off", "2.5", "3.0", "5.0", "10"};

// --- Helpers
void settings_defaults(Settings* s) {
    s->baud_rate = 9600;
    s->track_interval_s = 5;
    s->track_min_dist_m = 0;
    s->track_hdop_strict = false;
    s->preview_before_save = false;
    s->auto_photo_id = false;
    s->duplicate_check_m = 10; // ON par défaut, seuil 10m (OSM-friendly)
    s->avg_seconds = 0;        // instantané par défaut (compat avec v0.9)
    s->hdop_max_x10 = 50;      // HDOP <= 5.0 par défaut (permissif, NEO-6M consumer)
    s->survey_mode = false;    // off par défaut (opt-in : norme OSM quand on vérifie sur place)
}

uint8_t baud_rate_to_idx(uint32_t baud) {
    for(uint8_t i = 0; i < BAUD_RATE_COUNT; i++) {
        if(BAUD_RATES[i] == baud) return i;
    }
    return 1; // 9600 par défaut
}

uint8_t track_interval_to_idx(uint8_t seconds) {
    for(uint8_t i = 0; i < TRACK_INTERVAL_COUNT; i++) {
        if(TRACK_INTERVALS_S[i] == seconds) return i;
    }
    return 1; // 5s par défaut
}

uint8_t track_min_dist_to_idx(uint8_t meters) {
    for(uint8_t i = 0; i < TRACK_MIN_DIST_COUNT; i++) {
        if(TRACK_MIN_DISTS_M[i] == meters) return i;
    }
    return 0;
}

uint8_t duplicate_check_to_idx(uint8_t meters) {
    for(uint8_t i = 0; i < DUPLICATE_CHECK_COUNT; i++) {
        if(DUPLICATE_CHECK_M[i] == meters) return i;
    }
    return 2; // 10m
}

uint8_t avg_seconds_to_idx(uint8_t seconds) {
    for(uint8_t i = 0; i < AVG_SECONDS_COUNT; i++) {
        if(AVG_SECONDS[i] == seconds) return i;
    }
    return 0;
}

uint8_t hdop_max_to_idx(uint8_t hdop_x10) {
    for(uint8_t i = 0; i < HDOP_MAX_COUNT; i++) {
        if(HDOP_MAX_X10[i] == hdop_x10) return i;
    }
    return 3; // 5.0
}

static uint32_t parse_uint(const char* s, size_t len) {
    uint32_t v = 0;
    for(size_t i = 0; i < len; i++) {
        char c = s[i];
        if(c >= '0' && c <= '9') v = v * 10 + (uint32_t)(c - '0');
        else break;
    }
    return v;
}

// Parse un buffer "key=value\n..." et remplit s.
static void parse_settings_buffer(char* buf, size_t size, Settings* s) {
    size_t i = 0;
    while(i < size) {
        // skip whitespace
        while(i < size && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) i++;
        if(i >= size) break;

        // commentaire ?
        if(buf[i] == '#') {
            while(i < size && buf[i] != '\n') i++;
            continue;
        }

        // key
        size_t key_start = i;
        while(i < size && buf[i] != '=' && buf[i] != '\n') i++;
        if(i >= size || buf[i] != '=') {
            while(i < size && buf[i] != '\n') i++;
            continue;
        }
        size_t key_end = i;
        i++; // skip '='

        // value
        size_t val_start = i;
        while(i < size && buf[i] != '\n' && buf[i] != '\r') i++;
        size_t val_end = i;

        size_t key_len = key_end - key_start;
        size_t val_len = val_end - val_start;

        if(key_len == 9 && !strncmp(&buf[key_start], "baud_rate", 9)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            // ne garde que les valeurs supportées
            for(uint8_t b = 0; b < BAUD_RATE_COUNT; b++) {
                if(BAUD_RATES[b] == v) {
                    s->baud_rate = v;
                    break;
                }
            }
        } else if(key_len == 16 && !strncmp(&buf[key_start], "track_interval_s", 16)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            for(uint8_t t = 0; t < TRACK_INTERVAL_COUNT; t++) {
                if(TRACK_INTERVALS_S[t] == v) {
                    s->track_interval_s = (uint8_t)v;
                    break;
                }
            }
        } else if(key_len == 16 && !strncmp(&buf[key_start], "track_min_dist_m", 16)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            for(uint8_t d = 0; d < TRACK_MIN_DIST_COUNT; d++) {
                if(TRACK_MIN_DISTS_M[d] == v) {
                    s->track_min_dist_m = (uint8_t)v;
                    break;
                }
            }
        } else if(key_len == 17 && !strncmp(&buf[key_start], "track_hdop_strict", 17)) {
            s->track_hdop_strict = parse_uint(&buf[val_start], val_len) != 0;
        } else if(key_len == 19 && !strncmp(&buf[key_start], "preview_before_save", 19)) {
            s->preview_before_save = parse_uint(&buf[val_start], val_len) != 0;
        } else if(key_len == 13 && !strncmp(&buf[key_start], "auto_photo_id", 13)) {
            s->auto_photo_id = parse_uint(&buf[val_start], val_len) != 0;
        } else if(key_len == 17 && !strncmp(&buf[key_start], "duplicate_check_m", 17)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            for(uint8_t d = 0; d < DUPLICATE_CHECK_COUNT; d++) {
                if(DUPLICATE_CHECK_M[d] == v) {
                    s->duplicate_check_m = (uint8_t)v;
                    break;
                }
            }
        } else if(key_len == 11 && !strncmp(&buf[key_start], "avg_seconds", 11)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            for(uint8_t a = 0; a < AVG_SECONDS_COUNT; a++) {
                if(AVG_SECONDS[a] == v) {
                    s->avg_seconds = (uint8_t)v;
                    break;
                }
            }
        } else if(key_len == 12 && !strncmp(&buf[key_start], "hdop_max_x10", 12)) {
            uint32_t v = parse_uint(&buf[val_start], val_len);
            for(uint8_t h = 0; h < HDOP_MAX_COUNT; h++) {
                if(HDOP_MAX_X10[h] == v) {
                    s->hdop_max_x10 = (uint8_t)v;
                    break;
                }
            }
        } else if(key_len == 11 && !strncmp(&buf[key_start], "survey_mode", 11)) {
            s->survey_mode = parse_uint(&buf[val_start], val_len) != 0;
        }
    }
}

void settings_load(Settings* s) {
    settings_defaults(s);

    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(f && storage_file_open(f, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0 && sz <= SETTINGS_MAX_SIZE) {
            char* buf = malloc((size_t)sz + 1);
            if(buf) {
                uint16_t read = storage_file_read(f, buf, (uint16_t)sz);
                buf[read] = '\0';
                parse_settings_buffer(buf, read, s);
                free(buf);
                FURI_LOG_I(
                    "OSM",
                    "Settings loaded: baud=%lu track_interval=%us",
                    (unsigned long)s->baud_rate,
                    (unsigned)s->track_interval_s);
            }
        }
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void settings_save(const Settings* s) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    FileInfo fi;
    if(storage_common_stat(st, "/ext/apps_data/osm_logger", &fi) != FSE_OK) {
        storage_common_mkdir(st, "/ext/apps_data/osm_logger");
    }

    File* f = storage_file_alloc(st);
    if(f && storage_file_open(f, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[512];
        int n = snprintf(
            buf,
            sizeof(buf),
            "# OSM Logger settings (auto-generated)\n"
            "baud_rate=%lu\n"
            "track_interval_s=%u\n"
            "track_min_dist_m=%u\n"
            "track_hdop_strict=%u\n"
            "preview_before_save=%u\n"
            "auto_photo_id=%u\n"
            "duplicate_check_m=%u\n"
            "avg_seconds=%u\n"
            "hdop_max_x10=%u\n"
            "survey_mode=%u\n",
            (unsigned long)s->baud_rate,
            (unsigned)s->track_interval_s,
            (unsigned)s->track_min_dist_m,
            s->track_hdop_strict ? 1u : 0u,
            s->preview_before_save ? 1u : 0u,
            s->auto_photo_id ? 1u : 0u,
            (unsigned)s->duplicate_check_m,
            (unsigned)s->avg_seconds,
            (unsigned)s->hdop_max_x10,
            s->survey_mode ? 1u : 0u);
        if(n > 0) storage_file_write(f, buf, (size_t)n);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// --- View
static void settings_baud_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= BAUD_RATE_COUNT) idx = 1;
    uint32_t new_baud = BAUD_RATES[idx];
    variable_item_set_current_value_text(item, BAUD_LABELS[idx]);

    if(new_baud != app->settings.baud_rate) {
        app->settings.baud_rate = new_baud;
        settings_save(&app->settings);
        app_reconfigure_uart(app);
    }
}

static void settings_interval_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= TRACK_INTERVAL_COUNT) idx = 1;
    uint8_t new_interval = TRACK_INTERVALS_S[idx];
    variable_item_set_current_value_text(item, TRACK_INTERVAL_LABELS[idx]);

    if(new_interval != app->settings.track_interval_s) {
        app->settings.track_interval_s = new_interval;
        settings_save(&app->settings);
    }
}

static void settings_min_dist_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= TRACK_MIN_DIST_COUNT) idx = 0;
    uint8_t new_dist = TRACK_MIN_DISTS_M[idx];
    variable_item_set_current_value_text(item, TRACK_MIN_DIST_LABELS[idx]);

    if(new_dist != app->settings.track_min_dist_m) {
        app->settings.track_min_dist_m = new_dist;
        settings_save(&app->settings);
    }
}

static const char* const ON_OFF[2] = {"off", "on"};

static void settings_hdop_strict_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool new_val = (idx != 0);
    variable_item_set_current_value_text(item, ON_OFF[new_val ? 1 : 0]);

    if(new_val != app->settings.track_hdop_strict) {
        app->settings.track_hdop_strict = new_val;
        settings_save(&app->settings);
    }
}

static void settings_preview_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool new_val = (idx != 0);
    variable_item_set_current_value_text(item, ON_OFF[new_val ? 1 : 0]);

    if(new_val != app->settings.preview_before_save) {
        app->settings.preview_before_save = new_val;
        settings_save(&app->settings);
    }
}

static void settings_auto_photo_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool new_val = (idx != 0);
    variable_item_set_current_value_text(item, ON_OFF[new_val ? 1 : 0]);

    if(new_val != app->settings.auto_photo_id) {
        app->settings.auto_photo_id = new_val;
        settings_save(&app->settings);
    }
}

static void settings_duplicate_check_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= DUPLICATE_CHECK_COUNT) idx = 0;
    uint8_t new_val = DUPLICATE_CHECK_M[idx];
    variable_item_set_current_value_text(item, DUPLICATE_CHECK_LABELS[idx]);

    if(new_val != app->settings.duplicate_check_m) {
        app->settings.duplicate_check_m = new_val;
        settings_save(&app->settings);
    }
}

static void settings_avg_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= AVG_SECONDS_COUNT) idx = 0;
    uint8_t new_val = AVG_SECONDS[idx];
    variable_item_set_current_value_text(item, AVG_SECONDS_LABELS[idx]);

    if(new_val != app->settings.avg_seconds) {
        app->settings.avg_seconds = new_val;
        settings_save(&app->settings);
    }
}

static void settings_hdop_max_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= HDOP_MAX_COUNT) idx = 0;
    uint8_t new_val = HDOP_MAX_X10[idx];
    variable_item_set_current_value_text(item, HDOP_MAX_LABELS[idx]);

    if(new_val != app->settings.hdop_max_x10) {
        app->settings.hdop_max_x10 = new_val;
        settings_save(&app->settings);
    }
}

static void settings_survey_mode_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool new_val = (idx != 0);
    variable_item_set_current_value_text(item, ON_OFF[new_val ? 1 : 0]);

    if(new_val != app->settings.survey_mode) {
        app->settings.survey_mode = new_val;
        settings_save(&app->settings);
    }
}

static uint32_t settings_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

void app_start_settings(App* app) {
    if(!app->settings_view) {
        app->settings_list = variable_item_list_alloc();

        VariableItem* item;

        item = variable_item_list_add(
            app->settings_list, "GPS baud", BAUD_RATE_COUNT, settings_baud_changed, app);
        uint8_t b_idx = baud_rate_to_idx(app->settings.baud_rate);
        variable_item_set_current_value_index(item, b_idx);
        variable_item_set_current_value_text(item, BAUD_LABELS[b_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "Track interval",
            TRACK_INTERVAL_COUNT,
            settings_interval_changed,
            app);
        uint8_t t_idx = track_interval_to_idx(app->settings.track_interval_s);
        variable_item_set_current_value_index(item, t_idx);
        variable_item_set_current_value_text(item, TRACK_INTERVAL_LABELS[t_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "Track min dist",
            TRACK_MIN_DIST_COUNT,
            settings_min_dist_changed,
            app);
        uint8_t d_idx = track_min_dist_to_idx(app->settings.track_min_dist_m);
        variable_item_set_current_value_index(item, d_idx);
        variable_item_set_current_value_text(item, TRACK_MIN_DIST_LABELS[d_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "Track HDOP strict",
            2,
            settings_hdop_strict_changed,
            app);
        variable_item_set_current_value_index(item, app->settings.track_hdop_strict ? 1 : 0);
        variable_item_set_current_value_text(
            item, ON_OFF[app->settings.track_hdop_strict ? 1 : 0]);

        item = variable_item_list_add(
            app->settings_list,
            "Preview save",
            2,
            settings_preview_changed,
            app);
        variable_item_set_current_value_index(item, app->settings.preview_before_save ? 1 : 0);
        variable_item_set_current_value_text(
            item, ON_OFF[app->settings.preview_before_save ? 1 : 0]);

        item = variable_item_list_add(
            app->settings_list,
            "Auto photo ID",
            2,
            settings_auto_photo_changed,
            app);
        variable_item_set_current_value_index(item, app->settings.auto_photo_id ? 1 : 0);
        variable_item_set_current_value_text(
            item, ON_OFF[app->settings.auto_photo_id ? 1 : 0]);

        item = variable_item_list_add(
            app->settings_list,
            "Dup check",
            DUPLICATE_CHECK_COUNT,
            settings_duplicate_check_changed,
            app);
        uint8_t dup_idx = duplicate_check_to_idx(app->settings.duplicate_check_m);
        variable_item_set_current_value_index(item, dup_idx);
        variable_item_set_current_value_text(item, DUPLICATE_CHECK_LABELS[dup_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "Averaging",
            AVG_SECONDS_COUNT,
            settings_avg_changed,
            app);
        uint8_t avg_idx = avg_seconds_to_idx(app->settings.avg_seconds);
        variable_item_set_current_value_index(item, avg_idx);
        variable_item_set_current_value_text(item, AVG_SECONDS_LABELS[avg_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "HDOP max",
            HDOP_MAX_COUNT,
            settings_hdop_max_changed,
            app);
        uint8_t hdop_idx = hdop_max_to_idx(app->settings.hdop_max_x10);
        variable_item_set_current_value_index(item, hdop_idx);
        variable_item_set_current_value_text(item, HDOP_MAX_LABELS[hdop_idx]);

        item = variable_item_list_add(
            app->settings_list,
            "Survey mode",
            2,
            settings_survey_mode_changed,
            app);
        variable_item_set_current_value_index(item, app->settings.survey_mode ? 1 : 0);
        variable_item_set_current_value_text(
            item, ON_OFF[app->settings.survey_mode ? 1 : 0]);

        app->settings_view = variable_item_list_get_view(app->settings_list);
        view_set_previous_callback(app->settings_view, settings_previous_callback);
        view_dispatcher_add_view(app->dispatcher, AppViewSettings, app->settings_view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewSettings);
}

void app_stop_settings(App* app) {
    if(app->settings_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewSettings);
        variable_item_list_free(app->settings_list);
        app->settings_list = NULL;
        app->settings_view = NULL;
    }
}
