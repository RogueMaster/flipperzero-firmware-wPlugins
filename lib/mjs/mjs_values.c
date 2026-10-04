/*
 * Copyright (c) 2017 Cesanta Software Limited
 * All rights reserved
 *
 * Context-independent value operations are shared by the native engine and
 * resident FAL bridge. They must never allocate an interpreter or load the SD.
 */
#include "mjs_core.h"
#include "mjs_primitive_public.h"
#include "mjs_array_public.h"
#include "mjs_array_buf_public.h"
#include "mjs_object_public.h"
#include "mjs_string_public.h"
#include "mjs_util.h"
#include <math.h>

mjs_val_t mjs_mk_null(void) {
    return MJS_NULL;
}

int mjs_is_null(mjs_val_t v) {
    return v == MJS_NULL;
}

mjs_val_t mjs_mk_undefined(void) {
    return MJS_UNDEFINED;
}

int mjs_is_undefined(mjs_val_t v) {
    return v == MJS_UNDEFINED;
}

mjs_val_t mjs_mk_number(struct mjs* mjs, double v) {
    mjs_val_t res;
    (void)mjs;
    /* not every NaN is a JS NaN */
    if(isnan(v)) {
        res = MJS_TAG_NAN;
    } else {
        union {
            double d;
            mjs_val_t r;
        } u;
        u.d = v;
        res = u.r;
    }
    return res;
}

static double get_double(mjs_val_t v) {
    union {
        double d;
        mjs_val_t v;
    } u;
    u.v = v;
    /* Due to NaN packing, any non-numeric value is already a valid NaN value */
    return u.d;
}

double mjs_get_double(struct mjs* mjs, mjs_val_t v) {
    (void)mjs;
    return get_double(v);
}

int mjs_get_int(struct mjs* mjs, mjs_val_t v) {
    (void)mjs;
    /*
   * NOTE(dfrank): without double cast, all numbers >= 0x80000000 are always
   * converted to exactly 0x80000000.
   */
    return (int)(unsigned int)get_double(v);
}

int32_t mjs_get_int32(struct mjs* mjs, mjs_val_t v) {
    (void)mjs;
    return (int32_t)get_double(v);
}

int mjs_is_number(mjs_val_t v) {
    return v == MJS_TAG_NAN || !isnan(get_double(v));
}

mjs_val_t mjs_mk_boolean(struct mjs* mjs, int v) {
    (void)mjs;
    return (v ? 1 : 0) | MJS_TAG_BOOLEAN;
}

int mjs_get_bool(struct mjs* mjs, mjs_val_t v) {
    (void)mjs;
    if(mjs_is_boolean(v)) {
        return v & 1;
    } else {
        return 0;
    }
}

int mjs_is_boolean(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_BOOLEAN;
}

void* mjs_get_ptr(struct mjs* mjs, mjs_val_t v) {
    (void)mjs;
    if(!mjs_is_foreign(v)) {
        return NULL;
    }
    return (void*)(uintptr_t)(v & 0xFFFFFFFFFFFFUL);
}

int mjs_is_foreign(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_FOREIGN;
}

mjs_val_t mjs_mk_function(struct mjs* mjs, size_t off) {
    (void)mjs;
    return (mjs_val_t)off | MJS_TAG_FUNCTION;
}

int mjs_is_function(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_FUNCTION;
}

int mjs_is_object(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_OBJECT || (v & MJS_TAG_MASK) == MJS_TAG_ARRAY;
}

int mjs_is_object_based(mjs_val_t v) {
    return ((v & MJS_TAG_MASK) == MJS_TAG_OBJECT) || ((v & MJS_TAG_MASK) == MJS_TAG_ARRAY) ||
           ((v & MJS_TAG_MASK) == MJS_TAG_ARRAY_BUF_VIEW);
}

int mjs_is_array(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_ARRAY;
}

int mjs_is_array_buf(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_ARRAY_BUF;
}

int mjs_is_data_view(mjs_val_t v) {
    return (v & MJS_TAG_MASK) == MJS_TAG_ARRAY_BUF_VIEW;
}

int mjs_is_typed_array(mjs_val_t v) {
    return ((v & MJS_TAG_MASK) == MJS_TAG_ARRAY_BUF) ||
           ((v & MJS_TAG_MASK) == MJS_TAG_ARRAY_BUF_VIEW);
}

int mjs_is_string(mjs_val_t v) {
    uint64_t t = v & MJS_TAG_MASK;
    return t == MJS_TAG_STRING_I || t == MJS_TAG_STRING_F || t == MJS_TAG_STRING_O ||
           t == MJS_TAG_STRING_5 || t == MJS_TAG_STRING_D;
}

MJS_PRIVATE enum mjs_type mjs_get_type(mjs_val_t v) {
    int tag;
    if(mjs_is_number(v)) {
        return MJS_TYPE_NUMBER;
    }
    tag = (v & MJS_TAG_MASK) >> 48;
    switch(tag) {
    case MJS_TAG_FOREIGN >> 48:
        return MJS_TYPE_FOREIGN;
    case MJS_TAG_UNDEFINED >> 48:
        return MJS_TYPE_UNDEFINED;
    case MJS_TAG_OBJECT >> 48:
        return MJS_TYPE_OBJECT_GENERIC;
    case MJS_TAG_ARRAY >> 48:
        return MJS_TYPE_OBJECT_ARRAY;
    case MJS_TAG_FUNCTION >> 48:
        return MJS_TYPE_OBJECT_FUNCTION;
    case MJS_TAG_STRING_I >> 48:
    case MJS_TAG_STRING_O >> 48:
    case MJS_TAG_STRING_F >> 48:
    case MJS_TAG_STRING_D >> 48:
    case MJS_TAG_STRING_5 >> 48:
        return MJS_TYPE_STRING;
    case MJS_TAG_BOOLEAN >> 48:
        return MJS_TYPE_BOOLEAN;
    case MJS_TAG_NULL >> 48:
        return MJS_TYPE_NULL;
    case MJS_TAG_ARRAY_BUF >> 48:
        return MJS_TYPE_ARRAY_BUF;
    case MJS_TAG_ARRAY_BUF_VIEW >> 48:
        return MJS_TYPE_ARRAY_BUF_VIEW;
    default:
        abort();
        return MJS_TYPE_UNDEFINED;
    }
}

const char* mjs_typeof(mjs_val_t v) {
    return mjs_stringify_type(mjs_get_type(v));
}

MJS_PRIVATE const char* mjs_stringify_type(enum mjs_type t) {
    switch(t) {
    case MJS_TYPE_NUMBER:
        return "number";
    case MJS_TYPE_BOOLEAN:
        return "boolean";
    case MJS_TYPE_STRING:
        return "string";
    case MJS_TYPE_OBJECT_ARRAY:
        return "array";
    case MJS_TYPE_OBJECT_GENERIC:
        return "object";
    case MJS_TYPE_FOREIGN:
        return "foreign_ptr";
    case MJS_TYPE_OBJECT_FUNCTION:
        return "function";
    case MJS_TYPE_NULL:
        return "null";
    case MJS_TYPE_UNDEFINED:
        return "undefined";
    case MJS_TYPE_ARRAY_BUF:
        return "array_buf";
    case MJS_TYPE_ARRAY_BUF_VIEW:
        return "data_view";
    default:
        return "???";
    }
}
