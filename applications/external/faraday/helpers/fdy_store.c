#include "fdy_store.h"
#include "fdy_grade.h"
#include "fdy_subghz.h"

#include <furi_hal.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <datetime/datetime.h>
#include <stdio.h>
#include <string.h> // strcmp/strncpy, used below; do not rely on a transitive include

#define FDY_SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define FDY_RESULTS_PATH     APP_DATA_PATH("results.csv")
#define FDY_SETTINGS_MAGIC   0xFD
#define FDY_SETTINGS_VERSION 1

/* How many logged tests we keep in memory while scanning the file. The log
 * itself is allowed to grow; we only ever render the newest slice. */
#define FDY_RESULTS_MAX 20

/* A line longer than this is not one of ours. The results file lives on a card
 * the user can edit, and stream_read_line grows its FuriString until it finds
 * a newline - so a file with no newline in it lets the card decide how much of
 * the heap to take. Our own longest line is well under 128 bytes. */
#define FDY_LINE_MAX 160u

/* Stop scanning after this many lines. The log is append-only and uncapped, so
 * a long-lived install would otherwise re-parse the whole thing on the GUI
 * thread every time the Saved results screen opens. */
#define FDY_SCAN_MAX 4000u

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute;
    bool is_nfc;
    uint32_t frequency;
    int16_t base_value, shield_value, atten;
    uint8_t floored;
    char grade[4];
} FdyLogged;

static void fdy_store_ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
}

/* ---------------- settings ---------------- */

void fdy_store_settings_save(const FaradaySettings* s) {
    furi_assert(s);
    fdy_store_ensure_dir();
    saved_struct_save(
        FDY_SETTINGS_PATH, s, sizeof(FaradaySettings), FDY_SETTINGS_MAGIC, FDY_SETTINGS_VERSION);
}

void fdy_store_settings_load(FaradaySettings* s) {
    furi_assert(s);
    FaradaySettings loaded;
    if(!saved_struct_load(
           FDY_SETTINGS_PATH,
           &loaded,
           sizeof(FaradaySettings),
           FDY_SETTINGS_MAGIC,
           FDY_SETTINGS_VERSION)) {
        return; // nothing valid on disk - caller keeps its defaults
    }
    /* saved_struct checks the magic, the version and the size - never the
     * bytes. The file lives on a card the user can edit, so every field that
     * reaches the rest of the app is normalised here.
     *
     * The !! on the flags is not cosmetic: a _Bool holding anything other than
     * 0 or 1 is undefined the moment it is read, and a hand-edited file can
     * easily hold 2. */
    if(loaded.band_index >= FDY_BAND_COUNT) loaded.band_index = 1;
    loaded.sound = !!loaded.sound;
    loaded.led = !!loaded.led;
    *s = loaded;
}

/* ---------------- result log ---------------- */

bool fdy_store_result_append(const FdyResult* r) {
    furi_assert(r);
    fdy_store_ensure_dir();

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char line[128];
    int n = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u,%02u:%02u,%s,%lu,%d,%d,%d,%u,%s\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        r->is_nfc ? "NFC" : "SUBGHZ",
        (unsigned long)r->frequency,
        (int)r->base_value,
        (int)r->shield_value,
        (int)r->atten,
        r->floored ? 1u : 0u,
        fdy_rating_letter((FdyRating)r->rating));
    if(n <= 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, FDY_RESULTS_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        ok = storage_file_write(file, line, (size_t)n) == (size_t)n;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* Human label for a logged frequency.
 *
 * With its unit: the NFC rows said "13.56 MHz" while the Sub-GHz rows said
 * "433.92", so the one column that should have made the two comparable read
 * as two different kinds of thing. */
static const char* fdy_freq_label(bool is_nfc, uint32_t hz) {
    static char buf[16];
    if(is_nfc) return "13.56 MHz";
    for(size_t i = 0; i < FDY_BAND_COUNT; i++) {
        if(fdy_bands[i].frequency == hz) {
            snprintf(buf, sizeof(buf), "%s MHz", fdy_bands[i].label);
            return buf;
        }
    }
    return "Sub-GHz";
}

uint8_t fdy_store_results_render(FuriString* out, uint8_t max) {
    furi_assert(out);
    if(max > FDY_RESULTS_MAX) max = FDY_RESULTS_MAX;
    if(max == 0) return 0; // the ring arithmetic below is modulo `max`

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FdyLogged ring[FDY_RESULTS_MAX];
    uint8_t count = 0; // entries held
    uint8_t head = 0; // next write slot

    if(file_stream_open(stream, FDY_RESULTS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        uint32_t scanned = 0;
        while(stream_read_line(stream, line)) {
            if(++scanned > FDY_SCAN_MAX) break;
            if(furi_string_size(line) > FDY_LINE_MAX) continue; // not ours
            FdyLogged e;
            char radio[10] = {0};
            unsigned year, month, day, hour, minute, floored;
            unsigned long freq;
            int base, shield, atten;
            char grade[4] = {0};

            /* Anything that does not parse cleanly is skipped rather than
             * shown as garbage - the log is a plain text file a user may edit. */
            if(sscanf(
                   furi_string_get_cstr(line),
                   "%u-%u-%u,%u:%u,%9[^,],%lu,%d,%d,%d,%u,%3[^,\r\n]",
                   &year,
                   &month,
                   &day,
                   &hour,
                   &minute,
                   radio,
                   &freq,
                   &base,
                   &shield,
                   &atten,
                   &floored,
                   grade) != 12) {
                continue;
            }

            /* A row that parses is not a row that makes sense. These come off
             * a user-editable card, and an out-of-range value rendered
             * unchallenged would appear on screen as a real graded
             * measurement, indistinguishable from one the app took. */
            if(month > 12u || day > 31u || hour > 23u || minute > 59u) continue;
            if(base < -200 || base > 200) continue;
            if(shield < -200 || shield > 200) continue;
            if(atten < -200 || atten > 200) continue;
            if(grade[0] == '\0') continue;

            e.year = (uint16_t)year;
            e.month = (uint8_t)month;
            e.day = (uint8_t)day;
            e.hour = (uint8_t)hour;
            e.minute = (uint8_t)minute;
            e.is_nfc = (strcmp(radio, "NFC") == 0);
            e.frequency = (uint32_t)freq;
            e.base_value = (int16_t)base;
            e.shield_value = (int16_t)shield;
            e.atten = (int16_t)atten;
            e.floored = (uint8_t)(floored ? 1 : 0);
            strncpy(e.grade, grade, sizeof(e.grade) - 1);
            e.grade[sizeof(e.grade) - 1] = '\0';

            ring[head] = e; // rolling window: keep only the newest `max`
            head = (uint8_t)((head + 1) % max);
            if(count < max) count++;
        }
        furi_string_free(line);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    /* Newest first: walk the ring backwards from the most recent write. */
    for(uint8_t i = 0; i < count; i++) {
        uint8_t idx = (uint8_t)((head + max - 1 - i) % max);
        const FdyLogged* e = &ring[idx];
        furi_string_cat_printf(
            out,
            "\e#%s  %s%d %s\n%s\n%02u-%02u %02u:%02u\nair %d, bag %d\n\n",
            // screen-ok: "A+  >=-100 dB\n433.92 MHz\n09-26 10:53\nair -100, bag -100"
            e->grade,
            e->floored ? ">=" : "",
            (int)e->atten,
            e->is_nfc ? "%" : "dB",
            fdy_freq_label(e->is_nfc, e->frequency),
            (unsigned)e->month,
            (unsigned)e->day,
            (unsigned)e->hour,
            (unsigned)e->minute,
            (int)e->base_value,
            (int)e->shield_value);
    }

    return count;
}

const char* fdy_store_results_path(void) {
    return FDY_RESULTS_PATH;
}
