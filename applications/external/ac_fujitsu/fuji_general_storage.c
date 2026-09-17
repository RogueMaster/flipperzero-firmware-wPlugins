#include "fuji_general_storage.h"
#include <storage/storage.h>

#define AC_STATE_FILE_PATH APP_DATA_PATH("ac_state.bin")
#define AC_STATE_MAGIC     0xAC
#define AC_STATE_VERSION   1

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t temp;
    uint8_t fan;
    uint32_t sleep_timer;
    uint32_t on_timer;
    uint32_t off_timer;
    uint8_t on_off_timer_mode;
    uint8_t is_powered;
    uint8_t swing;
    uint8_t has_alarm;
    uint8_t economy;
} FujitsuAcSavedState;

void fujitsu_remote_ac_state_read_from_file(FujitsuAirState* ac) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    FujitsuAcSavedState state;
    if(storage_file_open(file, AC_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &state, sizeof(state)) == sizeof(state)) {
            if(state.magic == AC_STATE_MAGIC && state.version == AC_STATE_VERSION) {
                ac->mode = state.mode;
                ac->temperature = state.temp;
                ac->fan_speed = state.fan;
                ac->sleep_timer = state.sleep_timer;
                ac->on_timer = state.on_timer;
                ac->off_timer = state.off_timer;
                ac->on_off_timer_mode = state.on_off_timer_mode;
                ac->is_powered = state.is_powered;
                ac->swing = state.swing;
                ac->has_alarm = state.has_alarm;
                ac->economy = state.economy;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void fujitsu_remote_ac_state_write_to_file(FujitsuAirState* ac) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);

    FujitsuAcSavedState state = {
        .magic = AC_STATE_MAGIC,
        .version = AC_STATE_VERSION,
        .mode = ac->mode,
        .temp = ac->temperature,
        .fan = ac->fan_speed,
        .sleep_timer = ac->sleep_timer,
        .on_timer = ac->on_timer,
        .off_timer = ac->off_timer,
        .on_off_timer_mode = ac->on_off_timer_mode,
        .is_powered = ac->is_powered,
        .swing = ac->swing,
        .has_alarm = ac->has_alarm,
        .economy = ac->economy};

    if(storage_file_open(file, AC_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &state, sizeof(state));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
