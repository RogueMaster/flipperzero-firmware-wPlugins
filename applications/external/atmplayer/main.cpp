#include <furi.h>

#include <gui/gui.h>
#include <gui/modules/file_browser.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/ATMlib.h"
#include "atm_icons.h"

#define ATM_TXT_MAGIC        "ATM1"
#define ATM_TXT_CMD_NAME     "NAME"
#define ATM_TXT_CMD_ENTRY    "ENTRY"
#define ATM_TXT_CMD_TRACK    "TRACK"
#define ATM_TXT_CMD_ENDTRACK "ENDTRACK"
#define ATM_TXT_CMD_END      "END"
#define ATM_TXT_COMMENT      '#'
#define ATM_TXT_SEPARATOR    ','

#define ATM_TXT_OP_DB                "DB"
#define ATM_TXT_OP_NOTE              "NOTE"
#define ATM_TXT_OP_DELAY             "DELAY"
#define ATM_TXT_OP_STOP              "STOP"
#define ATM_TXT_OP_RETURN            "RETURN"
#define ATM_TXT_OP_GOTO              "GOTO"
#define ATM_TXT_OP_REPEAT            "REPEAT"
#define ATM_TXT_OP_SET_TEMPO         "SET_TEMPO"
#define ATM_TXT_OP_ADD_TEMPO         "ADD_TEMPO"
#define ATM_TXT_OP_SET_VOLUME        "SET_VOLUME"
#define ATM_TXT_OP_VOLUME_SLIDE_ON   "VOLUME_SLIDE_ON"
#define ATM_TXT_OP_VOLUME_SLIDE_OFF  "VOLUME_SLIDE_OFF"
#define ATM_TXT_OP_SET_NOTE_CUT      "SET_NOTE_CUT"
#define ATM_TXT_OP_NOTE_CUT_OFF      "NOTE_CUT_OFF"
#define ATM_TXT_OP_SET_TRANSPOSITION "SET_TRANSPOSITION"
#define ATM_TXT_OP_TRANSPOSITION_OFF "TRANSPOSITION_OFF"
#define ATM_TXT_OP_GOTO_ADVANCED     "GOTO_ADVANCED"
#define ATM_TXT_OP_SET_VIBRATO       "SET_VIBRATO"

#define ATM_SONG_MAX_TEXT_SIZE (32 * 1024)
#define ATM_VOLUME_UNIT_STEP   0.1f
#define ATM_VOLUME_UNIT_MAX    8

typedef enum {
    AtmViewBrowser = 0,
    AtmViewPlayer,
} AtmView;

typedef enum {
    AtmEventFileSelected = 1,
    AtmEventOpenBrowser,
    AtmEventUiTick,
} AtmEvent;

typedef struct {
    char song_name[48];
    char state_line[24];
    uint8_t levels[4];
    int8_t volume_units;
    bool playing;
    bool paused;
    bool loaded;
} AtmPlayerModel;

typedef struct {
    const char* cur;
} AtmTokenizer;

typedef struct {
    uint8_t* bytes;
    size_t size;
    size_t capacity;
} ByteBuffer;

typedef struct {
    uint16_t* items;
    size_t size;
    size_t capacity;
} OffsetBuffer;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    FileBrowser* file_browser;
    View* player_view;
    FuriString* selected_path;

    bool browser_started;
    AtmView current_view;

    uint8_t* song_buf;
    size_t song_size;
    bool playing;
    bool paused;
    char song_name[48];
    FuriTimer* ui_timer;
    uint16_t ui_level_q8[4];
    uint8_t ui_dither_phase;
    int8_t volume_units;
} FlipperAtmApp;

static void atm_extract_file_name(const char* path, char* out, size_t out_size);
static bool atm_play_selected_file(FlipperAtmApp* app);
static bool atm_switch_track(FlipperAtmApp* app, int8_t step);
static bool atm_file_browser_item_callback(
    FuriString* path,
    void* context,
    uint8_t** icon,
    FuriString* item_name);

static bool atm_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char atm_char_upper(char c) {
    if(c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

static bool atm_str_contains_ci(const char* haystack, const char* needle) {
    if(!haystack || !needle || !needle[0]) return false;
    for(const char* h = haystack; *h; h++) {
        const char* hp = h;
        const char* np = needle;
        while(*hp && *np && atm_char_upper(*hp) == atm_char_upper(*np)) {
            hp++;
            np++;
        }
        if(*np == '\0') return true;
    }
    return false;
}

static bool atm_token_equals(const char* token, const char* keyword) {
    while(*token && *keyword) {
        if(atm_char_upper(*token) != atm_char_upper(*keyword)) return false;
        token++;
        keyword++;
    }
    return (*token == '\0') && (*keyword == '\0');
}

static void atm_set_player_status(
    FlipperAtmApp* app,
    const char* song_name,
    const char* state,
    bool loaded) {
    with_view_model_cpp(
        app->player_view,
        AtmPlayerModel*,
        model,
        {
            snprintf(
                model->song_name, sizeof(model->song_name), "%s", song_name ? song_name : "-");
            snprintf(model->state_line, sizeof(model->state_line), "%s", state ? state : "-");
            model->volume_units = app->volume_units;
            model->playing = app->playing;
            model->paused = app->paused;
            model->loaded = loaded;
        },
        true);
}

static void atm_update_levels(FlipperAtmApp* app) {
    uint8_t raw_levels[4] = {0, 0, 0, 0};
    uint8_t smooth_widths[4] = {0, 0, 0, 0};
    const uint16_t meter_max_level = 63;
    const uint16_t meter_inner_w = 120;
    atm_get_channel_levels(raw_levels);
    // raw_levels[3] = (uint8_t)(raw_levels[3] > 31 ? 63 : raw_levels[3] * 2);
    app->ui_dither_phase++;

    for(size_t i = 0; i < 4; i++) {
        uint16_t target_q8 = (uint16_t)raw_levels[i] << 8;
        uint16_t cur_q8 = app->ui_level_q8[i];

        if(target_q8 >= cur_q8) {
            cur_q8 = target_q8;
        } else {
            uint16_t delta = (uint16_t)(cur_q8 - target_q8);
            uint16_t decay = (uint16_t)((delta >> 3) + 1);
            cur_q8 = (cur_q8 > decay) ? (uint16_t)(cur_q8 - decay) : 0;
        }

        app->ui_level_q8[i] = cur_q8;

        uint32_t w_q8 = ((uint32_t)cur_q8 * meter_inner_w) / meter_max_level;
        uint8_t w = (uint8_t)(w_q8 >> 8);
        uint8_t frac = (uint8_t)(w_q8 & 0xFF);

        if(w < meter_inner_w && frac > app->ui_dither_phase) w++;
        smooth_widths[i] = w;
    }

    with_view_model_cpp(
        app->player_view,
        AtmPlayerModel*,
        model,
        {
            for(size_t i = 0; i < 4; i++) {
                model->levels[i] = smooth_widths[i];
            }
        },
        true);
}

static void atm_reset_ui_level_meters(FlipperAtmApp* app) {
    memset(app->ui_level_q8, 0, sizeof(app->ui_level_q8));
    app->ui_dither_phase = 0;
}

static void atm_set_playback_state(FlipperAtmApp* app) {
    atm_set_player_status(app, app->song_name, "", app->song_buf != NULL);
}

static void atm_apply_volume_units(FlipperAtmApp* app) {
    if(app->volume_units > ATM_VOLUME_UNIT_MAX) app->volume_units = ATM_VOLUME_UNIT_MAX;
    if(app->volume_units < -ATM_VOLUME_UNIT_MAX) app->volume_units = -ATM_VOLUME_UNIT_MAX;

    float gain = 1.0f + (float)app->volume_units * ATM_VOLUME_UNIT_STEP;
    if(gain < 0.0f) gain = 0.0f;
    ATM.setMasterVolume(gain);
}

static void atm_extract_file_name(const char* path, char* out, size_t out_size) {
    const char* file = strrchr(path, '/');
    file = file ? (file + 1) : path;

    snprintf(out, out_size, "%s", file);

    size_t len = strlen(out);
    if(len > 4 && out[len - 4] == '.' && atm_char_upper(out[len - 3]) == 'A' &&
       atm_char_upper(out[len - 2]) == 'T' && atm_char_upper(out[len - 1]) == 'M') {
        out[len - 4] = '\0';
    }
}

static bool atm_has_atm_ext(const char* name) {
    if(!name) return false;
    size_t len = strlen(name);
    if(len < 4) return false;
    return name[len - 4] == '.' && atm_char_upper(name[len - 3]) == 'A' &&
           atm_char_upper(name[len - 2]) == 'T' && atm_char_upper(name[len - 1]) == 'M';
}

static void atm_free_name_list(char** names, size_t count) {
    if(!names) return;
    for(size_t i = 0; i < count; i++) {
        if(names[i]) free(names[i]);
    }
    free(names);
}

static bool byte_buffer_push(ByteBuffer* b, uint8_t value) {
    if(b->size == b->capacity) {
        size_t next = (b->capacity == 0) ? 128 : (b->capacity * 2);
        uint8_t* n = (uint8_t*)realloc(b->bytes, next);
        if(!n) return false;
        b->bytes = n;
        b->capacity = next;
    }
    b->bytes[b->size++] = value;
    return true;
}

static bool byte_buffer_push_u8_from_i32(ByteBuffer* b, int32_t value) {
    return byte_buffer_push(b, (uint8_t)(value & 0xFF));
}

static bool byte_buffer_push_vle(ByteBuffer* b, uint32_t value) {
    uint8_t groups[5];
    size_t n = 0;

    do {
        groups[n++] = (uint8_t)(value & 0x7F);
        value >>= 7;
    } while(value && n < sizeof(groups));

    for(size_t i = n; i > 0; i--) {
        uint8_t out = groups[i - 1];
        if(i != 1) out |= 0x80;
        if(!byte_buffer_push(b, out)) return false;
    }

    return true;
}

static bool offset_buffer_push(OffsetBuffer* b, uint16_t value) {
    if(b->size == b->capacity) {
        size_t next = (b->capacity == 0) ? 16 : (b->capacity * 2);
        uint16_t* n = (uint16_t*)realloc(b->items, next * sizeof(uint16_t));
        if(!n) return false;
        b->items = n;
        b->capacity = next;
    }
    b->items[b->size++] = value;
    return true;
}

static bool atm_next_token(AtmTokenizer* tz, char* token, size_t token_size) {
    const char* p = tz->cur;

    while(*p) {
        if(*p == ATM_TXT_COMMENT) {
            while(*p && *p != '\n')
                p++;
            continue;
        }

        if(atm_is_space(*p) || *p == ATM_TXT_SEPARATOR) {
            p++;
            continue;
        }

        break;
    }

    if(!*p) {
        tz->cur = p;
        return false;
    }

    size_t n = 0;
    while(*p && !atm_is_space(*p) && (*p != ATM_TXT_SEPARATOR) && (*p != ATM_TXT_COMMENT)) {
        if((n + 1) < token_size) token[n++] = *p;
        p++;
    }

    token[n] = '\0';
    tz->cur = p;
    return n > 0;
}

static bool atm_parse_i32(const char* token, int32_t* out) {
    char* end = NULL;
    long value = strtol(token, &end, 0);
    if(!end || (*end != '\0')) return false;
    *out = (int32_t)value;
    return true;
}

static bool atm_parse_arg_i32(AtmTokenizer* tz, int32_t* out) {
    char token[32];
    if(!atm_next_token(tz, token, sizeof(token))) return false;
    return atm_parse_i32(token, out);
}

static bool atm_parse_name_line(AtmTokenizer* tz, char* out, size_t out_size) {
    if(!out || out_size == 0) return false;

    const char* p = tz->cur;
    while(*p == ' ' || *p == '\t' || *p == ATM_TXT_SEPARATOR)
        p++;

    if(*p == '\0' || *p == '\n' || *p == '\r' || *p == ATM_TXT_COMMENT) return false;

    size_t n = 0;
    while(*p && *p != '\n' && *p != '\r' && *p != ATM_TXT_COMMENT) {
        if((n + 1) < out_size) out[n++] = *p;
        p++;
    }

    while(n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t' || out[n - 1] == ATM_TXT_SEPARATOR))
        n--;
    out[n] = '\0';

    tz->cur = p;
    return n > 0;
}

static bool atm_emit_instruction(AtmTokenizer* tz, const char* op, ByteBuffer* data) {
    int32_t a = 0;
    int32_t b = 0;
    int32_t c = 0;
    int32_t d = 0;

    if(atm_token_equals(op, ATM_TXT_OP_DB)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_NOTE)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        if(a < 0 || a > 63) return false;
        return byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_DELAY)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        if(a < 1) return false;

        if(a <= 64) {
            return byte_buffer_push_u8_from_i32(data, 159 + a);
        } else {
            if(!byte_buffer_push(data, 224)) return false;
            return byte_buffer_push_vle(data, (uint32_t)(a - 65));
        }
    }

    if(atm_token_equals(op, ATM_TXT_OP_STOP)) {
        return byte_buffer_push(data, 0x9F);
    }

    if(atm_token_equals(op, ATM_TXT_OP_RETURN)) {
        return byte_buffer_push(data, 0xFE);
    }

    if(atm_token_equals(op, ATM_TXT_OP_GOTO)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0xFC) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_REPEAT)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        if(!atm_parse_arg_i32(tz, &b)) return false;
        return byte_buffer_push(data, 0xFD) && byte_buffer_push_u8_from_i32(data, a) &&
               byte_buffer_push_u8_from_i32(data, b);
    }

    if(atm_token_equals(op, ATM_TXT_OP_SET_TEMPO)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x9D) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_ADD_TEMPO)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x9C) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_SET_VOLUME)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x40) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_VOLUME_SLIDE_ON)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x41) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_VOLUME_SLIDE_OFF)) {
        return byte_buffer_push(data, 0x43);
    }

    if(atm_token_equals(op, ATM_TXT_OP_SET_NOTE_CUT)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x54) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_NOTE_CUT_OFF)) {
        return byte_buffer_push(data, 0x55);
    }

    if(atm_token_equals(op, ATM_TXT_OP_SET_TRANSPOSITION)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        return byte_buffer_push(data, 0x4C) && byte_buffer_push_u8_from_i32(data, a);
    }

    if(atm_token_equals(op, ATM_TXT_OP_TRANSPOSITION_OFF)) {
        return byte_buffer_push(data, 0x4D);
    }

    if(atm_token_equals(op, ATM_TXT_OP_GOTO_ADVANCED)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        if(!atm_parse_arg_i32(tz, &b)) return false;
        if(!atm_parse_arg_i32(tz, &c)) return false;
        if(!atm_parse_arg_i32(tz, &d)) return false;
        return byte_buffer_push(data, 0x9E) && byte_buffer_push_u8_from_i32(data, a) &&
               byte_buffer_push_u8_from_i32(data, b) && byte_buffer_push_u8_from_i32(data, c) &&
               byte_buffer_push_u8_from_i32(data, d);
    }

    if(atm_token_equals(op, ATM_TXT_OP_SET_VIBRATO)) {
        if(!atm_parse_arg_i32(tz, &a)) return false;
        if(!atm_parse_arg_i32(tz, &b)) return false;
        return byte_buffer_push(data, 0x4E) && byte_buffer_push_u8_from_i32(data, a) &&
               byte_buffer_push_u8_from_i32(data, b);
    }

    if(atm_parse_i32(op, &a)) {
        return byte_buffer_push_u8_from_i32(data, a);
    }

    return false;
}

static bool atm_parse_song_text(
    const char* text,
    uint8_t** out_buf,
    size_t* out_size,
    char* out_song_name,
    size_t out_song_name_size) {
    AtmTokenizer tz = {.cur = text};
    char token[64];

    uint8_t entry[4] = {0};
    int32_t value = 0;

    ByteBuffer data = {NULL, 0, 0};
    OffsetBuffer offsets = {NULL, 0, 0};

    uint8_t* song = NULL;
    size_t song_size = 0;
    size_t p = 0;
    bool ok = false;
    char ignored_song_name[2] = {0};
    char* song_name_dst = out_song_name ? out_song_name : ignored_song_name;
    size_t song_name_dst_size = out_song_name ? out_song_name_size : sizeof(ignored_song_name);

    if(song_name_dst_size > 0) song_name_dst[0] = '\0';

    if(!atm_next_token(&tz, token, sizeof(token)) || !atm_token_equals(token, ATM_TXT_MAGIC))
        goto out;

    if(!atm_next_token(&tz, token, sizeof(token))) goto out;
    if(atm_token_equals(token, ATM_TXT_CMD_NAME)) {
        if(!atm_parse_name_line(&tz, song_name_dst, song_name_dst_size)) goto out;
        if(!atm_next_token(&tz, token, sizeof(token))) goto out;
    }

    if(!atm_token_equals(token, ATM_TXT_CMD_ENTRY)) goto out;
    for(size_t i = 0; i < 4; i++) {
        if(!atm_parse_arg_i32(&tz, &value)) goto out;
        entry[i] = (uint8_t)(value & 0xFF);
    }

    while(atm_next_token(&tz, token, sizeof(token))) {
        if(atm_token_equals(token, ATM_TXT_CMD_END)) {
            break;
        }

        if(!atm_token_equals(token, ATM_TXT_CMD_TRACK)) goto out;

        if(!offset_buffer_push(&offsets, (uint16_t)data.size)) goto out;

        while(atm_next_token(&tz, token, sizeof(token))) {
            if(atm_token_equals(token, ATM_TXT_CMD_ENDTRACK)) break;
            if(!atm_emit_instruction(&tz, token, &data)) goto out;
        }

        if(!atm_token_equals(token, ATM_TXT_CMD_ENDTRACK)) goto out;
    }

    if(!atm_token_equals(token, ATM_TXT_CMD_END)) goto out;
    if(offsets.size == 0 || offsets.size > 255) goto out;

    song_size = 1 + offsets.size * 2 + 4 + data.size;
    song = (uint8_t*)malloc(song_size);
    if(!song) goto out;

    song[p++] = (uint8_t)offsets.size;
    for(size_t i = 0; i < offsets.size; i++) {
        uint16_t off = offsets.items[i];
        song[p++] = (uint8_t)(off & 0xFF);
        song[p++] = (uint8_t)((off >> 8) & 0xFF);
    }
    for(size_t i = 0; i < 4; i++) {
        song[p++] = entry[i];
    }
    memcpy(song + p, data.bytes, data.size);

    *out_buf = song;
    *out_size = song_size;
    song = NULL;
    ok = true;

out:
    if(song) free(song);
    if(data.bytes) free(data.bytes);
    if(offsets.items) free(offsets.items);
    return ok;
}

static bool atm_load_song_from_file(
    FlipperAtmApp* app,
    const char* path,
    char* out_song_name,
    size_t out_song_name_size) {
    bool ok = false;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        uint64_t file_size = storage_file_size(file);
        if(file_size == 0 || file_size > ATM_SONG_MAX_TEXT_SIZE) break;

        char* text = (char*)malloc((size_t)file_size + 1);
        if(!text) break;

        size_t read_total = 0;
        while(read_total < (size_t)file_size) {
            size_t r = storage_file_read(file, text + read_total, (size_t)file_size - read_total);
            if(r == 0) break;
            read_total += r;
        }
        text[read_total] = '\0';

        uint8_t* compiled = NULL;
        size_t compiled_size = 0;
        if(read_total == (size_t)file_size &&
           atm_parse_song_text(
               text, &compiled, &compiled_size, out_song_name, out_song_name_size)) {
            if(app->song_buf) free(app->song_buf);
            app->song_buf = compiled;
            app->song_size = compiled_size;
            ok = true;
        }

        free(text);
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool atm_play_selected_file(FlipperAtmApp* app) {
    const char* selected_path = furi_string_get_cstr(app->selected_path);
    char short_name[48];
    atm_extract_file_name(selected_path, short_name, sizeof(short_name));

    char song_name[48] = {0};
    if(atm_load_song_from_file(app, selected_path, song_name, sizeof(song_name))) {
        snprintf(
            app->song_name, sizeof(app->song_name), "%s", song_name[0] ? song_name : short_name);
        atm_reset_ui_level_meters(app);
        ATM.setUniformToneMode(atm_str_contains_ci(selected_path, "blheli32"));
        ATM.play(app->song_buf);
        app->playing = true;
        app->paused = false;
        atm_set_playback_state(app);
        return true;
    } else {
        snprintf(app->song_name, sizeof(app->song_name), "%s", short_name);
        atm_reset_ui_level_meters(app);
        ATM.stop();
        app->playing = false;
        app->paused = false;
        atm_set_player_status(app, app->song_name, "Load error", false);
        return false;
    }
}

static bool atm_switch_track(FlipperAtmApp* app, int8_t step) {
    const char* current_path = furi_string_get_cstr(app->selected_path);
    const char* slash = strrchr(current_path, '/');
    if(!slash) return false;

    size_t dir_len = (size_t)(slash - current_path);
    if(dir_len == 0 || dir_len >= 255) return false;

    char dir_path[256];
    memcpy(dir_path, current_path, dir_len);
    dir_path[dir_len] = '\0';

    const char* current_name = slash + 1;

    File* dir = storage_file_alloc(app->storage);
    if(!dir) return false;

    char** names = NULL;
    size_t names_count = 0;
    size_t names_cap = 0;

    bool ok = false;
    do {
        if(!storage_dir_open(dir, dir_path)) break;

        FileInfo info;
        char name[256];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!atm_has_atm_ext(name)) continue;

            if(names_count == names_cap) {
                size_t next_cap = names_cap ? names_cap * 2 : 16;
                char** next = (char**)realloc(names, next_cap * sizeof(char*));
                if(!next) break;
                names = next;
                names_cap = next_cap;
            }

            size_t nlen = strlen(name);
            char* cp = (char*)malloc(nlen + 1);
            if(!cp) break;
            memcpy(cp, name, nlen + 1);
            names[names_count++] = cp;
        }

        if(names_count == 0) break;

        for(size_t i = 1; i < names_count; i++) {
            char* key = names[i];
            size_t j = i;
            while(j > 0 && strcmp(names[j - 1], key) > 0) {
                names[j] = names[j - 1];
                j--;
            }
            names[j] = key;
        }

        size_t current_idx = 0;
        bool found = false;
        for(size_t i = 0; i < names_count; i++) {
            if(strcmp(names[i], current_name) == 0) {
                current_idx = i;
                found = true;
                break;
            }
        }
        if(!found) current_idx = 0;

        size_t next_idx;
        if(step > 0) {
            next_idx = (current_idx + 1) % names_count;
        } else {
            next_idx = (current_idx == 0) ? (names_count - 1) : (current_idx - 1);
        }

        char next_path[320];
        snprintf(next_path, sizeof(next_path), "%s/%s", dir_path, names[next_idx]);
        furi_string_set_str(app->selected_path, next_path);

        ok = atm_play_selected_file(app);
    } while(false);

    storage_file_close(dir);
    storage_file_free(dir);
    atm_free_name_list(names, names_count);
    return ok;
}

static void atm_file_selected_callback(void* context) {
    FlipperAtmApp* app = (FlipperAtmApp*)context;
    view_dispatcher_send_custom_event(app->dispatcher, AtmEventFileSelected);
}

static void atm_open_browser(FlipperAtmApp* app) {
    if(!app->browser_started) {
        file_browser_start(app->file_browser, app->selected_path);
        app->browser_started = true;
    }

    app->current_view = AtmViewBrowser;
    view_dispatcher_switch_to_view(app->dispatcher, AtmViewBrowser);
}

static void atm_ui_timer_callback(void* context) {
    FlipperAtmApp* app = (FlipperAtmApp*)context;
    view_dispatcher_send_custom_event(app->dispatcher, AtmEventUiTick);
}

static void atm_player_draw_callback(Canvas* canvas, void* model_ptr) {
    AtmPlayerModel* model = (AtmPlayerModel*)model_ptr;

    const uint8_t meter_x = 3;
    const uint8_t meter_inner_w = 120;
    const uint8_t meter_inner_h = 4;
    const uint8_t meter_frame = 1;
    const uint8_t meter_gap = 2;
    const uint8_t meter_outer_w = meter_inner_w + (meter_frame * 2);
    const uint8_t meter_outer_h = meter_inner_h + (meter_frame * 2);
    const uint8_t meter_top = 34;
    const uint8_t vol_x = 26;
    const uint8_t vol_y = 25;
    const uint8_t vol_w = 98;
    const uint8_t vol_h = 7;
    const uint8_t vol_inner_w = 96;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, model->song_name);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 31, "Vol:");

    canvas_draw_frame(canvas, vol_x, vol_y, vol_w, vol_h);
    const uint8_t center_x = (uint8_t)(vol_x + 1 + (vol_inner_w / 2));
    canvas_draw_line(
        canvas, center_x, (uint8_t)(vol_y + 1), center_x, (uint8_t)(vol_y + vol_h - 2));

    int8_t vu = model->volume_units;
    if(vu > ATM_VOLUME_UNIT_MAX) vu = ATM_VOLUME_UNIT_MAX;
    if(vu < -ATM_VOLUME_UNIT_MAX) vu = -ATM_VOLUME_UNIT_MAX;

    if(vu >= 0) {
        uint8_t w = (uint8_t)(((uint16_t)vu * (vol_inner_w / 2)) / ATM_VOLUME_UNIT_MAX);
        if(w)
            canvas_draw_box(
                canvas, (uint8_t)(center_x + 1), (uint8_t)(vol_y + 1), w, (uint8_t)(vol_h - 2));
    } else {
        uint8_t w = (uint8_t)(((uint16_t)(-vu) * (vol_inner_w / 2)) / ATM_VOLUME_UNIT_MAX);
        if(w) {
            canvas_draw_box(
                canvas, (uint8_t)(center_x - w), (uint8_t)(vol_y + 1), w, (uint8_t)(vol_h - 2));
        }
    }

    const Icon* state_icon = NULL;
    if(model->playing) {
        state_icon = model->paused ? &I_pause : &I_play;
    }

    if(state_icon) {
        const int32_t icon_x = ((int32_t)128 - (int32_t)icon_get_width(state_icon)) / 2;
        const int32_t icon_y = 14;
        canvas_draw_icon(canvas, icon_x, icon_y, state_icon);
    } else if(model->state_line[0]) {
        canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, model->state_line);
    }

    for(size_t i = 0; i < 4; i++) {
        uint8_t w = model->levels[i];
        if(w > meter_inner_w) w = meter_inner_w;

        const uint8_t y = (uint8_t)(meter_top + i * (meter_outer_h + meter_gap));

        canvas_draw_frame(canvas, meter_x, y, meter_outer_w, meter_outer_h);
        if(w)
            canvas_draw_box(
                canvas,
                (uint8_t)(meter_x + meter_frame),
                (uint8_t)(y + meter_frame),
                w,
                meter_inner_h);
    }
}

static bool atm_player_input_callback(InputEvent* event, void* context) {
    FlipperAtmApp* app = (FlipperAtmApp*)context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyOk && app->song_buf) {
            if(!app->playing) {
                ATM.play(app->song_buf);
                app->playing = true;
                app->paused = false;
            } else {
                ATM.playPause();
                app->paused = !app->paused;
            }
            atm_set_playback_state(app);
            consumed = true;
        } else if(event->key == InputKeyDown) {
            atm_switch_track(app, +1);
            consumed = true;
        } else if(event->key == InputKeyUp) {
            atm_switch_track(app, -1);
            consumed = true;
        } else if(event->key == InputKeyRight) {
            if(app->volume_units < ATM_VOLUME_UNIT_MAX) {
                app->volume_units++;
                atm_apply_volume_units(app);
                atm_set_playback_state(app);
            }
            consumed = true;
        } else if(event->key == InputKeyLeft) {
            if(app->volume_units > -ATM_VOLUME_UNIT_MAX) {
                app->volume_units--;
                atm_apply_volume_units(app);
                atm_set_playback_state(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

static bool atm_custom_event_callback(void* context, uint32_t event) {
    FlipperAtmApp* app = (FlipperAtmApp*)context;

    if(event == AtmEventUiTick) {
        atm_update_levels(app);
        return true;
    }

    if(event == AtmEventOpenBrowser) {
        atm_open_browser(app);
        return true;
    }

    if(event == AtmEventFileSelected) {
        if(app->browser_started) {
            file_browser_stop(app->file_browser);
            app->browser_started = false;
        }

        atm_play_selected_file(app);

        app->current_view = AtmViewPlayer;
        view_dispatcher_switch_to_view(app->dispatcher, AtmViewPlayer);
        return true;
    }

    return false;
}

static bool atm_navigation_event_callback(void* context) {
    FlipperAtmApp* app = (FlipperAtmApp*)context;

    if(app->current_view == AtmViewPlayer) {
        view_dispatcher_send_custom_event(app->dispatcher, AtmEventOpenBrowser);
        return true;
    }

    return false;
}

static bool atm_file_browser_item_callback(
    FuriString* path,
    void* context,
    uint8_t** icon,
    FuriString* item_name) {
    UNUSED(path);
    UNUSED(context);
    UNUSED(item_name);

    const Icon* file_icon = &I_icon;
    const uint8_t* frame = icon_get_frame_data(file_icon, 0);
    if(!frame || !(*icon)) return false;

    // file_browser custom icon buffer size is fixed at 32 bytes.
    memcpy(*icon, frame, 32);
    return true;
}

extern "C" int32_t flipper_atm_app(void* p) {
    UNUSED(p);

    FlipperAtmApp* app = (FlipperAtmApp*)malloc(sizeof(FlipperAtmApp));
    if(!app) return -1;
    memset(app, 0, sizeof(FlipperAtmApp));

    app->gui = (Gui*)furi_record_open(RECORD_GUI);
    app->storage = (Storage*)furi_record_open(RECORD_STORAGE);
    app->dispatcher = view_dispatcher_alloc();

    app->selected_path = furi_string_alloc();
    furi_string_set_str(app->selected_path, APP_ASSETS_PATH("title.atm"));
    snprintf(app->song_name, sizeof(app->song_name), "%s", "-");
    app->volume_units = 0;
    atm_reset_ui_level_meters(app);

    app->file_browser = file_browser_alloc(app->selected_path);
    file_browser_configure(
        app->file_browser, ".atm", APP_ASSETS_PATH(""), false, true, NULL, true);
    file_browser_set_callback(app->file_browser, atm_file_selected_callback, app);
    file_browser_set_item_callback(app->file_browser, atm_file_browser_item_callback, app);

    app->player_view = view_alloc();
    view_set_context(app->player_view, app);
    view_allocate_model(app->player_view, ViewModelTypeLocking, sizeof(AtmPlayerModel));
    view_set_draw_callback(app->player_view, atm_player_draw_callback);
    view_set_input_callback(app->player_view, atm_player_input_callback);

    atm_apply_volume_units(app);
    atm_set_player_status(app, "-", "Choose file", false);
    atm_update_levels(app);

    app->ui_timer = furi_timer_alloc(atm_ui_timer_callback, FuriTimerTypePeriodic, app);
    if(app->ui_timer) {
        furi_timer_start(app->ui_timer, furi_ms_to_ticks(33));
    }

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, atm_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, atm_navigation_event_callback);

    view_dispatcher_add_view(
        app->dispatcher, AtmViewBrowser, file_browser_get_view(app->file_browser));
    view_dispatcher_add_view(app->dispatcher, AtmViewPlayer, app->player_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    atm_system_init();
    atm_set_enabled(1);

    atm_open_browser(app);
    view_dispatcher_run(app->dispatcher);

    ATM.stop();
    atm_system_deinit();

    if(app->ui_timer) {
        furi_timer_stop(app->ui_timer);
        furi_timer_free(app->ui_timer);
    }

    if(app->browser_started) {
        file_browser_stop(app->file_browser);
    }

    if(app->song_buf) free(app->song_buf);

    view_dispatcher_remove_view(app->dispatcher, AtmViewBrowser);
    view_dispatcher_remove_view(app->dispatcher, AtmViewPlayer);
    file_browser_free(app->file_browser);
    view_free(app->player_view);

    view_dispatcher_free(app->dispatcher);
    furi_string_free(app->selected_path);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
