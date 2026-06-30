#include "flipp_pomodoro_settings.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define SETTINGS_DIR  "/ext/apps_data/pomodoro_timer"
#define SETTINGS_PATH SETTINGS_DIR "/pomodoro_timer.conf"

typedef struct {
    uint8_t focus_minutes;
    uint8_t short_break_minutes;
    uint8_t long_break_minutes;
} FlippPomodoroSettingsV1;

void flipp_pomodoro_settings_set_default(FlippPomodoroSettings* settings) {
    settings->focus_minutes = 25;
    settings->short_break_minutes = 5;
    settings->long_break_minutes = 30;
    settings->buzz_mode = FlippPomodoroBuzzOnce;
}

static bool flipp_pomodoro_settings_try_load_from(File* file, const char* path, FlippPomodoroSettings* settings) {
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return false;
    }
    uint8_t buf[sizeof(FlippPomodoroSettings)] = {0};
    uint32_t n = storage_file_read(file, buf, sizeof(FlippPomodoroSettings));
    bool ok = false;
    if(n == sizeof(FlippPomodoroSettings)) {
        memcpy(settings, buf, sizeof(FlippPomodoroSettings));
        ok = true;
    } else if(n == sizeof(FlippPomodoroSettingsV1)) {
        const FlippPomodoroSettingsV1* v1 = (const FlippPomodoroSettingsV1*)buf;
        settings->focus_minutes = v1->focus_minutes;
        settings->short_break_minutes = v1->short_break_minutes;
        settings->long_break_minutes = v1->long_break_minutes;
        settings->buzz_mode = FlippPomodoroBuzzOnce; // upgrade by default
        ok = true;
    }
    storage_file_close(file);
    return ok;
}

bool flipp_pomodoro_settings_load(FlippPomodoroSettings* settings) {
    Storage* storage = furi_record_open("storage");
    File* file = storage_file_alloc(storage);

    bool ok = flipp_pomodoro_settings_try_load_from(file, SETTINGS_PATH, settings);

    storage_file_free(file);
    furi_record_close("storage");

    if(!ok) {
        flipp_pomodoro_settings_set_default(settings);
    }
    return ok;
}

bool flipp_pomodoro_settings_save(const FlippPomodoroSettings* settings) {
    Storage* storage = furi_record_open("storage");
    File* file = storage_file_alloc(storage);

    // ensure the dir exist
    storage_common_mkdir(storage, SETTINGS_DIR);

    bool ok = false;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(storage_file_write(file, settings, sizeof(FlippPomodoroSettings)) == sizeof(FlippPomodoroSettings)) {
            ok = true;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close("storage");
    return ok;
}
