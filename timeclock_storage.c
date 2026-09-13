// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_storage.h"
#include "timeclock_i18n.h"

#include <furi_hal_rtc.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#define BADGES_HEADER  "uid,name,tech,created,last_used,last_event"
#define HISTORY_HEADER "date,time,name,uid,type"

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

// Strip trailing \r and \n from a FuriString.
static void tc_trim_eol(FuriString* s) {
    size_t len = furi_string_size(s);
    while(len > 0) {
        char c = furi_string_get_char(s, len - 1);
        if(c == '\n' || c == '\r') {
            furi_string_left(s, len - 1);
            len--;
        } else {
            break;
        }
    }
}

// Copy the n-th (0-based) comma-separated field of a CSV line into "out".
static void tc_csv_field(const char* line, int index, char* out, size_t out_size) {
    out[0] = '\0';
    int field = 0;
    const char* p = line;
    const char* start = line;
    while(true) {
        if(*p == ',' || *p == '\0') {
            if(field == index) {
                size_t n = (size_t)(p - start);
                if(n >= out_size) n = out_size - 1;
                memcpy(out, start, n);
                out[n] = '\0';
                return;
            }
            if(*p == '\0') return;
            field++;
            start = p + 1;
        }
        p++;
    }
}

// Replace commas/newlines with spaces so the CSV format is not broken.
static void tc_sanitize(const char* in, char* out, size_t out_size) {
    size_t i = 0;
    for(; in[i] != '\0' && i < out_size - 1; i++) {
        char c = in[i];
        if(c == ',' || c == '\n' || c == '\r') c = ' ';
        out[i] = c;
    }
    out[i] = '\0';
}

static TcEventType tc_event_from_str(const char* s) {
    if(strcmp(s, "IN") == 0) return TcEventIn;
    if(strcmp(s, "OUT") == 0) return TcEventOut;
    return TcEventNone;
}

static const char* tc_event_to_str(TcEventType t) {
    switch(t) {
    case TcEventIn:
        return "IN";
    case TcEventOut:
        return "OUT";
    default:
        return "-";
    }
}

static int tc_hhmm_to_minutes(const char* hhmm) {
    // "HH:MM"
    if(strlen(hhmm) < 5) return -1;
    int h = (hhmm[0] - '0') * 10 + (hhmm[1] - '0');
    int m = (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
    if(h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

void tc_storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, TC_DIR_PATH);
    furi_record_close(RECORD_STORAGE);
}

// -----------------------------------------------------------------------------
// Date/time
// -----------------------------------------------------------------------------

void tc_now_date(char* out, size_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, out_size, "%04u-%02u-%02u", dt.year, dt.month, dt.day);
}

void tc_now_time(char* out, size_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, out_size, "%02u:%02u", dt.hour, dt.minute);
}

void tc_now_datetime(char* out, size_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        out, out_size, "%04u-%02u-%02u %02u:%02u", dt.year, dt.month, dt.day, dt.hour, dt.minute);
}

// -----------------------------------------------------------------------------
// Config (simple key=value format)
// -----------------------------------------------------------------------------

void tc_config_load(TcConfig* config) {
    // Defaults
    config->auto_mode = false;
    config->use_lf = false;
    config->sound_enabled = true; // on by default
    config->vibro_enabled = true; // on by default
    config->led_enabled = true; // on by default
    config->pin_enabled = false;
    config->pin_hash = 0;
    config->pin_salt = 0;
    config->attempts = 0;
    config->onboarded = false;
    config->language = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, TC_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            unsigned long v = 0;
            if(sscanf(s, "auto_mode=%lu", &v) == 1) {
                config->auto_mode = v != 0;
            } else if(sscanf(s, "use_lf=%lu", &v) == 1) {
                config->use_lf = v != 0;
            } else if(sscanf(s, "sound_enabled=%lu", &v) == 1) {
                config->sound_enabled = v != 0;
            } else if(sscanf(s, "vibro_enabled=%lu", &v) == 1) {
                config->vibro_enabled = v != 0;
            } else if(sscanf(s, "led_enabled=%lu", &v) == 1) {
                config->led_enabled = v != 0;
            } else if(sscanf(s, "pin_enabled=%lu", &v) == 1) {
                config->pin_enabled = v != 0;
            } else if(sscanf(s, "pin_hash=%lu", &v) == 1) {
                config->pin_hash = (uint32_t)v;
            } else if(sscanf(s, "pin_salt=%lu", &v) == 1) {
                config->pin_salt = (uint32_t)v;
            } else if(sscanf(s, "attempts=%lu", &v) == 1) {
                config->attempts = (uint32_t)v;
            } else if(sscanf(s, "onboarded=%lu", &v) == 1) {
                config->onboarded = v != 0;
            } else if(sscanf(s, "language=%lu", &v) == 1) {
                config->language = (uint32_t)v;
            }
        }
        furi_string_free(line);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

void tc_config_save(const TcConfig* config) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, TC_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_format(stream, "auto_mode=%u\n", config->auto_mode ? 1 : 0);
        stream_write_format(stream, "use_lf=%u\n", config->use_lf ? 1 : 0);
        stream_write_format(stream, "sound_enabled=%u\n", config->sound_enabled ? 1 : 0);
        stream_write_format(stream, "vibro_enabled=%u\n", config->vibro_enabled ? 1 : 0);
        stream_write_format(stream, "led_enabled=%u\n", config->led_enabled ? 1 : 0);
        stream_write_format(stream, "pin_enabled=%u\n", config->pin_enabled ? 1 : 0);
        stream_write_format(stream, "pin_hash=%lu\n", (unsigned long)config->pin_hash);
        stream_write_format(stream, "pin_salt=%lu\n", (unsigned long)config->pin_salt);
        stream_write_format(stream, "attempts=%lu\n", (unsigned long)config->attempts);
        stream_write_format(stream, "onboarded=%u\n", config->onboarded ? 1 : 0);
        stream_write_format(stream, "language=%lu\n", (unsigned long)config->language);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

// -----------------------------------------------------------------------------
// Badges
// -----------------------------------------------------------------------------

size_t tc_badges_load(Badge* out_badges, size_t max) {
    size_t count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, TC_BADGES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        while(stream_read_line(stream, line) && count < max) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "uid,", 4) == 0) continue; // skip header
            }
            if(furi_string_size(line) == 0) continue;

            Badge* b = &out_badges[count];
            char event[TC_NAME_MAX];
            tc_csv_field(s, 0, b->uid, sizeof(b->uid));
            tc_csv_field(s, 1, b->name, sizeof(b->name));
            tc_csv_field(s, 2, b->tech, sizeof(b->tech));
            tc_csv_field(s, 3, b->created, sizeof(b->created));
            tc_csv_field(s, 4, b->last_used, sizeof(b->last_used));
            tc_csv_field(s, 5, event, sizeof(event));
            b->last_event = tc_event_from_str(event);
            if(strlen(b->uid) > 0) count++;
        }
        furi_string_free(line);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return count;
}

bool tc_badges_save(const Badge* badges, size_t count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool ok = false;
    if(file_stream_open(stream, TC_BADGES_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_format(stream, "%s\n", BADGES_HEADER);
        char name[TC_NAME_MAX];
        for(size_t i = 0; i < count; i++) {
            const Badge* b = &badges[i];
            tc_sanitize(b->name, name, sizeof(name));
            stream_write_format(
                stream,
                "%s,%s,%s,%s,%s,%s\n",
                b->uid,
                name,
                b->tech,
                b->created,
                b->last_used,
                tc_event_to_str(b->last_event));
        }
        ok = true;
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// -----------------------------------------------------------------------------
// History
// -----------------------------------------------------------------------------

bool tc_history_append(
    const char* date,
    const char* time,
    const char* name,
    const char* uid,
    TcEventType type) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // If the file does not exist yet, write the header.
    bool exists = storage_file_exists(storage, TC_HISTORY_PATH);

    Stream* stream = file_stream_alloc(storage);
    bool ok = false;
    if(file_stream_open(stream, TC_HISTORY_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(!exists) {
            stream_write_format(stream, "%s\n", HISTORY_HEADER);
        }
        char safe_name[TC_NAME_MAX];
        tc_sanitize(name, safe_name, sizeof(safe_name));
        stream_write_format(
            stream, "%s,%s,%s,%s,%s\n", date, time, safe_name, uid, tc_event_to_str(type));
        ok = true;
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void tc_history_read_range(
    FuriString* out,
    const char* filter_uid,
    const char* from_date,
    const char* to_date) {
    furi_string_reset(out);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    size_t shown = 0;
    if(file_stream_open(stream, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        char date[TC_DT_MAX], time[8], name[TC_NAME_MAX], uid[TC_UID_STR_MAX], type[TC_NAME_MAX];
        while(stream_read_line(stream, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) continue;
            }
            if(furi_string_size(line) == 0) continue;

            tc_csv_field(s, 0, date, sizeof(date));
            tc_csv_field(s, 1, time, sizeof(time));
            tc_csv_field(s, 2, name, sizeof(name));
            tc_csv_field(s, 3, uid, sizeof(uid));
            tc_csv_field(s, 4, type, sizeof(type));

            if(from_date && strcmp(date, from_date) < 0) continue;
            if(to_date && strcmp(date, to_date) > 0) continue;
            if(filter_uid && strcmp(uid, filter_uid) != 0) continue;

            // Formatted row: "date time name TYPE"
            furi_string_cat_printf(out, "%s %s %s %s\n", date, time, name, type);
            shown++;
        }
        furi_string_free(line);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    if(shown == 0) {
        furi_string_set(out, tc_str(StrNoPunches));
    }
}

void tc_history_read(FuriString* out, const char* filter_uid, bool today_only) {
    if(today_only) {
        char today[TC_DT_MAX];
        tc_now_date(today, sizeof(today));
        tc_history_read_range(out, filter_uid, today, today);
    } else {
        tc_history_read_range(out, filter_uid, NULL, NULL);
    }
}

uint32_t tc_history_minutes_for_date(
    const char* date_filter,
    const char* filter_uid,
    char* first_in,
    size_t first_in_size,
    char* last_out,
    size_t last_out_size) {
    if(first_in && first_in_size) first_in[0] = '\0';
    if(last_out && last_out_size) last_out[0] = '\0';

    uint32_t total = 0;
    int open_in = -1; // minutes of the last IN without a matching OUT

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        char date[TC_DT_MAX], time[8], name[TC_NAME_MAX], uid[TC_UID_STR_MAX], type[TC_NAME_MAX];
        while(stream_read_line(stream, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) continue;
            }
            if(furi_string_size(line) == 0) continue;

            tc_csv_field(s, 0, date, sizeof(date));
            tc_csv_field(s, 1, time, sizeof(time));
            tc_csv_field(s, 2, name, sizeof(name));
            tc_csv_field(s, 3, uid, sizeof(uid));
            tc_csv_field(s, 4, type, sizeof(type));

            if(strcmp(date, date_filter) != 0) continue;
            if(filter_uid && strcmp(uid, filter_uid) != 0) continue;

            int minutes = tc_hhmm_to_minutes(time);
            if(minutes < 0) continue;
            TcEventType t = tc_event_from_str(type);

            if(t == TcEventIn) {
                open_in = minutes;
                if(first_in && first_in[0] == '\0') {
                    strncpy(first_in, time, first_in_size - 1);
                    first_in[first_in_size - 1] = '\0';
                }
            } else if(t == TcEventOut) {
                if(open_in >= 0 && minutes >= open_in) {
                    total += (uint32_t)(minutes - open_in);
                    open_in = -1;
                }
                if(last_out) {
                    strncpy(last_out, time, last_out_size - 1);
                    last_out[last_out_size - 1] = '\0';
                }
            }
        }
        furi_string_free(line);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return total;
}

uint32_t tc_history_today_minutes(
    const char* filter_uid,
    char* first_in,
    size_t first_in_size,
    char* last_out,
    size_t last_out_size) {
    char today[TC_DT_MAX];
    tc_now_date(today, sizeof(today));
    return tc_history_minutes_for_date(
        today, filter_uid, first_in, first_in_size, last_out, last_out_size);
}

uint32_t tc_history_month_minutes(const char* month_prefix, const char* filter_uid) {
    uint32_t total = 0;
    int open_in = -1;
    char cur_date[TC_DT_MAX] = "";

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        char date[TC_DT_MAX], time[8], name[TC_NAME_MAX], uid[TC_UID_STR_MAX], type[TC_NAME_MAX];
        while(stream_read_line(stream, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) continue;
            }
            if(furi_string_size(line) == 0) continue;

            tc_csv_field(s, 0, date, sizeof(date));
            tc_csv_field(s, 1, time, sizeof(time));
            tc_csv_field(s, 2, name, sizeof(name));
            tc_csv_field(s, 3, uid, sizeof(uid));
            tc_csv_field(s, 4, type, sizeof(type));

            if(strncmp(date, month_prefix, 7) != 0) continue;
            if(filter_uid && strcmp(uid, filter_uid) != 0) continue;
            if(strcmp(date, cur_date) != 0) {
                open_in = -1; // reset pairing at each new day
                strncpy(cur_date, date, sizeof(cur_date) - 1);
                cur_date[sizeof(cur_date) - 1] = '\0';
            }

            int minutes = tc_hhmm_to_minutes(time);
            if(minutes < 0) continue;
            TcEventType t = tc_event_from_str(type);
            if(t == TcEventIn) {
                open_in = minutes;
            } else if(t == TcEventOut && open_in >= 0 && minutes >= open_in) {
                total += (uint32_t)(minutes - open_in);
                open_in = -1;
            }
        }
        furi_string_free(line);
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return total;
}

bool tc_history_undo_last(const char* uid, TcEventType* new_last) {
    if(new_last) *new_last = TcEventNone;
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Pass 1: find the index of the last data row matching uid, and the event
    // type of the matching row before it (what the last event becomes).
    int target = -1;
    int idx = -1;
    TcEventType prev = TcEventNone;
    TcEventType lastmatch = TcEventNone;
    Stream* in = file_stream_alloc(storage);
    if(file_stream_open(in, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        char u[TC_UID_STR_MAX], ty[TC_NAME_MAX];
        while(stream_read_line(in, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) continue;
            }
            if(furi_string_size(line) == 0) continue;
            idx++;
            tc_csv_field(s, 3, u, sizeof(u));
            if(strcmp(u, uid) != 0) continue;
            tc_csv_field(s, 4, ty, sizeof(ty));
            prev = lastmatch;
            lastmatch = tc_event_from_str(ty);
            target = idx;
        }
        furi_string_free(line);
    }
    file_stream_close(in);
    stream_free(in);

    if(target < 0) {
        furi_record_close(RECORD_STORAGE);
        return false; // nothing to undo for this badge
    }

    // Pass 2: rewrite to a temp file, skipping the target data row.
    const char* tmp = TC_DIR_PATH "/punches.tmp";
    Stream* rin = file_stream_alloc(storage);
    Stream* out = file_stream_alloc(storage);
    bool ok = false;
    if(file_stream_open(rin, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       file_stream_open(out, tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* line = furi_string_alloc();
        bool first = true;
        int j = -1;
        while(stream_read_line(rin, line)) {
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) {
                    stream_write_string(out, line);
                    continue;
                }
            }
            FuriString* trimmed = furi_string_alloc_set(line);
            tc_trim_eol(trimmed);
            bool empty = furi_string_size(trimmed) == 0;
            furi_string_free(trimmed);
            if(empty) continue;
            j++;
            if(j == target) continue; // drop the removed punch
            stream_write_string(out, line);
        }
        furi_string_free(line);
        ok = true;
    }
    file_stream_close(rin);
    file_stream_close(out);
    stream_free(rin);
    stream_free(out);

    if(ok) {
        storage_common_remove(storage, TC_HISTORY_PATH);
        storage_common_rename(storage, tmp, TC_HISTORY_PATH);
        if(new_last) *new_last = prev;
    } else {
        storage_common_remove(storage, tmp);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tc_history_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool ok = false;
    if(file_stream_open(stream, TC_HISTORY_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_format(stream, "%s\n", HISTORY_HEADER);
        ok = true;
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tc_history_export_json(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* in = file_stream_alloc(storage);
    Stream* out = file_stream_alloc(storage);
    bool ok = false;

    if(file_stream_open(in, TC_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       file_stream_open(out, TC_EXPORT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_format(out, "[\n");
        FuriString* line = furi_string_alloc();
        bool first = true;
        bool wrote = false;
        char date[TC_DT_MAX], time[8], name[TC_NAME_MAX], uid[TC_UID_STR_MAX], type[TC_NAME_MAX];
        while(stream_read_line(in, line)) {
            tc_trim_eol(line);
            const char* s = furi_string_get_cstr(line);
            if(first) {
                first = false;
                if(strncmp(s, "date,", 5) == 0) continue;
            }
            if(furi_string_size(line) == 0) continue;

            tc_csv_field(s, 0, date, sizeof(date));
            tc_csv_field(s, 1, time, sizeof(time));
            tc_csv_field(s, 2, name, sizeof(name));
            tc_csv_field(s, 3, uid, sizeof(uid));
            tc_csv_field(s, 4, type, sizeof(type));

            if(wrote) stream_write_format(out, ",\n");
            stream_write_format(
                out,
                "  {\"date\":\"%s\",\"time\":\"%s\",\"name\":\"%s\",\"uid\":\"%s\",\"type\":\"%s\"}",
                date,
                time,
                name,
                uid,
                type);
            wrote = true;
        }
        stream_write_format(out, "\n]\n");
        furi_string_free(line);
        ok = true;
    }
    file_stream_close(in);
    file_stream_close(out);
    stream_free(in);
    stream_free(out);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void tc_now_stamp(char* out, size_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        out, out_size, "%04u-%02u-%02u_%02u%02u", dt.year, dt.month, dt.day, dt.hour, dt.minute);
}

bool tc_export_csv_dated(char* out_name, size_t out_size) {
    char date[TC_DT_MAX];
    tc_now_date(date, sizeof(date));
    char path[128];
    snprintf(path, sizeof(path), "%s/punches-%s.csv", TC_DIR_PATH, date);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, path); // overwrite a same-day snapshot
    FS_Error err = storage_common_copy(storage, TC_HISTORY_PATH, path);
    furi_record_close(RECORD_STORAGE);

    if(out_name) snprintf(out_name, out_size, "punches-%s.csv", date);
    return err == FSE_OK;
}

bool tc_backup_all(char* out, size_t out_size) {
    char ts[24];
    tc_now_stamp(ts, sizeof(ts));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, TC_DIR_PATH "/backup");

    char bpath[160];
    char ppath[160];
    snprintf(bpath, sizeof(bpath), "%s/backup/badges-%s.csv", TC_DIR_PATH, ts);
    snprintf(ppath, sizeof(ppath), "%s/backup/punches-%s.csv", TC_DIR_PATH, ts);
    FS_Error be = storage_common_copy(storage, TC_BADGES_PATH, bpath);
    FS_Error pe = storage_common_copy(storage, TC_HISTORY_PATH, ppath);
    furi_record_close(RECORD_STORAGE);

    if(out) snprintf(out, out_size, "backup/%s", ts);
    return be == FSE_OK || pe == FSE_OK;
}

size_t tc_backup_list(char out[][24], size_t max) {
    size_t count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, TC_DIR_PATH "/backup")) {
        FileInfo info;
        char name[128];
        while(count < max && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            // Match "badges-<ts>.csv" and extract <ts>.
            if(strncmp(name, "badges-", 7) != 0) continue;
            const char* start = name + 7;
            const char* dot = strstr(start, ".csv");
            if(!dot) continue;
            size_t len = (size_t)(dot - start);
            if(len == 0 || len >= 24) continue;
            memcpy(out[count], start, len);
            out[count][len] = '\0';
            count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return count;
}

bool tc_backup_restore(const char* stamp) {
    char bpath[160];
    char ppath[160];
    snprintf(bpath, sizeof(bpath), "%s/backup/badges-%s.csv", TC_DIR_PATH, stamp);
    snprintf(ppath, sizeof(ppath), "%s/backup/punches-%s.csv", TC_DIR_PATH, stamp);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;
    // Only replace a destination if the backup source actually exists, so a
    // partial backup can never wipe current data without a replacement.
    if(storage_file_exists(storage, bpath)) {
        storage_common_remove(storage, TC_BADGES_PATH);
        if(storage_common_copy(storage, bpath, TC_BADGES_PATH) == FSE_OK) ok = true;
    }
    if(storage_file_exists(storage, ppath)) {
        storage_common_remove(storage, TC_HISTORY_PATH);
        if(storage_common_copy(storage, ppath, TC_HISTORY_PATH) == FSE_OK) ok = true;
    }
    furi_record_close(RECORD_STORAGE);

    return ok;
}
