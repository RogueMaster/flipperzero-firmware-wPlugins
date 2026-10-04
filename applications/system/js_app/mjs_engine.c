#include <mjs/fal/mjs_fal_api.h>
#include <mjs/mjs_variadic.h>
#include <flipper_application/flipper_application.h>

static const MjsFalApi mjs_engine_api = {
    .size = sizeof(MjsFalApi),
    .create = mjs_create,
    .destroy = mjs_destroy,
    .vcall = mjs_vcall,
    .vset_errorf = mjs_vset_errorf,
    .vprepend_errorf = mjs_vprepend_errorf,
#define MJS_FAL_ENTRY(ret, name, params, args) .name = name,
    MJS_FAL_FUNCTIONS(MJS_FAL_ENTRY)
#undef MJS_FAL_ENTRY
};

static const FlipperAppPluginDescriptor mjs_engine_descriptor = {
    .appid = MJS_FAL_APP_ID,
    .ep_api_version = MJS_FAL_API_VERSION,
    .entry_point = &mjs_engine_api,
};

const FlipperAppPluginDescriptor* mjs_engine_ep(void) {
    return &mjs_engine_descriptor;
}
