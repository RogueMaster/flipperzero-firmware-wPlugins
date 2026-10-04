#pragma once

/* Private va_list entry points for type-correct forwarding across the FAL ABI. */
#include "mjs_exec_public.h"
#include <stdarg.h>

mjs_err_t mjs_vcall(
    struct mjs* mjs,
    mjs_val_t* res,
    mjs_val_t func,
    mjs_val_t this_val,
    int nargs,
    va_list args);
mjs_err_t mjs_vset_errorf(struct mjs* mjs, mjs_err_t err, const char* fmt, va_list args);
mjs_err_t mjs_vprepend_errorf(struct mjs* mjs, mjs_err_t err, const char* fmt, va_list args);
