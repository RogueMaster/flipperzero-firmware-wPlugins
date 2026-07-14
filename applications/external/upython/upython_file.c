#include <cli/cli.h>
#include <furi.h>

#include <genhdr/mpversion.h>
#include <mp_flipper_compiler.h>

#include <mp_flipper_repl.h>

#include "upython.h"

void upython_file_execute(FuriString* file) {
    size_t stack;

    const char* path = furi_string_get_cstr(file);
    FuriString* file_path = furi_string_alloc_printf("%s", path);

    do {
        FURI_LOG_I(TAG, "executing script %s", path);

        const size_t heap_size = memmgr_get_free_heap() * 0.1;
        const size_t stack_size = 2 * 1024;
        uint8_t* heap = malloc(heap_size * sizeof(uint8_t));

        FURI_LOG_D(TAG, "initial heap size is %zu bytes", heap_size);
        FURI_LOG_D(TAG, "stack size is %zu bytes", stack_size);

        size_t index = furi_string_search_rchar(file_path, '/');

        if(index == FURI_STRING_FAILURE) {
            FURI_LOG_E(TAG, "invalid file path");

            break;
        }

        bool is_py_file = furi_string_end_with_str(file_path, ".py");

        furi_string_left(file_path, index);

        mp_flipper_set_root_module_path(furi_string_get_cstr(file_path));

        mp_flipper_init(heap, heap_size, stack_size, &stack);

        if(is_py_file) {
            mp_flipper_exec_py_file(path);
        }

        mp_flipper_deinit();

        free(heap);
    } while(false);

    furi_string_free(file_path);
}
