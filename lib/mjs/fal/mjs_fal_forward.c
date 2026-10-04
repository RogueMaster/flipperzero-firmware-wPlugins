#include "mjs_fal.h"

/* Ordinary calls require a live context, which pins this exact engine image.
 * As with native mJS, destroying a context concurrently with its use is invalid. */
#define MJS_FAL_FORWARD(ret, name, params, args) \
    ret name params {                            \
        return mjs_fal_get_api()->name args;     \
    }
MJS_FAL_FUNCTIONS(MJS_FAL_FORWARD)
#undef MJS_FAL_FORWARD

struct mjs* mjs_create(void* context) {
    if(!mjs_fal_acquire()) return NULL;
    struct mjs* mjs = mjs_fal_get_api()->create(context);
    if(!mjs) mjs_fal_release();
    return mjs;
}

void mjs_destroy(struct mjs* mjs) {
    mjs_fal_get_api()->destroy(mjs);
    mjs_fal_release();
}

mjs_err_t
    mjs_call(struct mjs* mjs, mjs_val_t* res, mjs_val_t func, mjs_val_t this_val, int nargs, ...) {
    va_list ap;
    va_start(ap, nargs);
    mjs_err_t result = mjs_fal_get_api()->vcall(mjs, res, func, this_val, nargs, ap);
    va_end(ap);
    return result;
}

mjs_err_t mjs_set_errorf(struct mjs* mjs, mjs_err_t err, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    mjs_err_t result = mjs_fal_get_api()->vset_errorf(mjs, err, fmt, ap);
    va_end(ap);
    return result;
}

mjs_err_t mjs_prepend_errorf(struct mjs* mjs, mjs_err_t err, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    mjs_err_t result = mjs_fal_get_api()->vprepend_errorf(mjs, err, fmt, ap);
    va_end(ap);
    return result;
}
