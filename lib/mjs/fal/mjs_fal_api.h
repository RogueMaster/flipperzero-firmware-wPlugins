#pragma once

/* Private firmware/engine ABI. Increment VERSION for any layout or semantic change.
 * Public mJS SDK signatures and value tags remain unchanged. */
#include <mjs/mjs_core_public.h>
#include <mjs/mjs_exec_public.h>
#include <mjs/mjs_object_public.h>
#include <mjs/mjs_array_public.h>
#include <mjs/mjs_array_buf_public.h>
#include <mjs/mjs_string_public.h>
#include <mjs/mjs_primitive_public.h>
#include <mjs/mjs_util_public.h>
#include <mjs/mjs_ffi_public.h>
#include <stdarg.h>

#define MJS_FAL_APP_ID      "mjs_engine"
#define MJS_FAL_API_VERSION 1
#define MJS_FAL_PATH        "/ext/apps_data/js_app/plugins/mjs_engine.fal"

/* One typed list supplies the table, implementations and forwarding stubs. */
#define MJS_FAL_FUNCTIONS(X)                                                                       \
    X(mjs_err_t,                                                                                   \
      mjs_apply,                                                                                   \
      (struct mjs * mjs,                                                                           \
       mjs_val_t * res,                                                                            \
       mjs_val_t func,                                                                             \
       mjs_val_t this_val,                                                                         \
       int nargs,                                                                                  \
       mjs_val_t* args),                                                                           \
      (mjs, res, func, this_val, nargs, args))                                                     \
    X(mjs_val_t, mjs_arg, (struct mjs * mjs, int n), (mjs, n))                                     \
    X(char*,                                                                                       \
      mjs_array_buf_get_ptr,                                                                       \
      (struct mjs * mjs, mjs_val_t buf, size_t * bytelen),                                         \
      (mjs, buf, bytelen))                                                                         \
    X(void,                                                                                        \
      mjs_array_del,                                                                               \
      (struct mjs * mjs, mjs_val_t arr, unsigned long index),                                      \
      (mjs, arr, index))                                                                           \
    X(mjs_val_t,                                                                                   \
      mjs_array_get,                                                                               \
      (struct mjs * mjs, mjs_val_t arr, unsigned long index),                                      \
      (mjs, arr, index))                                                                           \
    X(unsigned long, mjs_array_length, (struct mjs * mjs, mjs_val_t arr), (mjs, arr))              \
    X(mjs_err_t, mjs_array_push, (struct mjs * mjs, mjs_val_t arr, mjs_val_t v), (mjs, arr, v))    \
    X(mjs_err_t,                                                                                   \
      mjs_array_set,                                                                               \
      (struct mjs * mjs, mjs_val_t arr, unsigned long index, mjs_val_t v),                         \
      (mjs, arr, index, v))                                                                        \
    X(mjs_val_t, mjs_dataview_get_buf, (struct mjs * mjs, mjs_val_t obj), (mjs, obj))              \
    X(int,                                                                                         \
      mjs_del,                                                                                     \
      (struct mjs * mjs, mjs_val_t obj, const char* name, size_t len),                             \
      (mjs, obj, name, len))                                                                       \
    X(void,                                                                                        \
      mjs_disasm_all,                                                                              \
      (struct mjs * mjs, MjsPrintCallback print_cb, void* print_ctx),                              \
      (mjs, print_cb, print_ctx))                                                                  \
    X(int, mjs_disown, (struct mjs * mjs, mjs_val_t * v), (mjs, v))                                \
    X(mjs_err_t, mjs_exec, (struct mjs * mjs, const char* src, mjs_val_t* res), (mjs, src, res))   \
    X(mjs_err_t,                                                                                   \
      mjs_exec_file,                                                                               \
      (struct mjs * mjs, const char* path, mjs_val_t* res),                                        \
      (mjs, path, res))                                                                            \
    X(void, mjs_exit, (struct mjs * mjs), (mjs))                                                   \
    X(void*, mjs_ffi_resolve, (struct mjs * mjs, const char* symbol), (mjs, symbol))               \
    X(mjs_val_t,                                                                                   \
      mjs_get,                                                                                     \
      (struct mjs * mjs, mjs_val_t obj, const char* name, size_t len),                             \
      (mjs, obj, name, len))                                                                       \
    X(void*, mjs_get_context, (struct mjs * mjs), (mjs))                                           \
    X(const char*, mjs_get_cstring, (struct mjs * mjs, mjs_val_t * v), (mjs, v))                   \
    X(mjs_val_t, mjs_get_global, (struct mjs * mjs), (mjs))                                        \
    X(int, mjs_get_lineno_by_offset, (struct mjs * mjs, int offset), (mjs, offset))                \
    X(int, mjs_get_offset_by_call_frame_num, (struct mjs * mjs, int cf_num), (mjs, cf_num))        \
    X(const char*, mjs_get_stack_trace, (struct mjs * mjs), (mjs))                                 \
    X(const char*, mjs_get_string, (struct mjs * mjs, mjs_val_t * v, size_t * len), (mjs, v, len)) \
    X(mjs_val_t, mjs_get_this, (struct mjs * mjs), (mjs))                                          \
    X(mjs_val_t, mjs_get_v, (struct mjs * mjs, mjs_val_t obj, mjs_val_t name), (mjs, obj, name))   \
    X(mjs_val_t,                                                                                   \
      mjs_get_v_proto,                                                                             \
      (struct mjs * mjs, mjs_val_t obj, mjs_val_t key),                                            \
      (mjs, obj, key))                                                                             \
    X(int, mjs_is_truthy, (struct mjs * mjs, mjs_val_t v), (mjs, v))                               \
    X(mjs_val_t, mjs_mk_array, (struct mjs * mjs), (mjs))                                          \
    X(mjs_val_t,                                                                                   \
      mjs_mk_array_buf,                                                                            \
      (struct mjs * mjs, char* data, size_t buf_len),                                              \
      (mjs, data, buf_len))                                                                        \
    X(mjs_val_t, mjs_mk_foreign, (struct mjs * mjs, void* ptr), (mjs, ptr))                        \
    X(mjs_val_t, mjs_mk_foreign_func, (struct mjs * mjs, mjs_func_ptr_t fn), (mjs, fn))            \
    X(mjs_val_t, mjs_mk_object, (struct mjs * mjs), (mjs))                                         \
    X(mjs_val_t,                                                                                   \
      mjs_mk_string,                                                                               \
      (struct mjs * mjs, const char* str, size_t len, int copy),                                   \
      (mjs, str, len, copy))                                                                       \
    X(int, mjs_nargs, (struct mjs * mjs), (mjs))                                                   \
    X(mjs_val_t,                                                                                   \
      mjs_next,                                                                                    \
      (struct mjs * mjs, mjs_val_t obj, mjs_val_t * iterator),                                     \
      (mjs, obj, iterator))                                                                        \
    X(void, mjs_own, (struct mjs * mjs, mjs_val_t * v), (mjs, v))                                  \
    X(void, mjs_return, (struct mjs * mjs, mjs_val_t v), (mjs, v))                                 \
    X(mjs_err_t,                                                                                   \
      mjs_set,                                                                                     \
      (struct mjs * mjs, mjs_val_t obj, const char* name, size_t len, mjs_val_t val),              \
      (mjs, obj, name, len, val))                                                                  \
    X(void,                                                                                        \
      mjs_set_exec_flags_poller,                                                                   \
      (struct mjs * mjs, mjs_flags_poller_t poller),                                               \
      (mjs, poller))                                                                               \
    X(void,                                                                                        \
      mjs_set_ffi_resolver,                                                                        \
      (struct mjs * mjs, mjs_ffi_resolver_t * dlsym, void* handle),                                \
      (mjs, dlsym, handle))                                                                        \
    X(mjs_err_t,                                                                                   \
      mjs_set_v,                                                                                   \
      (struct mjs * mjs, mjs_val_t obj, mjs_val_t name, mjs_val_t val),                            \
      (mjs, obj, name, val))                                                                       \
    X(void,                                                                                        \
      mjs_sprintf,                                                                                 \
      (mjs_val_t v, struct mjs * mjs, char* buf, size_t buflen),                                   \
      (v, mjs, buf, buflen))                                                                       \
    X(int,                                                                                         \
      mjs_strcmp,                                                                                  \
      (struct mjs * mjs, mjs_val_t * a, const char* b, size_t len),                                \
      (mjs, a, b, len))                                                                            \
    X(const char*, mjs_strerror, (struct mjs * mjs, enum mjs_err err), (mjs, err))                 \
    X(mjs_val_t,                                                                                   \
      mjs_struct_to_obj,                                                                           \
      (struct mjs * mjs, const void* base, const struct mjs_c_struct_member* members),             \
      (mjs, base, members))                                                                        \
    X(mjs_val_t, mjs_to_boolean_v, (struct mjs * mjs, mjs_val_t v), (mjs, v))                      \
    X(mjs_err_t,                                                                                   \
      mjs_to_string,                                                                               \
      (struct mjs * mjs, mjs_val_t * v, char** p, size_t* sizep, int* need_free),                  \
      (mjs, v, p, sizep, need_free))

typedef struct {
    uint32_t size;
    struct mjs* (*create)(void* context);
    void (*destroy)(struct mjs* mjs);
    mjs_err_t (*vcall)(
        struct mjs* mjs,
        mjs_val_t* res,
        mjs_val_t func,
        mjs_val_t this_val,
        int nargs,
        va_list args);
    mjs_err_t (*vset_errorf)(struct mjs* mjs, mjs_err_t err, const char* fmt, va_list args);
    mjs_err_t (*vprepend_errorf)(struct mjs* mjs, mjs_err_t err, const char* fmt, va_list args);
#define MJS_FAL_FIELD(ret, name, params, args) ret(*name) params;
    MJS_FAL_FUNCTIONS(MJS_FAL_FIELD)
#undef MJS_FAL_FIELD
} MjsFalApi;
