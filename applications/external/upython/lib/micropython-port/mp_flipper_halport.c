#include <stdio.h>

#include <furi.h>
#include <storage/storage.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_halport.h>

#include "mp_flipper_file_helper.h"

inline void mp_flipper_stdout_tx_str(const char* str) {
    printf("%s", str);
}

inline void mp_flipper_stdout_tx_strn_cooked(const char* str, size_t len) {
    printf("%.*s", len, str);
}

inline mp_flipper_import_stat_t mp_flipper_import_stat(const char* path) {
    FuriString* file_path = furi_string_alloc_printf("%s", path);

    mp_flipper_import_stat_t stat = mp_flipper_try_resolve_filesystem_path(file_path);

    stat = furi_string_end_with_str(file_path, path) ? stat : MP_FLIPPER_IMPORT_STAT_NO_EXIST;

    furi_string_free(file_path);

    return stat;
}

inline size_t mp_flipper_gc_get_max_new_split(void) {
    return memmgr_heap_get_max_free_block();
}
