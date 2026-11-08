#include "storage.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <string.h>

#define LOTERIA_SAVE_PATH    APP_DATA_PATH("loteria_zero.sav")
#define LOTERIA_SAVE_MAGIC   0x4C /* L */
#define LOTERIA_SAVE_VERSION 1

void loteria_persist_init_defaults(LoteriaPersist* persist) {
    memset(persist, 0, sizeof(LoteriaPersist));
    persist->auto_mode = LoteriaAutoManual;
    persist->skip_welcome = false;
    persist->has_saved_game = false;
    loteria_game_init(&persist->saved_game);
}

bool loteria_persist_load(LoteriaPersist* persist) {
    loteria_persist_init_defaults(persist);
    bool ok = saved_struct_load(
        LOTERIA_SAVE_PATH,
        persist,
        sizeof(LoteriaPersist),
        LOTERIA_SAVE_MAGIC,
        LOTERIA_SAVE_VERSION);
    if(!ok) {
        loteria_persist_init_defaults(persist);
    }
    if(persist->auto_mode >= LoteriaAutoCount) {
        persist->auto_mode = LoteriaAutoManual;
    }
    return ok;
}

bool loteria_persist_save(const LoteriaPersist* persist) {
    return saved_struct_save(
        LOTERIA_SAVE_PATH,
        persist,
        sizeof(LoteriaPersist),
        LOTERIA_SAVE_MAGIC,
        LOTERIA_SAVE_VERSION);
}

uint32_t loteria_auto_mode_ms(uint8_t mode) {
    switch(mode) {
    case LoteriaAuto5s:
        return 5000;
    case LoteriaAuto10s:
        return 10000;
    case LoteriaAuto15s:
        return 15000;
    default:
        return 0;
    }
}

const char* loteria_auto_mode_label(uint8_t mode) {
    switch(mode) {
    case LoteriaAuto5s:
        return "5 seconds";
    case LoteriaAuto10s:
        return "10 seconds";
    case LoteriaAuto15s:
        return "15 seconds";
    default:
        return "Manual";
    }
}
