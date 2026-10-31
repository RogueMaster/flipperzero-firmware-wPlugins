#include "flippass_cmd.h"

#include <string.h>

static unsigned char cmd_upper(unsigned char ch) {
    return ch >= 'a' && ch <= 'z' ? (unsigned char)(ch - 'a' + 'A') : ch;
}

static bool cmd_equal(const char* text, size_t length, const char* expected) {
    if(strlen(expected) != length) return false;
    for(size_t i = 0U; i < length; ++i) {
        if(cmd_upper((unsigned char)text[i]) != cmd_upper((unsigned char)expected[i]))
            return false;
    }
    return true;
}

static bool cmd_blank(char ch) {
    return ch == ' ' || ch == '\t';
}

/* Whole-token double quotes, with only backslash and quote escapes inside.
 * NULL output validates/counts without allocating. */
static bool
    cmd_token(const char* text, size_t length, size_t* cursor, char* output, size_t* bytes) {
    while(*cursor < length && cmd_blank(text[*cursor]))
        ++*cursor;
    if(*cursor == length) return false;
    const bool quoted = text[*cursor] == '"';
    if(quoted) ++*cursor;
    size_t used = 0U;
    bool closed = !quoted;
    while(*cursor < length) {
        char ch = text[*cursor];
        if(quoted && ch == '"') {
            ++*cursor;
            closed = true;
            break;
        }
        if(!quoted && cmd_blank(ch)) break;
        if(ch == '"' || (unsigned char)ch < 32U || (unsigned char)ch == 127U) return false;
        if(quoted && ch == '\\' && *cursor + 1U < length &&
           (text[*cursor + 1U] == '\\' || text[*cursor + 1U] == '"')) {
            ch = text[++*cursor];
        }
        if(output != NULL) output[used] = ch;
        ++used;
        ++*cursor;
    }
    if(!closed || (*cursor < length && !cmd_blank(text[*cursor]))) return false;
    if(output != NULL) output[used] = '\0';
    *bytes = used;
    return true;
}

typedef struct {
    size_t application_start;
    size_t arguments_start;
    bool self;
    bool literal_arguments;
} FlipPassCmdLayout;

static bool cmd_application(const char* text, size_t length, bool* self) {
    *self = cmd_equal(text, length, "flippas") || cmd_equal(text, length, "flippass");
    if(length == 0U || length > FLIPPASS_CMD_MAX_APPLICATION_BYTES) return false;
    if(text[0] == '/') {
        if(length < 15U || memcmp(text, "/ext/apps/", 10U) != 0 ||
           !cmd_equal(text + length - 4U, 4U, ".fap"))
            return false;
        for(size_t i = 1U; i < length; ++i) {
            if(text[i] == '/' && (text[i - 1U] == '/' ||
                                  (text[i - 1U] == '.' &&
                                   (i == 1U || text[i - 2U] == '/' ||
                                    (text[i - 2U] == '.' && (i == 2U || text[i - 3U] == '/'))))))
                return false;
        }
    }
    for(size_t i = 0U; i < length; ++i) {
        const unsigned char ch = (unsigned char)text[i];
        if(ch < 32U || ch == 127U || ch == '\\' || ch == '"' || ch == '{' || ch == '}' ||
           ch == ';' || ch == '|' || ch == '&' || ch == '<' || ch == '>' || ch == '`' ||
           (text[0] != '/' && ch == '/'))
            return false;
    }
    return true;
}

static bool cmd_layout(const FlipPassCmd* command, FlipPassCmdLayout* layout) {
    size_t cursor = 0U, bytes = 0U;
    bool cli = false;
    while(cursor < command->length && cmd_blank(command->text[cursor]))
        ++cursor;
    size_t start = cursor;
    if(!cmd_token(command->text, command->length, &cursor, NULL, &bytes)) return false;
    if(cmd_equal(command->text + start, cursor - start, "loader")) {
        cli = true;
        while(cursor < command->length && cmd_blank(command->text[cursor]))
            ++cursor;
        start = cursor;
        if(!cmd_token(command->text, command->length, &cursor, NULL, &bytes) ||
           !cmd_equal(command->text + start, cursor - start, "open"))
            return false;
        while(cursor < command->length && cmd_blank(command->text[cursor]))
            ++cursor;
        start = cursor;
        if(!cmd_token(command->text, command->length, &cursor, NULL, &bytes)) return false;
    }
    layout->application_start = start;
    /* Check the decoded application without a 1 KiB automatic array. Quoted
     * names cannot contain escapes: quote and backslash are invalid app bytes. */
    const bool quoted = command->text[start] == '"';
    const char* application = command->text + start + (quoted ? 1U : 0U);
    if(bytes != cursor - start - (quoted ? 2U : 0U) ||
       !cmd_application(application, bytes, &layout->self))
        return false;
    /* Direct FAP calls carry the application's literal argument string.
     * Named built-ins and explicit loader-open syntax retain CLI normalization. */
    layout->literal_arguments = layout->self || (!cli && application[0] == '/');
    while(cursor < command->length && cmd_blank(command->text[cursor]))
        ++cursor;
    layout->arguments_start = cursor;
    while(cursor < command->length) {
        if(!cmd_token(command->text, command->length, &cursor, NULL, &bytes)) return false;
        while(cursor < command->length && cmd_blank(command->text[cursor]))
            ++cursor;
    }
    return true;
}

FlipPassCmdParseResult flippass_cmd_parse(const char* sequence, FlipPassCmd* command) {
    if(sequence == NULL || sequence[0] != '{') return FlipPassCmdNotCommand;
    size_t cursor = 1U;
    while(cmd_blank(sequence[cursor]))
        ++cursor;
    for(size_t i = 0U; i < 3U; ++i) {
        if(cmd_upper((unsigned char)sequence[cursor]) != (unsigned char)"CMD"[i])
            return FlipPassCmdNotCommand;
        ++cursor;
    }
    if(sequence[cursor] != ':') {
        return sequence[cursor] == '\0' || sequence[cursor] == '}' || cmd_blank(sequence[cursor]) ?
                   FlipPassCmdInvalid :
                   FlipPassCmdNotCommand;
    }
    if(cursor + 3U >= FLIPPASS_CMD_MAX_WRAPPER_BYTES || command == NULL) return FlipPassCmdInvalid;
    const unsigned char separator = (unsigned char)sequence[++cursor];
    if(separator < 33U || separator > 126U || separator == '{' || separator == '}')
        return FlipPassCmdInvalid;
    const size_t body = ++cursor;
    while(cursor < FLIPPASS_CMD_MAX_WRAPPER_BYTES && sequence[cursor] != '\0' &&
          (unsigned char)sequence[cursor] != separator) {
        const unsigned char ch = (unsigned char)sequence[cursor++];
        if(ch < 32U || ch == 127U || ch == '{' || ch == '}') return FlipPassCmdInvalid;
    }
    if(cursor >= FLIPPASS_CMD_MAX_WRAPPER_BYTES || (unsigned char)sequence[cursor] != separator)
        return FlipPassCmdInvalid;
    const size_t body_length = cursor - body;
    const size_t options = ++cursor;
    while(cursor < FLIPPASS_CMD_MAX_WRAPPER_BYTES && sequence[cursor] != '\0' &&
          (unsigned char)sequence[cursor] != separator && sequence[cursor] != '{' &&
          sequence[cursor] != '}')
        ++cursor;
    if(cursor + 2U > FLIPPASS_CMD_MAX_WRAPPER_BYTES ||
       (unsigned char)sequence[cursor] != separator || sequence[cursor + 1U] != '}')
        return FlipPassCmdInvalid;
    const size_t options_length = cursor - options;
    if(options_length != 0U && !cmd_equal(sequence + options, options_length, "O=0") &&
       !cmd_equal(sequence + options, options_length, "W=0") &&
       !cmd_equal(sequence + options, options_length, "O=0,W=0") &&
       !cmd_equal(sequence + options, options_length, "W=0,O=0"))
        return FlipPassCmdInvalid;
    *command = (FlipPassCmd){sequence + body, body_length, cursor + 2U};
    FlipPassCmdLayout layout;
    return cmd_layout(command, &layout) ? FlipPassCmdValid : FlipPassCmdInvalid;
}

bool flippass_cmd_decode(
    const FlipPassCmd* command,
    char* scratch,
    size_t capacity,
    FlipPassCmdLaunch* launch) {
    if(command == NULL || scratch == NULL || launch == NULL ||
       command->length > FLIPPASS_CMD_MAX_WRAPPER_BYTES ||
       capacity < command->length + FLIPPASS_CMD_SCRATCH_EXTRA)
        return false;
    FlipPassCmdLayout layout;
    if(!cmd_layout(command, &layout)) return false;
    size_t cursor = layout.application_start, bytes = 0U;
    if(!cmd_token(command->text, command->length, &cursor, scratch, &bytes)) return false;
    char* arguments = scratch + bytes + 1U;
    const char* tail = command->text + layout.arguments_start;
    size_t end = command->length;
    while(end > layout.arguments_start && cmd_blank(command->text[end - 1U]))
        --end;
    const size_t tail_bytes = end - layout.arguments_start;
    if(!layout.literal_arguments && tail_bytes != 0U && tail[0] == '"') {
        /* Match args_read_probably_quoted_string_and_trim exactly: its first
         * closing quote is literal (no backslash escapes), and the trimmed
         * remainder is appended with one separator. Direct FAP calls preserve
         * quotes because they already specify the native argument string. */
        cursor = layout.arguments_start + 1U;
        while(cursor < end && command->text[cursor] != '"')
            ++cursor;
        if(cursor == end) return false;
        bytes = cursor - layout.arguments_start - 1U;
        memcpy(arguments, tail + 1U, bytes);
        ++cursor;
        while(cursor < end && cmd_blank(command->text[cursor]))
            ++cursor;
        if(cursor < end) {
            arguments[bytes++] = ' ';
            memcpy(arguments + bytes, command->text + cursor, end - cursor);
            bytes += end - cursor;
        }
        arguments[bytes] = '\0';
    } else {
        memcpy(arguments, tail, tail_bytes);
        arguments[tail_bytes] = '\0';
    }
    *launch = (FlipPassCmdLaunch){scratch, arguments, layout.self};
    return true;
}

bool flippass_cmd_preflight(const char* sequence) {
    if(sequence == NULL) return false;
    unsigned commands = 0U;
    bool otp = false;
    for(size_t i = 0U; sequence[i] != '\0';) {
        if(sequence[i] != '{') {
            ++i;
            continue;
        }
        if((sequence[i + 1U] == '{' || sequence[i + 1U] == '}') && sequence[i + 2U] == '}') {
            i += 3U;
            continue;
        }
        FlipPassCmd command;
        const FlipPassCmdParseResult result = flippass_cmd_parse(sequence + i, &command);
        if(result == FlipPassCmdInvalid || (result == FlipPassCmdValid && ++commands > 1U))
            return false;
        if(result == FlipPassCmdValid) {
            i += command.consumed;
            continue;
        }
        size_t start = i + 1U;
        while(cmd_blank(sequence[start]))
            ++start;
        size_t end = start;
        while(sequence[end] != '\0' && sequence[end] != '}' && !cmd_blank(sequence[end]))
            ++end;
        if(cmd_equal(sequence + start, end - start, "HMACOTP") ||
           cmd_equal(sequence + start, end - start, "TIMEOTP"))
            otp = true;
        ++i;
    }
    return commands == 0U || !otp;
}
