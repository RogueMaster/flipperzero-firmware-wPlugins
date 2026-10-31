#include "flippass_password_gen_plugin.h"

#include "../kdbx/memzero.h"

#include <furi_hal_random.h>
#include <storage/storage.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool active;
    FlipPassPasswordGenPluginRequestV1 request;
    uint8_t stream[32];
    size_t stream_offset;
} FlipPassPasswordGenState;

static FlipPassPasswordGenState flippass_password_gen_state = {0};

static const char flippass_password_gen_lower[] = "abcdefghijklmnopqrstuvwxyz";
static const char flippass_password_gen_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char flippass_password_gen_digits[] = "0123456789";
static const char flippass_password_gen_symbols[] = "!@#$%^&*()-_=+[]{};:,.?/";
static const char flippass_password_gen_hex[] = "0123456789ABCDEF";
static const char flippass_password_gen_octal[] = "01234567";
static const char flippass_password_gen_binary[] = "01";

typedef struct {
    const char* const* tokens;
    size_t count;
} FlipPassPasswordGenUtf8Class;

typedef enum {
    FlipPassPasswordGenUtf8ClassLower = 0,
    FlipPassPasswordGenUtf8ClassUpper,
    FlipPassPasswordGenUtf8ClassDigits,
    FlipPassPasswordGenUtf8ClassSymbols,
    FlipPassPasswordGenUtf8ClassExtended,
    FlipPassPasswordGenUtf8ClassCount,
} FlipPassPasswordGenUtf8ClassId;

static const char* const flippass_password_gen_utf8_lower[] = {
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
};

static const char* const flippass_password_gen_utf8_upper[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
};

static const char* const flippass_password_gen_utf8_digits[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
};

static const char* const flippass_password_gen_utf8_symbols[] = {
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "_",
    "=", "+", "[", "]", "{", "}", ";", ":", ",", ".", "?", "/",
};

static const char* const flippass_password_gen_utf8_extended[] = {
    "\xC2\xA1", "\xC2\xA3", "\xC2\xA5", "\xC2\xA7", "\xC2\xB0", "\xC2\xB5", "\xC2\xBF", "\xC3\x80",
    "\xC3\x81", "\xC3\x86", "\xC3\x87", "\xC3\x89", "\xC3\x8D", "\xC3\x91", "\xC3\x93", "\xC3\x98",
    "\xC3\x9A", "\xC3\x9C", "\xC3\x9F", "\xC3\xA0", "\xC3\xA1", "\xC3\xA6", "\xC3\xA7", "\xC3\xA9",
    "\xC3\xAD", "\xC3\xB1", "\xC3\xB3", "\xC3\xB8", "\xC3\xBA", "\xC3\xBC",
};

static const FlipPassPasswordGenUtf8Class flippass_password_gen_utf8_classes[] = {
    [FlipPassPasswordGenUtf8ClassLower] =
        {
            flippass_password_gen_utf8_lower,
            COUNT_OF(flippass_password_gen_utf8_lower),
        },
    [FlipPassPasswordGenUtf8ClassUpper] =
        {
            flippass_password_gen_utf8_upper,
            COUNT_OF(flippass_password_gen_utf8_upper),
        },
    [FlipPassPasswordGenUtf8ClassDigits] =
        {
            flippass_password_gen_utf8_digits,
            COUNT_OF(flippass_password_gen_utf8_digits),
        },
    [FlipPassPasswordGenUtf8ClassSymbols] =
        {
            flippass_password_gen_utf8_symbols,
            COUNT_OF(flippass_password_gen_utf8_symbols),
        },
    [FlipPassPasswordGenUtf8ClassExtended] =
        {
            flippass_password_gen_utf8_extended,
            COUNT_OF(flippass_password_gen_utf8_extended),
        },
};

static bool flippass_password_gen_request_valid(
    const FlipPassPasswordGenPluginRequestV1* request,
    FuriString* error) {
    if(request == NULL || request->api_version != FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass password generator received an invalid request.");
        }
        return false;
    }

    if(request->target == FlipPassPasswordGenTargetNone) {
        if(error != NULL) {
            furi_string_set_str(error, "No editable secret field is selected.");
        }
        return false;
    }

    if(request->length == 0U || request->length > FLIPPASS_PASSWORD_GEN_MAX_LENGTH) {
        if(error != NULL) {
            furi_string_set_str(error, "Password length is out of range.");
        }
        return false;
    }

    if(request->charset > FLIPPASS_PASSWORD_GEN_CHARSET_MAX) {
        if(error != NULL) {
            furi_string_set_str(error, "Password complexity is unsupported.");
        }
        return false;
    }

    return true;
}

static void flippass_password_gen_reset_state(void) {
    memzero(&flippass_password_gen_state, sizeof(flippass_password_gen_state));
    flippass_password_gen_state.stream_offset = sizeof(flippass_password_gen_state.stream);
}

static bool flippass_password_gen_begin(
    const FlipPassPasswordGenPluginRequestV1* request,
    FuriString* error) {
    if(!flippass_password_gen_request_valid(request, error)) {
        return false;
    }

    flippass_password_gen_reset_state();
    flippass_password_gen_state.request = *request;
    flippass_password_gen_state.active = true;
    return true;
}

static uint8_t flippass_password_gen_next_byte(void) {
    if(flippass_password_gen_state.stream_offset >= sizeof(flippass_password_gen_state.stream)) {
        furi_hal_random_fill_buf(
            flippass_password_gen_state.stream, sizeof(flippass_password_gen_state.stream));
        flippass_password_gen_state.stream_offset = 0U;
    }

    return flippass_password_gen_state.stream[flippass_password_gen_state.stream_offset++];
}

static size_t flippass_password_gen_uniform(size_t count) {
    if(count <= 1U) {
        return 0U;
    }

    const uint16_t limit = (uint16_t)(256U - (256U % count));
    uint8_t byte = 0U;
    do {
        byte = flippass_password_gen_next_byte();
    } while(byte >= limit);

    return byte % count;
}

static void flippass_password_gen_shuffle(char* text, size_t length) {
    if(length <= 1U) {
        return;
    }

    for(size_t index = length - 1U; index > 0U; index--) {
        const size_t swap_index = flippass_password_gen_uniform(index + 1U);
        const char tmp = text[index];
        text[index] = text[swap_index];
        text[swap_index] = tmp;
    }
}

static void flippass_password_gen_shuffle_u8(uint8_t* values, size_t length) {
    if(values == NULL || length <= 1U) {
        return;
    }

    for(size_t index = length - 1U; index > 0U; index--) {
        const size_t swap_index = flippass_password_gen_uniform(index + 1U);
        const uint8_t tmp = values[index];
        values[index] = values[swap_index];
        values[swap_index] = tmp;
    }
}

static size_t flippass_password_gen_append_chars(char* out, size_t out_size, const char* chars) {
    size_t out_len = strlen(out);

    for(const char* cursor = chars; cursor != NULL && *cursor != '\0' && out_len + 1U < out_size;
        cursor++) {
        out[out_len++] = *cursor;
    }
    out[out_len] = '\0';
    return out_len;
}

static size_t flippass_password_gen_build_classes(
    const FlipPassPasswordGenPluginRequestV1* request,
    const char** classes,
    size_t max_classes,
    char* combined,
    size_t combined_size) {
    size_t class_count = 0U;
    combined[0] = '\0';

    switch(request->charset) {
    case FlipPassPasswordGenCharsetAlnum:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        classes[class_count++] = flippass_password_gen_digits;
        break;
    case FlipPassPasswordGenCharsetAlpha:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        break;
    case FlipPassPasswordGenCharsetSymbols:
        classes[class_count++] = flippass_password_gen_symbols;
        break;
    case FlipPassPasswordGenCharsetNumeric:
        classes[class_count++] = flippass_password_gen_digits;
        break;
    case FlipPassPasswordGenCharsetHex:
        classes[class_count++] = flippass_password_gen_hex;
        break;
    case FlipPassPasswordGenCharsetOctal:
        classes[class_count++] = flippass_password_gen_octal;
        break;
    case FlipPassPasswordGenCharsetBinary:
        classes[class_count++] = flippass_password_gen_binary;
        break;
    case FlipPassPasswordGenCharsetFull:
    default:
        classes[class_count++] = flippass_password_gen_lower;
        classes[class_count++] = flippass_password_gen_upper;
        classes[class_count++] = flippass_password_gen_digits;
        classes[class_count++] = flippass_password_gen_symbols;
        break;
    }

    if(class_count > max_classes) {
        class_count = max_classes;
    }

    for(size_t index = 0U; index < class_count; index++) {
        flippass_password_gen_append_chars(combined, combined_size, classes[index]);
    }

    return class_count;
}

static bool flippass_password_gen_load_layout_file(const char* path, uint16_t layout[128]) {
    if(path == NULL || path[0] == '\0' || layout == NULL) {
        return false;
    }

    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* layout_file = storage_file_alloc(storage);

    if(storage_file_open(layout_file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(layout_file, layout, 128U * sizeof(uint16_t)) ==
           128U * sizeof(uint16_t)) {
        loaded = true;
    }

    storage_file_close(layout_file);
    storage_file_free(layout_file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}

static void flippass_password_gen_filter_layout_chars(
    const char* source,
    const uint16_t layout[128],
    char* out,
    size_t out_size) {
    size_t out_len = 0U;

    if(source == NULL || layout == NULL || out == NULL || out_size == 0U) {
        return;
    }

    for(const char* cursor = source; *cursor != '\0' && out_len + 1U < out_size; cursor++) {
        const uint8_t index = (uint8_t)*cursor;
        if(index < 128U && layout[index] != 0U) {
            out[out_len++] = *cursor;
        }
    }
    out[out_len] = '\0';
}

static size_t flippass_password_gen_build_layout_classes(
    const FlipPassPasswordGenPluginRequestV1* request,
    const char** classes,
    size_t max_classes,
    char* lower,
    size_t lower_size,
    char* upper,
    size_t upper_size,
    char* digits,
    size_t digits_size,
    char* symbols,
    size_t symbols_size,
    char* combined,
    size_t combined_size,
    FuriString* error) {
    uint16_t layout[128];
    size_t class_count = 0U;

    if(combined != NULL && combined_size > 0U) {
        combined[0] = '\0';
    }

    if(!flippass_password_gen_load_layout_file(request->keyboard_layout_path, layout)) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected keyboard layout could not be loaded.");
        }
        return 0U;
    }

    flippass_password_gen_filter_layout_chars(
        flippass_password_gen_lower, layout, lower, lower_size);
    flippass_password_gen_filter_layout_chars(
        flippass_password_gen_upper, layout, upper, upper_size);
    flippass_password_gen_filter_layout_chars(
        flippass_password_gen_digits, layout, digits, digits_size);
    flippass_password_gen_filter_layout_chars(
        flippass_password_gen_symbols, layout, symbols, symbols_size);

    if(lower[0] != '\0' && class_count < max_classes) {
        classes[class_count++] = lower;
    }
    if(upper[0] != '\0' && class_count < max_classes) {
        classes[class_count++] = upper;
    }
    if(digits[0] != '\0' && class_count < max_classes) {
        classes[class_count++] = digits;
    }
    if(symbols[0] != '\0' && class_count < max_classes) {
        classes[class_count++] = symbols;
    }

    for(size_t index = 0U; index < class_count; index++) {
        flippass_password_gen_append_chars(combined, combined_size, classes[index]);
    }

    memzero(layout, sizeof(layout));
    return class_count;
}

static const char* flippass_password_gen_utf8_pick_token(FlipPassPasswordGenUtf8ClassId class_id) {
    if(class_id >= COUNT_OF(flippass_password_gen_utf8_classes)) {
        class_id = FlipPassPasswordGenUtf8ClassLower;
    }

    const FlipPassPasswordGenUtf8Class* cls = &flippass_password_gen_utf8_classes[class_id];
    if(cls->count == 0U || cls->tokens == NULL) {
        return "a";
    }

    return cls->tokens[flippass_password_gen_uniform(cls->count)];
}

static const char* flippass_password_gen_utf8_pick_ascii_token(void) {
    static const FlipPassPasswordGenUtf8ClassId ascii_classes[] = {
        FlipPassPasswordGenUtf8ClassLower,
        FlipPassPasswordGenUtf8ClassUpper,
        FlipPassPasswordGenUtf8ClassDigits,
        FlipPassPasswordGenUtf8ClassSymbols,
    };

    const FlipPassPasswordGenUtf8ClassId class_id =
        ascii_classes[flippass_password_gen_uniform(COUNT_OF(ascii_classes))];
    return flippass_password_gen_utf8_pick_token(class_id);
}

static bool flippass_password_gen_append_token_reserved(
    char* out,
    size_t* out_index,
    const char* token,
    size_t remaining_codepoints) {
    if(out == NULL || out_index == NULL || token == NULL) {
        return false;
    }

    const size_t token_len = strlen(token);
    if(token_len == 0U ||
       *out_index + token_len + remaining_codepoints > FLIPPASS_PASSWORD_GEN_MAX_LENGTH) {
        return false;
    }

    memcpy(out + *out_index, token, token_len);
    *out_index += token_len;
    out[*out_index] = '\0';
    return true;
}

static bool flippass_password_gen_generate_utf8_password(
    const FlipPassPasswordGenPluginRequestV1* request,
    char out[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U],
    FuriString* error) {
    uint8_t classes[FLIPPASS_PASSWORD_GEN_MAX_LENGTH];
    size_t out_index = 0U;
    const size_t class_count = COUNT_OF(flippass_password_gen_utf8_classes);

    if(request->length == 0U || request->length > COUNT_OF(classes)) {
        if(error != NULL) {
            furi_string_set_str(error, "Password length is out of range.");
        }
        return false;
    }

    memset(out, 0, FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U);
    memset(classes, 0, sizeof(classes));

    for(size_t index = 0U; index < request->length; index++) {
        if(request->length >= class_count && index < class_count) {
            classes[index] = (uint8_t)index;
        } else {
            classes[index] = (uint8_t)flippass_password_gen_uniform(class_count);
        }
    }
    flippass_password_gen_shuffle_u8(classes, request->length);

    for(size_t index = 0U; index < request->length; index++) {
        const size_t remaining_codepoints = request->length - index - 1U;
        const char* token =
            flippass_password_gen_utf8_pick_token((FlipPassPasswordGenUtf8ClassId)classes[index]);
        if(!flippass_password_gen_append_token_reserved(
               out, &out_index, token, remaining_codepoints)) {
            token = flippass_password_gen_utf8_pick_ascii_token();
            if(!flippass_password_gen_append_token_reserved(
                   out, &out_index, token, remaining_codepoints)) {
                if(error != NULL) {
                    furi_string_set_str(error, "Password complexity exceeded the output buffer.");
                }
                memzero(classes, sizeof(classes));
                return false;
            }
        }
    }

    memzero(classes, sizeof(classes));
    return true;
}

static bool flippass_password_gen_generate_password_from_classes(
    const FlipPassPasswordGenPluginRequestV1* request,
    const char** classes,
    size_t class_count,
    const char* combined,
    char out[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U],
    FuriString* error) {
    size_t out_index = 0U;
    const size_t combined_len = strlen(combined);
    if(class_count == 0U || combined_len == 0U) {
        if(error != NULL) {
            furi_string_set_str(error, "Password complexity produced an empty character set.");
        }
        return false;
    }

    memset(out, 0, FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U);

    if(class_count > 1U && request->length >= class_count) {
        for(size_t class_index = 0U; class_index < class_count; class_index++) {
            const char* cls = classes[class_index];
            out[out_index++] = cls[flippass_password_gen_uniform(strlen(cls))];
        }
    }

    while(out_index < request->length) {
        out[out_index++] = combined[flippass_password_gen_uniform(combined_len)];
    }
    out[out_index] = '\0';

    flippass_password_gen_shuffle(out, out_index);
    return true;
}

static bool flippass_password_gen_generate_layout_password(
    const FlipPassPasswordGenPluginRequestV1* request,
    char out[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U],
    FuriString* error) {
    if(request->keyboard_layout_path == NULL || request->keyboard_layout_path[0] == '\0') {
        return flippass_password_gen_generate_utf8_password(request, out, error);
    }

    const char* classes[4] = {0};
    char lower[sizeof(flippass_password_gen_lower)];
    char upper[sizeof(flippass_password_gen_upper)];
    char digits[sizeof(flippass_password_gen_digits)];
    char symbols[sizeof(flippass_password_gen_symbols)];
    char combined[128];
    const size_t class_count = flippass_password_gen_build_layout_classes(
        request,
        classes,
        COUNT_OF(classes),
        lower,
        sizeof(lower),
        upper,
        sizeof(upper),
        digits,
        sizeof(digits),
        symbols,
        sizeof(symbols),
        combined,
        sizeof(combined),
        error);

    return flippass_password_gen_generate_password_from_classes(
        request, classes, class_count, combined, out, error);
}

static bool flippass_password_gen_generate_password(
    const FlipPassPasswordGenPluginRequestV1* request,
    char out[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U],
    FuriString* error) {
    const char* classes[4] = {0};
    char combined[128];

    if(request->charset == FlipPassPasswordGenCharsetUtf8) {
        return flippass_password_gen_generate_utf8_password(request, out, error);
    }
    if(request->charset == FlipPassPasswordGenCharsetLayout) {
        return flippass_password_gen_generate_layout_password(request, out, error);
    }

    const size_t class_count = flippass_password_gen_build_classes(
        request, classes, COUNT_OF(classes), combined, sizeof(combined));
    return flippass_password_gen_generate_password_from_classes(
        request, classes, class_count, combined, out, error);
}

static bool
    flippass_password_gen_finish(FlipPassPasswordGenPluginResultV1* result, FuriString* error) {
    if(result == NULL || !flippass_password_gen_state.active) {
        if(error != NULL) {
            furi_string_set_str(error, "Password generation is not active.");
        }
        return false;
    }

    memset(result, 0, sizeof(*result));
    const bool ok = flippass_password_gen_generate_password(
        &flippass_password_gen_state.request, result->password, error);

    flippass_password_gen_state.active = false;
    flippass_password_gen_reset_state();
    return ok;
}

static void flippass_password_gen_abort(void) {
    flippass_password_gen_state.active = false;
    flippass_password_gen_reset_state();
}

static const FlipPassPasswordGenPluginV1 flippass_password_gen_plugin = {
    .api_version = FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
    .begin = flippass_password_gen_begin,
    .finish = flippass_password_gen_finish,
    .abort = flippass_password_gen_abort,
};

static const FlipperAppPluginDescriptor flippass_password_gen_descriptor = {
    .appid = FLIPPASS_PASSWORD_GEN_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION,
    .entry_point = &flippass_password_gen_plugin,
};

const FlipperAppPluginDescriptor* flippass_password_gen_plugin_ep(void) {
    return &flippass_password_gen_descriptor;
}
