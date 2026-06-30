#include "py/mperrno.h"
#include "py/obj.h"
#include "py/runtime.h"
#include <stdio.h>

#include "py/mphal.h"
#include "py/builtin.h"

#include "mp_flipper_halport.h"
#include "mp_flipper_fileio.h"

void mp_hal_stdout_tx_str(const char* str) {
    mp_flipper_stdout_tx_str(str);
}

void mp_hal_stdout_tx_strn_cooked(const char* str, size_t len) {
    mp_flipper_stdout_tx_strn_cooked(str, len);
}

mp_import_stat_t mp_import_stat(const char* path) {
    mp_flipper_import_stat_t stat = mp_flipper_import_stat(path);

    if(stat == MP_FLIPPER_IMPORT_STAT_FILE) {
        return MP_IMPORT_STAT_FILE;
    }

    if(stat == MP_FLIPPER_IMPORT_STAT_DIR) {
        return MP_IMPORT_STAT_DIR;
    }

    return MP_IMPORT_STAT_NO_EXIST;
}

size_t gc_get_max_new_split(void) {
    return mp_flipper_gc_get_max_new_split();
}