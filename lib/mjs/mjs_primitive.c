/*
 * Copyright (c) 2017 Cesanta Software Limited
 * All rights reserved
 */

#include "mjs_core.h"
#include "mjs_internal.h"
#include "mjs_primitive.h"
#include "mjs_string_public.h"
#include "mjs_util.h"

#include <float.h>

#define MJS_IS_POINTER_LEGIT(n) \
    (((n) & MJS_TAG_MASK) == 0 || ((n) & MJS_TAG_MASK) == (~0 & MJS_TAG_MASK))

MJS_PRIVATE mjs_val_t mjs_pointer_to_value(struct mjs* mjs, void* p) {
    uint64_t n = ((uint64_t)(uintptr_t)p);

    if(!MJS_IS_POINTER_LEGIT(n)) {
        mjs_prepend_errorf(mjs, MJS_TYPE_ERROR, "invalid pointer value: %p", p);
    }
    return n & ~MJS_TAG_MASK;
}

MJS_PRIVATE mjs_val_t mjs_legit_pointer_to_value(void* p) {
    uint64_t n = ((uint64_t)(uintptr_t)p);

    assert(MJS_IS_POINTER_LEGIT(n));
    return n & ~MJS_TAG_MASK;
}

MJS_PRIVATE void* get_ptr(mjs_val_t v) {
    return (void*)(uintptr_t)(v & 0xFFFFFFFFFFFFUL);
}

mjs_val_t mjs_mk_foreign(struct mjs* mjs, void* p) {
    (void)mjs;
    return mjs_pointer_to_value(mjs, p) | MJS_TAG_FOREIGN;
}

mjs_val_t mjs_mk_foreign_func(struct mjs* mjs, mjs_func_ptr_t fn) {
    union {
        mjs_func_ptr_t fn;
        void* p;
    } u;
    u.fn = fn;
    (void)mjs;
    return mjs_pointer_to_value(mjs, u.p) | MJS_TAG_FOREIGN;
}

MJS_PRIVATE void mjs_op_isnan(struct mjs* mjs) {
    mjs_val_t ret = MJS_UNDEFINED;
    mjs_val_t val = mjs_arg(mjs, 0);

    ret = mjs_mk_boolean(mjs, val == MJS_TAG_NAN);

    mjs_return(mjs, ret);
}

MJS_PRIVATE void mjs_number_to_string(struct mjs* mjs) {
    mjs_val_t ret = MJS_UNDEFINED;
    mjs_val_t base_v = MJS_UNDEFINED;
    int32_t base = 10;
    double num;

    /* get number from `this` */
    if(!mjs_check_arg(mjs, -1 /*this*/, "this", MJS_TYPE_NUMBER, NULL)) {
        goto clean;
    }
    num = mjs_get_double(mjs, mjs->vals.this_obj);

    if(mjs_nargs(mjs) >= 1) {
        /* get base from arg 0 */
        if(!mjs_check_arg(mjs, 0, "base", MJS_TYPE_NUMBER, &base_v)) {
            goto clean;
        }
        base = mjs_get_int(mjs, base_v);
    }

    if(base != 10 || floor(num) == num) {
        char tmp_str[] = "-2147483648";
        itoa((int32_t)num, tmp_str, base);
        ret = mjs_mk_string(mjs, tmp_str, ~0, true);
    } else {
        char tmp_str[] = "2.22514337200000e-308";
        snprintf(tmp_str, sizeof(tmp_str), "%.*g", DBL_DIG, num);
        size_t len = strlen(tmp_str);
        while(len && tmp_str[len - 1] == '0') {
            len--;
        }
        tmp_str[len] = '\0';
        ret = mjs_mk_string(mjs, tmp_str, ~0, true);
    }

clean:
    mjs_return(mjs, ret);
}
