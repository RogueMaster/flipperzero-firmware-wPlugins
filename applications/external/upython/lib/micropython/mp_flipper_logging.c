#include "py/objint.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "py/obj.h"

#include "mp_flipper_logging.h"

static struct _mp_obj_int_t mp_flipper_log_level_obj = {&mp_type_int, MP_FLIPPER_LOG_LEVEL_INFO};

static mp_obj_t mp_flipper_logging_log_internal(uint8_t level, size_t n_args, const mp_obj_t* args) {
    if(n_args < 1 || level > mp_flipper_log_get_effective_level()) {
        return mp_const_none;
    }

    mp_obj_t message = args[0];

    if(n_args > 1) {
        mp_obj_t values = mp_obj_new_tuple(n_args - 1, &args[1]);

        message = mp_obj_str_binary_op(MP_BINARY_OP_MODULO, args[0], values);
    }

    mp_flipper_log(level, mp_obj_str_get_str(message));

    return mp_const_none;
}

static mp_obj_t mp_flipper_logging_set_level(mp_obj_t raw_level) {
    uint8_t level = mp_obj_get_int(raw_level);

    if(level >= MP_FLIPPER_LOG_LEVEL_NONE && level <= MP_FLIPPER_LOG_LEVEL_TRACE) {
        mp_flipper_log_level_obj.val = level;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_flipper_logging_set_level_obj, mp_flipper_logging_set_level);

static mp_obj_t mp_flipper_logging_get_effective_level() {
    uint8_t level = mp_flipper_log_get_effective_level();

    return mp_obj_new_int_from_uint(level);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_flipper_logging_get_effective_level_obj, mp_flipper_logging_get_effective_level);

static mp_obj_t mp_flipper_logging_log(size_t n_args, const mp_obj_t* args) {
    if(n_args < 2) {
        return mp_const_none;
    }

    uint8_t level = mp_obj_get_int(args[0]);

    return mp_flipper_logging_log_internal(level, n_args - 1, &args[1]);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_log_obj, 2, mp_flipper_logging_log);

static mp_obj_t mp_flipper_logging_trace(size_t n_args, const mp_obj_t* args) {
    return mp_flipper_logging_log_internal(MP_FLIPPER_LOG_LEVEL_TRACE, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_trace_obj, 1, mp_flipper_logging_trace);

static mp_obj_t mp_flipper_logging_debug(size_t n_args, const mp_obj_t* args) {
    return mp_flipper_logging_log_internal(MP_FLIPPER_LOG_LEVEL_DEBUG, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_debug_obj, 1, mp_flipper_logging_debug);

static mp_obj_t mp_flipper_logging_info(size_t n_args, const mp_obj_t* args) {
    return mp_flipper_logging_log_internal(MP_FLIPPER_LOG_LEVEL_INFO, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_info_obj, 1, mp_flipper_logging_info);

static mp_obj_t mp_flipper_logging_warn(size_t n_args, const mp_obj_t* args) {
    return mp_flipper_logging_log_internal(MP_FLIPPER_LOG_LEVEL_WARN, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_warn_obj, 1, mp_flipper_logging_warn);

static mp_obj_t mp_flipper_logging_error(size_t n_args, const mp_obj_t* args) {
    return mp_flipper_logging_log_internal(MP_FLIPPER_LOG_LEVEL_ERROR, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR(mp_flipper_logging_error_obj, 1, mp_flipper_logging_error);

static const mp_rom_map_elem_t mp_module_logging_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_logging)},
    {MP_ROM_QSTR(MP_QSTR_level), MP_ROM_PTR(&mp_flipper_log_level_obj)},
    {MP_ROM_QSTR(MP_QSTR_setLevel), MP_ROM_PTR(&mp_flipper_logging_set_level_obj)},
    {MP_ROM_QSTR(MP_QSTR_getEffectiveLevel), MP_ROM_PTR(&mp_flipper_logging_get_effective_level_obj)},
    {MP_ROM_QSTR(MP_QSTR_trace), MP_ROM_PTR(&mp_flipper_logging_trace_obj)},
    {MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&mp_flipper_logging_debug_obj)},
    {MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&mp_flipper_logging_info_obj)},
    {MP_ROM_QSTR(MP_QSTR_warn), MP_ROM_PTR(&mp_flipper_logging_warn_obj)},
    {MP_ROM_QSTR(MP_QSTR_error), MP_ROM_PTR(&mp_flipper_logging_error_obj)},
    {MP_ROM_QSTR(MP_QSTR_log), MP_ROM_PTR(&mp_flipper_logging_log_obj)},
    {MP_ROM_QSTR(MP_QSTR_TRACE), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_TRACE)},
    {MP_ROM_QSTR(MP_QSTR_DEBUG), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_DEBUG)},
    {MP_ROM_QSTR(MP_QSTR_INFO), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_INFO)},
    {MP_ROM_QSTR(MP_QSTR_WARN), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_WARN)},
    {MP_ROM_QSTR(MP_QSTR_ERROR), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_ERROR)},
    {MP_ROM_QSTR(MP_QSTR_NONE), MP_ROM_INT(MP_FLIPPER_LOG_LEVEL_NONE)},
};

static MP_DEFINE_CONST_DICT(mp_module_logging_globals, mp_module_logging_globals_table);

const mp_obj_module_t mp_module_logging = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&mp_module_logging_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_logging, mp_module_logging);