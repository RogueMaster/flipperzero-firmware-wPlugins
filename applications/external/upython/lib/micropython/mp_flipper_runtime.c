#include <string.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/persistentcode.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"

#include "mp_flipper_runtime.h"
#include "mp_flipper_halport.h"

const char* mp_flipper_root_module_path;

void* mp_flipper_context;

void mp_flipper_set_root_module_path(const char* path) {
    mp_flipper_root_module_path = path;
}

void mp_flipper_init(void* heap, size_t heap_size, size_t stack_size, void* stack_top) {
    mp_flipper_context = mp_flipper_context_alloc();

    mp_stack_set_top(stack_top);
    mp_stack_set_limit(stack_size);

    gc_init(heap, (uint8_t*)heap + heap_size);

    mp_init();
}

void mp_flipper_exec_mpy_file(const char* file_path) {
#ifdef MP_FLIPPER_MPY_SUPPORT
#if MP_FLIPPER_IS_RUNTIME
    nlr_buf_t nlr;

    if(nlr_push(&nlr) == 0) {
        do {
            // check if file exists
            if(mp_flipper_import_stat(file_path) == MP_FLIPPER_IMPORT_STAT_NO_EXIST) {
                mp_raise_OSError_with_filename(MP_ENOENT, file_path);

                break;
            }

            // Execute the given .mpy file
            mp_module_context_t* context = m_new_obj(mp_module_context_t);
            context->module.globals = mp_globals_get();
            mp_compiled_module_t compiled_module;
            compiled_module.context = context;
            mp_raw_code_load_file(qstr_from_str(file_path), &compiled_module);
            mp_obj_t f = mp_make_function_from_proto_fun(compiled_module.rc, context, MP_OBJ_NULL);
            mp_call_function_0(f);
        } while(false);
        nlr_pop();
    } else {
        // Uncaught exception: print it out.
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    }
#endif
#endif
}

void mp_flipper_deinit() {
    mp_deinit();

    mp_flipper_context_free(mp_flipper_context);
}

// Called if an exception is raised outside all C exception-catching handlers.
void nlr_jump_fail(void* val) {
    mp_flipper_nlr_jump_fail(val);
}

#if MICROPY_ENABLE_GC
// Run a garbage collection cycle.
void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}
#endif

#ifndef NDEBUG
// Used when debugging is enabled.
void __assert_func(const char* file, int line, const char* func, const char* expr) {
    mp_flipper_assert(file, line, func, expr);
}

void NORETURN __fatal_error(const char* msg) {
    mp_flipper_fatal_error(msg);
}
#endif

void mp_flipper_raise_os_error(int error) {
    mp_raise_OSError(error);
}

void mp_flipper_raise_os_error_with_filename(int error, const char* filename) {
    mp_raise_OSError_with_filename(error, filename);
}
