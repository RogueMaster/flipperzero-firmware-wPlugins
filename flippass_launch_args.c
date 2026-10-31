#include "flippass_launch_args.h"

#include <string.h>

static void flippass_launch_scrub(char* buffer, size_t size) {
    volatile unsigned char* bytes = (volatile unsigned char*)buffer;
    while(size-- > 0U)
        *bytes++ = 0U;
}

void flippass_launch_args_clear(FlipPassLaunchArgs* args, char* scratch, size_t scratch_capacity) {
    if(scratch != NULL) flippass_launch_scrub(scratch, scratch_capacity);
    if(args != NULL) memset(args, 0, sizeof(*args));
}

static bool flippass_launch_ascii_equal(const char* left, const char* right) {
    while(*left != '\0' && *right != '\0') {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b) return false;
    }
    return *left == *right;
}

bool flippass_launch_layout_id_valid(const char* id) {
    if(id == NULL || id[0] == '\0') return true;
    if(strcmp(id, ".") == 0 || strcmp(id, "..") == 0) return false;
    size_t length = 0U;
    for(; id[length] != '\0'; ++length) {
        const unsigned char ch = (unsigned char)id[length];
        if(length >= FLIPPASS_LAUNCH_LAYOUT_MAX_BYTES || ch < 32U || ch >= 127U ||
           strchr("/\\:\"*?<>|", ch) != NULL)
            return false;
    }
    return id[length - 1U] != ' ' && id[length - 1U] != '.';
}

FlipPassLaunchArgsError flippass_launch_args_parse(
    const char* input,
    char* scratch,
    size_t scratch_capacity,
    FlipPassLaunchArgs* out) {
    if(out != NULL) memset(out, 0, sizeof(*out));
    if(input == NULL || scratch == NULL || out == NULL) return FlipPassLaunchArgsInvalidArgument;

    size_t input_length = 0U;
    while(input_length <= FLIPPASS_LAUNCH_ARGS_MAX_BYTES && input[input_length] != '\0') {
        const unsigned char byte = (unsigned char)input[input_length];
        if(byte < 32U || byte == 127U) return FlipPassLaunchArgsControlCharacter;
        ++input_length;
    }
    if(input_length > FLIPPASS_LAUNCH_ARGS_MAX_BYTES) return FlipPassLaunchArgsTooLong;
    if(scratch_capacity <= input_length) return FlipPassLaunchArgsScratchTooSmall;

    memmove(scratch, input, input_length + 1U);
    char* cursor = scratch;
    char* destination = scratch;
    const char* end = scratch + input_length;
    const char* tokens[FLIPPASS_LAUNCH_TOKEN_COUNT];
    size_t lengths[FLIPPASS_LAUNCH_TOKEN_COUNT];
    size_t count = 0U;
    FlipPassLaunchArgsError error = FlipPassLaunchArgsOk;

    while(cursor < end) {
        while(cursor < end && *cursor == ' ')
            ++cursor;
        if(cursor == end) break;
        if(count == FLIPPASS_LAUNCH_TOKEN_COUNT) {
            error = FlipPassLaunchArgsTooManyTokens;
            goto failed;
        }
        tokens[count] = destination;
        if(*cursor == '"') {
            ++cursor;
            bool closed = false;
            while(cursor < end) {
                char byte = *cursor++;
                if(byte == '"') {
                    closed = true;
                    break;
                }
                if(byte == '\\') {
                    if(cursor == end) {
                        error = FlipPassLaunchArgsInvalidEscape;
                        goto failed;
                    }
                    if(*cursor == '"' || *cursor == '\\') byte = *cursor++;
                }
                *destination++ = byte;
            }
            if(!closed || (cursor < end && *cursor != ' ')) {
                error = FlipPassLaunchArgsInvalidQuotes;
                goto failed;
            }
        } else {
            while(cursor < end && *cursor != ' ') {
                if(*cursor == '"') {
                    error = FlipPassLaunchArgsInvalidQuotes;
                    goto failed;
                }
                *destination++ = *cursor++;
            }
        }
        lengths[count] = (size_t)(destination - tokens[count]);
        ++count;
        /* Consume separators before an in-place NUL can overwrite one. */
        while(cursor < end && *cursor == ' ')
            ++cursor;
        *destination++ = '\0';
    }

    if((count > 0U && lengths[0] > FLIPPASS_LAUNCH_PATH_MAX_BYTES) ||
       (count > 2U && lengths[2] > FLIPPASS_LAUNCH_PATH_MAX_BYTES)) {
        error = FlipPassLaunchArgsPathTooLong;
    } else if(count > 0U && tokens[0][0] != '/') {
        error = FlipPassLaunchArgsInvalidDatabasePath;
    } else if(count > 1U && lengths[1] > FLIPPASS_LAUNCH_PASSWORD_MAX_BYTES) {
        error = FlipPassLaunchArgsPasswordTooLong;
    } else if(count > 3U && lengths[3] > FLIPPASS_LAUNCH_FIELD_MAX_BYTES) {
        error = FlipPassLaunchArgsFieldTooLong;
    } else if(count > 3U && lengths[3] > 0U && lengths[2] == 0U) {
        error = FlipPassLaunchArgsFieldWithoutEntry;
    }
    if(error != FlipPassLaunchArgsOk) goto failed;

    FlipPassLaunchTransport transport = FlipPassLaunchTransportAuto;
    if(count > 4U && lengths[4] > 0U) {
        if(flippass_launch_ascii_equal(tokens[4], "USB"))
            transport = FlipPassLaunchTransportUsb;
        else if(flippass_launch_ascii_equal(tokens[4], "BT"))
            transport = FlipPassLaunchTransportBluetooth;
        else if(!flippass_launch_ascii_equal(tokens[4], "AUTO")) {
            error = FlipPassLaunchArgsInvalidTransport;
            goto failed;
        }
    }
    const bool legacy_close = count == 6U && lengths[5] == 1U &&
                              (tokens[5][0] == '0' || tokens[5][0] == '1');
    const char* layout_id = count > 5U && !legacy_close ? tokens[5] : NULL;
    if(layout_id != NULL && lengths[5] > FLIPPASS_LAUNCH_LAYOUT_MAX_BYTES) {
        error = FlipPassLaunchArgsLayoutTooLong;
        goto failed;
    }
    if(!flippass_launch_layout_id_valid(layout_id)) {
        error = FlipPassLaunchArgsInvalidLayout;
        goto failed;
    }
    bool autoclose = false;
    const size_t close_index = legacy_close ? 5U : 6U;
    if(count > close_index && lengths[close_index] > 0U) {
        if(lengths[close_index] != 1U ||
           (tokens[close_index][0] != '0' && tokens[close_index][0] != '1')) {
            error = FlipPassLaunchArgsInvalidAutoclose;
            goto failed;
        }
        autoclose = tokens[close_index][0] == '1';
    }

    out->database_path = count > 0U ? tokens[0] : NULL;
    out->password = count > 1U ? tokens[1] : NULL;
    out->entry_path = count > 2U ? tokens[2] : NULL;
    out->field_tag = count > 3U ? tokens[3] : NULL;
    out->layout_id = layout_id;
    out->transport = transport;
    out->autoclose = autoclose;
    out->token_count = (uint8_t)count;
    out->present_mask = (uint8_t)((1U << count) - 1U);
    return FlipPassLaunchArgsOk;

failed:
    flippass_launch_scrub(scratch, input_length + 1U);
    return error;
}

const char* flippass_launch_args_error_message(FlipPassLaunchArgsError error) {
    switch(error) {
    case FlipPassLaunchArgsOk:
        return "";
    case FlipPassLaunchArgsInvalidArgument:
        return "Invalid launch parser arguments.";
    case FlipPassLaunchArgsTooLong:
        return "Launch arguments exceed the byte limit.";
    case FlipPassLaunchArgsScratchTooSmall:
        return "Launch argument workspace is too small.";
    case FlipPassLaunchArgsTooManyTokens:
        return "Too many launch arguments.";
    case FlipPassLaunchArgsInvalidQuotes:
        return "Invalid launch argument quoting.";
    case FlipPassLaunchArgsInvalidEscape:
        return "Incomplete quoted launch escape.";
    case FlipPassLaunchArgsControlCharacter:
        return "Launch arguments contain a control character.";
    case FlipPassLaunchArgsInvalidDatabasePath:
        return "The database path must be absolute.";
    case FlipPassLaunchArgsPathTooLong:
        return "A launch path exceeds the byte limit.";
    case FlipPassLaunchArgsPasswordTooLong:
        return "The launch password exceeds the byte limit.";
    case FlipPassLaunchArgsFieldTooLong:
        return "The launch field tag exceeds the byte limit.";
    case FlipPassLaunchArgsFieldWithoutEntry:
        return "A field tag requires an entry path.";
    case FlipPassLaunchArgsInvalidTransport:
        return "The launch transport must be USB, BT or AUTO.";
    case FlipPassLaunchArgsLayoutTooLong:
        return "The keyboard layout ID exceeds 63 bytes.";
    case FlipPassLaunchArgsInvalidLayout:
        return "Use a keyboard layout ID, not a path.";
    case FlipPassLaunchArgsInvalidAutoclose:
        return "Launch autoclose must be 0 or 1.";
    default:
        return "Invalid launch arguments.";
    }
}
