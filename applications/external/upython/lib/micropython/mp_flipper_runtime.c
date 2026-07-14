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

#if MICROPY_ENABLE_GC
    gc_init(heap, (uint8_t*)heap + heap_size);
#endif

    mp_init();
}

void mp_flipper_deinit() {
    gc_sweep_all();

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
