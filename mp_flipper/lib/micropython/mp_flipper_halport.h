#pragma once

#include "mpconfigport.h"

// Define so there's no dependency on extmod/virtpin.h
#define mp_hal_pin_obj_t

typedef enum {
    MP_FLIPPER_IMPORT_STAT_NO_EXIST,
    MP_FLIPPER_IMPORT_STAT_FILE,
    MP_FLIPPER_IMPORT_STAT_DIR,
} mp_flipper_import_stat_t;

void mp_flipper_stdout_tx_str(const char* str);
void mp_flipper_stdout_tx_strn_cooked(const char* str, size_t len);

mp_flipper_import_stat_t mp_flipper_import_stat(const char* path);

size_t mp_flipper_gc_get_max_new_split();
