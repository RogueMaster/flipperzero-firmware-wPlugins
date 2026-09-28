#include <flipper_application/flipper_application.h>

#include "../dfc_text_codec_plugin.h"

static const DfcTextHostApi* host_api;

static void bind(const DfcTextHostApi* api) {
    host_api = api;
}

#define FORWARD(ret, name, params, args) \
    ret name params {                    \
        return host_api->name args;      \
    }

FORWARD(
    uint8_t,
    dfc_credential_app_auth_commands,
    (const DfcCredential* c, const DfcApplication* a),
    (c, a))
FORWARD(bool, dfc_credential_clear, (DfcCredential * c), (c))
FORWARD(
    uint8_t*,
    dfc_credential_key,
    (DfcCredential * c, DfcApplication* a, size_t slot),
    (c, a, slot))
FORWARD(
    const uint8_t*,
    dfc_credential_key_const,
    (const DfcCredential* c, const DfcApplication* a, size_t slot),
    (c, a, slot))
FORWARD(
    uint8_t*,
    dfc_credential_key_in_set,
    (DfcCredential * c, DfcApplication* a, size_t set, size_t slot),
    (c, a, set, slot))
FORWARD(
    const uint8_t*,
    dfc_credential_key_in_set_const,
    (const DfcCredential* c, const DfcApplication* a, size_t set, size_t slot),
    (c, a, set, slot))
FORWARD(size_t, dfc_credential_key_length, (uint8_t settings), (settings))
FORWARD(
    bool,
    dfc_credential_key_sets_resize,
    (DfcCredential * c,
     DfcApplication* a,
     size_t sets,
     size_t keys,
     size_t active_len,
     size_t max_len),
    (c, a, sets, keys, active_len, max_len))
FORWARD(
    uint8_t*,
    dfc_credential_key_version_in_set,
    (DfcApplication * a, size_t set, size_t slot),
    (a, set, slot))
FORWARD(
    bool,
    dfc_credential_keys_resize,
    (DfcCredential * c, DfcApplication* a, size_t count, size_t length),
    (c, a, count, length))
FORWARD(uint8_t, dfc_credential_picc_auth_commands, (const DfcCredential* c), (c))

void dfc_credential_reset_application(DfcApplication* a) {
    host_api->dfc_credential_reset_application(a);
}

FORWARD(
    DfcDerStatus,
    dfc_der_decode,
    (DfcCredential * c, const uint8_t* data, size_t len),
    (c, data, len))
FORWARD(const char*, dfc_der_status_name, (DfcDerStatus status), (status))
FORWARD(DfcDerStatus, dfc_der_validate_model, (const DfcCredential* c), (c))
FORWARD(uint8_t*, dfc_file_data, (DfcCredential * c, DfcFile* file), (c, file))
FORWARD(
    const uint8_t*,
    dfc_file_data_const,
    (const DfcCredential* c, const DfcFile* file),
    (c, file))
FORWARD(bool, dfc_file_resize, (DfcCredential * c, DfcFile* file, size_t len), (c, file, len))

#undef FORWARD

static const DfcTextCodecPlugin codec = {
    .bind = bind,
#if defined(DFC_TEXT_IMPORT_PLUGIN)
    .parse = dfc_text_parse,
#else
    .write = dfc_text_write,
#endif
    .status_name = dfc_text_status_name,
};

static const FlipperAppPluginDescriptor descriptor = {
#if defined(DFC_TEXT_IMPORT_PLUGIN)
    .appid = DFC_TEXT_IMPORT_PLUGIN_ID,
#else
    .appid = DFC_TEXT_EXPORT_PLUGIN_ID,
#endif
    .ep_api_version = DFC_TEXT_CODEC_PLUGIN_API_VERSION,
    .entry_point = &codec,
};

const FlipperAppPluginDescriptor* dfc_text_codec_ep(void) {
    return &descriptor;
}
