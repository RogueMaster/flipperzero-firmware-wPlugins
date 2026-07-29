#include "nyx_store.h"
#include "ir_sense.h"
#include "../nyx_i.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define NYX_SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define NYX_SETTINGS_MAGIC   0x4E // 'N'
#define NYX_SETTINGS_VERSION 1

static void nyx_store_ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
}

void nyx_store_settings_save(const NyxSettings* s) {
    furi_assert(s);
    nyx_store_ensure_dir();
    saved_struct_save(
        NYX_SETTINGS_PATH, s, sizeof(NyxSettings), NYX_SETTINGS_MAGIC, NYX_SETTINGS_VERSION);
}

void nyx_store_settings_load(NyxSettings* s) {
    furi_assert(s);
    NyxSettings loaded;
    if(!saved_struct_load(
           NYX_SETTINGS_PATH,
           &loaded,
           sizeof(NyxSettings),
           NYX_SETTINGS_MAGIC,
           NYX_SETTINGS_VERSION)) {
        return; // nothing valid on disk — caller keeps its defaults
    }

    /* Never trust a file to index an array. Clamp everything that later
     * subscripts a table or drives a HAL enum. */
    if(loaded.mode_index > IrSenseModeProbe) loaded.mode_index = IrSenseModeAuto;
    if(loaded.sensitivity_index > 2) loaded.sensitivity_index = 1;
    uint8_t pin_count = ir_sense_probe_pin_count();
    if(pin_count == 0 || loaded.probe_pin_index >= pin_count) loaded.probe_pin_index = 0;
    loaded.sound = loaded.sound ? 1 : 0;
    loaded.vibro = loaded.vibro ? 1 : 0;
    loaded.led = loaded.led ? 1 : 0;

    *s = loaded;
}
