#include "flippass_otp.h"

#include "flippass.h"
#include "flippass_db.h"
#include "kdbx/memzero.h"
#include "plugins/flippass_otp_plugin.h"

#include <datetime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    FlipPassOtpKind kind;
    FlipPassOtpSecretEncoding encoding;
} FlipPassOtpSecretFieldDef;

static const FlipPassOtpSecretFieldDef flippass_otp_secret_fields[] = {
    {"HmacOtp-Secret", FlipPassOtpKindHmac, FlipPassOtpSecretEncodingText},
    {"HmacOtp-Secret-Hex", FlipPassOtpKindHmac, FlipPassOtpSecretEncodingHex},
    {"HmacOtp-Secret-Base32", FlipPassOtpKindHmac, FlipPassOtpSecretEncodingBase32},
    {"HmacOtp-Secret-Base64", FlipPassOtpKindHmac, FlipPassOtpSecretEncodingBase64},
    {"TimeOtp-Secret", FlipPassOtpKindTime, FlipPassOtpSecretEncodingText},
    {"TimeOtp-Secret-Hex", FlipPassOtpKindTime, FlipPassOtpSecretEncodingHex},
    {"TimeOtp-Secret-Base32", FlipPassOtpKindTime, FlipPassOtpSecretEncodingBase32},
    {"TimeOtp-Secret-Base64", FlipPassOtpKindTime, FlipPassOtpSecretEncodingBase64},
};

static char flippass_otp_ascii_upper(char value) {
    if(value >= 'a' && value <= 'z') {
        value = (char)(value - 'a' + 'A');
    }
    return value;
}

static bool flippass_otp_str_equals_ignore_case(const char* a, const char* b) {
    if(a == NULL || b == NULL) {
        return false;
    }

    while(*a != '\0' && *b != '\0') {
        if(flippass_otp_ascii_upper(*a) != flippass_otp_ascii_upper(*b)) {
            return false;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static bool
    flippass_otp_token_n_equals(const char* token, size_t token_len, const char* expected) {
    size_t index = 0U;

    if(token == NULL || expected == NULL) {
        return false;
    }

    while(index < token_len && expected[index] != '\0') {
        if(flippass_otp_ascii_upper(token[index]) != flippass_otp_ascii_upper(expected[index])) {
            return false;
        }
        index++;
    }

    return index == token_len && expected[index] == '\0';
}

static bool flippass_otp_secret_field_match(
    const char* key,
    FlipPassOtpKind kind,
    FlipPassOtpSecretEncoding* out_encoding) {
    if(key == NULL) {
        return false;
    }

    for(size_t index = 0U; index < COUNT_OF(flippass_otp_secret_fields); index++) {
        const FlipPassOtpSecretFieldDef* field = &flippass_otp_secret_fields[index];
        if(field->kind == kind && strcmp(key, field->name) == 0) {
            if(out_encoding != NULL) {
                *out_encoding = field->encoding;
            }
            return true;
        }
    }

    return false;
}

static bool flippass_otp_parse_u64(const char* text, uint64_t* out_value) {
    uint64_t value = 0ULL;

    if(text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(*cursor < '0' || *cursor > '9') {
            return false;
        }
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if(value > (UINT64_MAX - digit) / 10ULL) {
            return false;
        }
        value = (value * 10ULL) + digit;
    }

    *out_value = value;
    return true;
}

static bool flippass_otp_parse_u32(const char* text, uint32_t* out_value) {
    uint64_t value = 0ULL;
    if(!flippass_otp_parse_u64(text, &value) || value > UINT32_MAX) {
        return false;
    }
    *out_value = (uint32_t)value;
    return true;
}

static bool flippass_otp_parse_algorithm(const char* text, FlipPassOtpAlgorithm* out_algorithm) {
    if(text == NULL || text[0] == '\0') {
        *out_algorithm = FlipPassOtpAlgorithmSha1;
        return true;
    }

    if(flippass_otp_str_equals_ignore_case(text, "HMAC-SHA-1")) {
        *out_algorithm = FlipPassOtpAlgorithmSha1;
        return true;
    }
    if(flippass_otp_str_equals_ignore_case(text, "HMAC-SHA-256")) {
        *out_algorithm = FlipPassOtpAlgorithmSha256;
        return true;
    }
    if(flippass_otp_str_equals_ignore_case(text, "HMAC-SHA-512")) {
        *out_algorithm = FlipPassOtpAlgorithmSha512;
        return true;
    }

    return false;
}

static KDBXCustomField* flippass_otp_find_custom_field(KDBXEntry* entry, const char* key) {
    if(entry == NULL || key == NULL) {
        return NULL;
    }

    for(KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        if(field->key != NULL && strcmp(field->key, key) == 0) {
            return field;
        }
    }

    return NULL;
}

static bool flippass_otp_ensure_custom_field_value(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    if(field == NULL) {
        return false;
    }

    if(!flippass_db_ensure_custom_field(app, entry, field, error)) {
        return false;
    }

    if(field->value == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not load the OTP custom field.");
        }
        return false;
    }

    return true;
}

static bool flippass_otp_update_counter_field(
    App* app,
    KDBXEntry* entry,
    uint64_t next_counter,
    FuriString* error) {
    char counter_text[FLIPPASS_OTP_COUNTER_TEXT_SIZE];
    KDBXCustomField* field = flippass_otp_find_custom_field(entry, "HmacOtp-Counter");

    snprintf(counter_text, sizeof(counter_text), "%llu", (unsigned long long)next_counter);

    if(field != NULL) {
        return flippass_db_update_custom_field(
            app, entry, field, "HmacOtp-Counter", counter_text, false, error);
    }

    return flippass_db_create_custom_field(
        app, entry, "HmacOtp-Counter", counter_text, false, NULL, error);
}

static const FlipPassOtpPluginV1* flippass_otp_plugin_load(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOtp,
        NULL,
        FLIPPASS_OTP_PLUGIN_APP_ID,
        FLIPPASS_OTP_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass OTP plugin is unavailable.");
        }
        return NULL;
    }

    const FlipPassOtpPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_OTP_PLUGIN_API_VERSION || plugin->generate == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass OTP plugin has an incompatible API.");
        }
        flippass_module_unload(app, FlipPassModuleSlotOtp);
        return NULL;
    }

    return plugin;
}

static uint64_t flippass_otp_unix_time_now(void) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    return (uint64_t)datetime_datetime_to_timestamp(&now);
}

const char* flippass_otp_kind_label(FlipPassOtpKind kind) {
    switch(kind) {
    case FlipPassOtpKindHmac:
        return "HMACOTP";
    case FlipPassOtpKindTime:
        return "TIMEOTP";
    case FlipPassOtpKindNone:
    default:
        return "OTP";
    }
}

const char*
    flippass_otp_secret_field_name(FlipPassOtpKind kind, FlipPassOtpSecretEncoding encoding) {
    for(size_t index = 0U; index < COUNT_OF(flippass_otp_secret_fields); index++) {
        const FlipPassOtpSecretFieldDef* field = &flippass_otp_secret_fields[index];
        if(field->kind == kind && field->encoding == encoding) {
            return field->name;
        }
    }

    return NULL;
}

const char* flippass_otp_algorithm_field_value(FlipPassOtpAlgorithm algorithm) {
    switch(algorithm) {
    case FlipPassOtpAlgorithmSha256:
        return "HMAC-SHA-256";
    case FlipPassOtpAlgorithmSha512:
        return "HMAC-SHA-512";
    case FlipPassOtpAlgorithmSha1:
    default:
        return "HMAC-SHA-1";
    }
}

bool flippass_otp_custom_field_is_reserved(const char* key) {
    if(key == NULL) {
        return false;
    }

    for(size_t index = 0U; index < COUNT_OF(flippass_otp_secret_fields); index++) {
        if(strcmp(key, flippass_otp_secret_fields[index].name) == 0) {
            return true;
        }
    }

    return strcmp(key, "HmacOtp-Counter") == 0 || strcmp(key, "TimeOtp-Length") == 0 ||
           strcmp(key, "TimeOtp-Period") == 0 || strcmp(key, "TimeOtp-Algorithm") == 0;
}

bool flippass_otp_entry_has_kind(const KDBXEntry* entry, FlipPassOtpKind kind) {
    if(entry == NULL || kind == FlipPassOtpKindNone) {
        return false;
    }

    for(const KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        FlipPassOtpSecretEncoding encoding = FlipPassOtpSecretEncodingText;
        if(flippass_otp_secret_field_match(field->key, kind, &encoding)) {
            return true;
        }
    }

    return false;
}

bool flippass_otp_entry_has_any_config(const KDBXEntry* entry) {
    return flippass_otp_entry_has_kind(entry, FlipPassOtpKindHmac) ||
           flippass_otp_entry_has_kind(entry, FlipPassOtpKindTime);
}

bool flippass_otp_draft_is_reserved(const FlipPassEditorCustomFieldDraft* draft) {
    return draft != NULL && flippass_otp_custom_field_is_reserved(draft->name);
}

bool flippass_otp_drafts_have_any_config(const FlipPassEditorCustomFieldDraft* drafts) {
    for(const FlipPassEditorCustomFieldDraft* draft = drafts; draft != NULL; draft = draft->next) {
        if(draft->name == NULL) {
            continue;
        }
        FlipPassOtpSecretEncoding encoding = FlipPassOtpSecretEncodingText;
        if(flippass_otp_secret_field_match(draft->name, FlipPassOtpKindHmac, &encoding) ||
           flippass_otp_secret_field_match(draft->name, FlipPassOtpKindTime, &encoding)) {
            return true;
        }
    }

    return false;
}

void flippass_otp_config_init(FlipPassOtpConfig* config, FlipPassOtpKind kind) {
    furi_assert(config);
    memset(config, 0, sizeof(*config));
    config->kind = kind;
    config->secret_encoding = FlipPassOtpSecretEncodingText;
    config->algorithm = FlipPassOtpAlgorithmSha1;
    config->digits = FLIPPASS_OTP_DEFAULT_DIGITS;
    config->period = FLIPPASS_OTP_DEFAULT_PERIOD;
    config->counter = FLIPPASS_OTP_DEFAULT_COUNTER;
}

bool flippass_otp_load_config(
    App* app,
    KDBXEntry* entry,
    FlipPassOtpKind kind,
    FlipPassOtpConfig* config,
    FuriString* error) {
    furi_assert(config);
    flippass_otp_config_init(config, kind);

    if(entry == NULL || kind == FlipPassOtpKindNone) {
        if(error != NULL) {
            furi_string_set_str(error, "No entry is selected for OTP.");
        }
        return false;
    }

    for(KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        FlipPassOtpSecretEncoding encoding = FlipPassOtpSecretEncodingText;
        if(flippass_otp_secret_field_match(field->key, kind, &encoding)) {
            if(config->has_secret) {
                config->too_many_secrets = true;
                continue;
            }
            if(!flippass_otp_ensure_custom_field_value(app, entry, field, error)) {
                return false;
            }
            config->secret_encoding = encoding;
            config->secret = field->value;
            config->secret_field_name = field->key;
            config->secret_field = field;
            config->has_secret = true;
        } else if(
            kind == FlipPassOtpKindHmac && field->key != NULL &&
            strcmp(field->key, "HmacOtp-Counter") == 0) {
            uint64_t counter = 0ULL;
            if(!flippass_otp_ensure_custom_field_value(app, entry, field, error)) {
                return false;
            }
            if(!flippass_otp_parse_u64(field->value, &counter)) {
                if(error != NULL) {
                    furi_string_set_str(error, "HmacOtp-Counter must be a decimal number.");
                }
                return false;
            }
            config->counter = counter;
            config->counter_field = field;
        } else if(
            kind == FlipPassOtpKindTime && field->key != NULL &&
            strcmp(field->key, "TimeOtp-Length") == 0) {
            uint32_t digits = 0U;
            if(!flippass_otp_ensure_custom_field_value(app, entry, field, error)) {
                return false;
            }
            if(!flippass_otp_parse_u32(field->value, &digits) || digits == 0U ||
               digits > FLIPPASS_OTP_MAX_DIGITS) {
                if(error != NULL) {
                    furi_string_set_str(error, "TimeOtp-Length must be between 1 and 8.");
                }
                return false;
            }
            config->digits = (uint8_t)digits;
        } else if(
            kind == FlipPassOtpKindTime && field->key != NULL &&
            strcmp(field->key, "TimeOtp-Period") == 0) {
            uint32_t period = 0U;
            if(!flippass_otp_ensure_custom_field_value(app, entry, field, error)) {
                return false;
            }
            if(!flippass_otp_parse_u32(field->value, &period) || period == 0U) {
                if(error != NULL) {
                    furi_string_set_str(
                        error, "TimeOtp-Period must be a positive decimal number.");
                }
                return false;
            }
            config->period = period;
        } else if(
            kind == FlipPassOtpKindTime && field->key != NULL &&
            strcmp(field->key, "TimeOtp-Algorithm") == 0) {
            if(!flippass_otp_ensure_custom_field_value(app, entry, field, error)) {
                return false;
            }
            if(!flippass_otp_parse_algorithm(field->value, &config->algorithm)) {
                if(error != NULL) {
                    furi_string_set_str(error, "TimeOtp-Algorithm is unsupported.");
                }
                return false;
            }
        }
    }

    if(config->too_many_secrets) {
        if(error != NULL) {
            furi_string_set_str(error, "This entry has more than one OTP secret field.");
        }
        return false;
    }

    if(!config->has_secret || config->secret == NULL || config->secret[0] == '\0') {
        if(error != NULL) {
            furi_string_set_str(error, "This entry does not contain the required OTP secret.");
        }
        return false;
    }

    config->valid = true;
    return true;
}

bool flippass_otp_generate_code(
    App* app,
    KDBXEntry* entry,
    FlipPassOtpKind kind,
    bool advance_hotp,
    char out_code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U],
    FuriString* error) {
    FlipPassOtpConfig config;
    FlipPassOtpPluginRequestV1 request = {0};
    FlipPassOtpPluginResultV1 result = {0};
    bool ok = false;

    if(out_code == NULL) {
        return false;
    }
    out_code[0] = '\0';

    if(!flippass_otp_load_config(app, entry, kind, &config, error)) {
        return false;
    }

    if(kind == FlipPassOtpKindHmac && advance_hotp && config.counter == UINT64_MAX) {
        if(error != NULL) {
            furi_string_set_str(error, "HmacOtp-Counter cannot be incremented further.");
        }
        return false;
    }

    FuriString* load_error = furi_string_alloc();
    const FlipPassOtpPluginV1* plugin = flippass_otp_plugin_load(app, load_error);
    if(plugin == NULL) {
        if(error != NULL) {
            furi_string_set(error, load_error);
        }
        furi_string_free(load_error);
        return false;
    }

    request.api_version = FLIPPASS_OTP_PLUGIN_API_VERSION;
    request.kind = kind;
    request.secret_encoding = config.secret_encoding;
    request.algorithm = (kind == FlipPassOtpKindHmac) ? FlipPassOtpAlgorithmSha1 :
                                                        config.algorithm;
    request.secret = config.secret;
    request.digits = (kind == FlipPassOtpKindHmac) ? FLIPPASS_OTP_DEFAULT_DIGITS : config.digits;
    request.period = config.period;
    request.counter = config.counter;
    request.unix_time = flippass_otp_unix_time_now();
    request.time_zone_offset_seconds = (int32_t)app->otp_time_zone_minutes * 60;

    ok = plugin->generate(&request, &result, error);
    flippass_module_unload(app, FlipPassModuleSlotOtp);
    furi_string_free(load_error);

    if(!ok) {
        return false;
    }

    snprintf(out_code, FLIPPASS_OTP_CODE_MAX_CHARS + 1U, "%s", result.code);

    if(kind == FlipPassOtpKindHmac && advance_hotp) {
        ok = flippass_otp_update_counter_field(app, entry, result.next_counter, error);
    }

    memzero(&result, sizeof(result));
    return ok;
}

bool flippass_otp_resolve_autotype_sequence(
    App* app,
    KDBXEntry* entry,
    const char* sequence,
    FuriString* out_sequence,
    FuriString* error) {
    bool changed = false;
    size_t copy_start = 0U;
    size_t index = 0U;

    if(sequence == NULL || out_sequence == NULL) {
        return false;
    }

    furi_string_reset(out_sequence);
    while(sequence[index] != '\0') {
        if(sequence[index] != '{') {
            index++;
            continue;
        }

        size_t token_start = index + 1U;
        while(sequence[token_start] == ' ' || sequence[token_start] == '\t') {
            token_start++;
        }

        size_t token_end = token_start;
        while(sequence[token_end] != '\0' && sequence[token_end] != '}') {
            token_end++;
        }
        if(sequence[token_end] != '}') {
            break;
        }

        size_t name_end = token_start;
        while(name_end < token_end && sequence[name_end] != ' ' && sequence[name_end] != '\t') {
            name_end++;
        }

        size_t params_start = name_end;
        while(params_start < token_end &&
              (sequence[params_start] == ' ' || sequence[params_start] == '\t')) {
            params_start++;
        }

        const bool no_params = params_start == token_end;
        FlipPassOtpKind kind = FlipPassOtpKindNone;
        if(no_params && flippass_otp_token_n_equals(
                            sequence + token_start, name_end - token_start, "TIMEOTP")) {
            kind = FlipPassOtpKindTime;
        } else if(
            no_params && flippass_otp_token_n_equals(
                             sequence + token_start, name_end - token_start, "HMACOTP")) {
            kind = FlipPassOtpKindHmac;
        }

        if(kind != FlipPassOtpKindNone) {
            char code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U];
            furi_string_cat_printf(
                out_sequence, "%.*s", (int)(index - copy_start), sequence + copy_start);
            if(!flippass_otp_generate_code(
                   app, entry, kind, kind == FlipPassOtpKindHmac, code, error)) {
                return false;
            }
            furi_string_cat_str(out_sequence, code);
            memzero(code, sizeof(code));
            changed = true;
            index = token_end + 1U;
            copy_start = index;
            continue;
        }

        index = token_end + 1U;
    }

    if(changed) {
        furi_string_cat_str(out_sequence, sequence + copy_start);
    } else {
        furi_string_set_str(out_sequence, sequence);
    }

    return true;
}
