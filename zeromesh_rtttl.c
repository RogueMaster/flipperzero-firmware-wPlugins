#include "zeromesh_rtttl.h"
#include "zeromesh_notify.h"

#include <furi_hal_speaker.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define RTTTL_MAX_BYTES 512
#define RTTTL_MAX_NOTES 192
#define RTTTL_MAX_TOTAL_MS 12000

/* Equal temperament, octave 4. Every other octave is a power of two away, so
   the whole table is two multiplies instead of powf, which faults from a FAP. */
static const float note_hz[12] = {
    261.63f,
    277.18f,
    293.66f,
    311.13f,
    329.63f,
    349.23f,
    369.99f,
    392.00f,
    415.30f,
    440.00f,
    466.16f,
    493.88f,
};

static float freq_for(int note, int octave) {
    float f = note_hz[note];
    while(octave > 4) {
        f *= 2.0f;
        octave--;
    }
    while(octave < 4) {
        f *= 0.5f;
        octave++;
    }
    return f;
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int note_of(char c) {
    switch(lower(c)) {
    case 'c':
        return 0;
    case 'd':
        return 2;
    case 'e':
        return 4;
    case 'f':
        return 5;
    case 'g':
        return 7;
    case 'a':
        return 9;
    case 'b':
        return 11;
    case 'p':
        return -1;
    default:
        return -2;
    }
}

static bool rtttl_play_string(const char* s) {
    const char* p = strchr(s, ':');
    if(!p) return false;
    p++;

    const char* end = strchr(p, ':');
    if(!end) return false;

    int d = 4, o = 5, b = 63;
    const char* sec = p;
    while(sec < end) {
        if(sec + 1 < end && sec[1] == '=') {
            int v = atoi(sec + 2);
            if(lower(sec[0]) == 'd' && v > 0) d = v;
            else if(lower(sec[0]) == 'o' && v >= 3 && v <= 8) o = v;
            else if(lower(sec[0]) == 'b' && v > 0) b = v;
        }
        while(sec < end && *sec != ',') sec++;
        if(sec < end) sec++;
    }

    /* A whole note is four beats. Clamped so a silly tempo cannot divide by
       zero or hold the speaker for a minute. */
    if(b < 8) b = 8;
    if(b > 900) b = 900;
    uint32_t whole_ms = 240000u / (uint32_t)b;

    p = end + 1;
    uint32_t total = 0;
    int played = 0;

    while(*p && played < RTTTL_MAX_NOTES && total < RTTTL_MAX_TOTAL_MS) {
        while(*p == ',' || *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
        if(!*p) break;

        int dur = 0;
        while(*p >= '0' && *p <= '9') {
            dur = dur * 10 + (*p - '0');
            p++;
        }
        if(dur <= 0) dur = d;

        int note = note_of(*p);
        if(note == -2) {
            p++;
            continue;
        }
        p++;

        int oct = o;
        bool dotted = false;

        if(*p == '#') {
            if(note >= 0) note++;
            p++;
        }
        if(*p == '.') {
            dotted = true;
            p++;
        }
        if(*p >= '3' && *p <= '8') {
            oct = *p - '0';
            p++;
        }
        if(*p == '.') {
            dotted = true;
            p++;
        }

        if(note >= 12) {
            note -= 12;
            oct++;
        }

        uint32_t ms = whole_ms / (uint32_t)dur;
        if(dotted) ms += ms / 2;
        if(ms < 10) ms = 10;
        if(ms > 2000) ms = 2000;

        /* A short gap keeps repeated notes from running into one tone. */
        uint32_t gap = ms > 40 ? 15 : 5;

        if(note >= 0) {
            furi_hal_speaker_start(freq_for(note, oct), 0.6f);
            furi_delay_ms(ms - gap);
            furi_hal_speaker_stop();
            furi_delay_ms(gap);
        } else {
            furi_delay_ms(ms);
        }

        total += ms;
        played++;
    }

    return played > 0;
}

void ringtones_scan(ZeroMeshApp* app) {
    if(!app) return;
    app->custom_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, RINGTONE_DIR);

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, RINGTONE_DIR)) {
        FileInfo info;
        char name[64];
        while(app->custom_count < RINGTONE_CUSTOM_MAX) {
            if(!storage_dir_read(dir, &info, name, sizeof(name))) break;
            if(info.flags & FSF_DIRECTORY) continue;

            size_t l = strlen(name);
            if(l < 7 || l >= RINGTONE_NAME_MAX) continue;
            const char* ext = name + l - 6;
            if(lower(ext[0]) != '.' || lower(ext[1]) != 'r' || lower(ext[2]) != 't' ||
               lower(ext[3]) != 't' || lower(ext[4]) != 't' || lower(ext[5]) != 'l')
                continue;

            strlcpy(app->custom_files[app->custom_count], name, RINGTONE_NAME_MAX);
            app->custom_count++;
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

uint16_t ringtone_total(const ZeroMeshApp* app) {
    return (uint16_t)(RINGTONE_COUNT + (app ? app->custom_count : 0));
}

void ringtone_label(const ZeroMeshApp* app, uint16_t index, char* out, size_t cap) {
    if(!out || cap == 0) return;
    out[0] = '\0';

    if(index < RINGTONE_COUNT) {
        strlcpy(out, ringtone_names[index], cap);
        return;
    }

    uint16_t i = index - RINGTONE_COUNT;
    if(!app || i >= app->custom_count) return;

    strlcpy(out, app->custom_files[i], cap);
    char* dot = strrchr(out, '.');
    if(dot) *dot = '\0';
}

int16_t ringtone_index_of(const ZeroMeshApp* app, const char* filename) {
    if(!app || !filename || !filename[0]) return -1;
    for(uint8_t i = 0; i < app->custom_count; i++) {
        if(strcmp(app->custom_files[i], filename) == 0) return (int16_t)(RINGTONE_COUNT + i);
    }
    return -1;
}

bool rtttl_play_custom(const ZeroMeshApp* app, uint16_t index) {
    if(!app || index < RINGTONE_COUNT) return false;
    uint16_t i = index - RINGTONE_COUNT;
    if(i >= app->custom_count) return false;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", RINGTONE_DIR, app->custom_files[i]);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char* buf = malloc(RTTTL_MAX_BYTES + 1);
    bool ok = false;

    if(buf && storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t n = storage_file_read(file, buf, RTTTL_MAX_BYTES);
        buf[n] = '\0';
        storage_file_close(file);
        ok = rtttl_play_string(buf);
    }

    if(buf) free(buf);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
