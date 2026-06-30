#include <stdio.h>

#include <cli/cli.h>
#include <cli/cli_ansi.h>
#include <furi.h>

#include <genhdr/mpversion.h>
#include <mp_flipper_compiler.h>

#include <mp_flipper_repl.h>

#include "upython.h"

#define AUTOCOMPLETE_MANY_MATCHES (size_t)(-1)
#define HISTORY_SIZE              16

typedef enum {
    CliSymbolAsciiSOH = 0x01,
    CliSymbolAsciiETX = 0x03,
    CliSymbolAsciiEOT = 0x04,
    CliSymbolAsciiBell = 0x07,
    CliSymbolAsciiBackspace = 0x08,
    CliSymbolAsciiTab = 0x09,
    CliSymbolAsciiLF = 0x0A,
    CliSymbolAsciiCR = 0x0D,
    CliSymbolAsciiEsc = 0x1B,
    CliSymbolAsciiUS = 0x1F,
    CliSymbolAsciiSpace = 0x20,
    CliSymbolAsciiDel = 0x7F,
} CliSymbols;

typedef struct {
    FuriString** stack;
    size_t pointer;
    size_t size;
} mp_flipper_repl_history_t;

typedef struct {
    mp_flipper_repl_history_t* history;
    FuriString* line;
    FuriString* code;
    size_t cursor;
    bool is_ps2;
} mp_flipper_repl_context_t;

static mp_flipper_repl_history_t* mp_flipper_repl_history_alloc() {
    mp_flipper_repl_history_t* history = malloc(sizeof(mp_flipper_repl_history_t));

    history->stack = malloc(HISTORY_SIZE * sizeof(FuriString*));
    history->pointer = 0;
    history->size = 1;

    for(size_t i = 0; i < HISTORY_SIZE; i++) {
        history->stack[i] = furi_string_alloc();
    }

    return history;
}

static void mp_flipper_repl_history_free(mp_flipper_repl_history_t* history) {
    for(size_t i = 0; i < HISTORY_SIZE; i++) {
        furi_string_free(history->stack[i]);
    }

    free(history);
}

static mp_flipper_repl_context_t* mp_flipper_repl_context_alloc() {
    mp_flipper_repl_context_t* context = malloc(sizeof(mp_flipper_repl_context_t));

    context->history = mp_flipper_repl_history_alloc();
    context->code = furi_string_alloc();
    context->line = furi_string_alloc();
    context->cursor = 0;
    context->is_ps2 = false;

    return context;
}

static void mp_flipper_repl_context_free(mp_flipper_repl_context_t* context) {
    mp_flipper_repl_history_free(context->history);

    furi_string_free(context->code);
    furi_string_free(context->line);

    free(context);
}

static void print_full_psx(mp_flipper_repl_context_t* context) {
    const char* psx = context->is_ps2 ? "... " : ">>> ";

    printf("\e[2K\r%s%s", psx, furi_string_get_cstr(context->line));

    fflush(stdout);

    for(size_t i = context->cursor; i < furi_string_size(context->line); i++) {
        printf("\e[D");
    }

    fflush(stdout);
}

inline static void handle_arrow_keys(char character, mp_flipper_repl_context_t* context) {
    mp_flipper_repl_history_t* history = context->history;

    do {
        bool update_by_history = false;
        // up arrow
        if(character == 'A' && history->pointer == 0) {
            furi_string_set(history->stack[0], context->line);
        }

        if(character == 'A' && history->pointer < history->size) {
            history->pointer += (history->pointer + 1) == history->size ? 0 : 1;

            update_by_history = true;
        }

        // down arrow
        if(character == 'B' && history->pointer > 0) {
            history->pointer--;

            update_by_history = true;
        }

        if(update_by_history) {
            furi_string_set(context->line, history->stack[history->pointer]);

            context->cursor = furi_string_size(context->line);

            break;
        }

        // right arrow
        if(character == 'C' && context->cursor != furi_string_size(context->line)) {
            context->cursor++;

            break;
        }

        // left arrow
        if(character == 'D' && context->cursor > 0) {
            context->cursor--;

            break;
        }
    } while(false);

    print_full_psx(context);
}

inline static void handle_backspace(mp_flipper_repl_context_t* context) {
    // skip backspace at begin of line
    if(context->cursor == 0) {
        return;
    }

    const char* line = furi_string_get_cstr(context->line);
    size_t before = context->cursor - 1;
    size_t after = furi_string_size(context->line) - context->cursor;

    furi_string_printf(context->line, "%.*s%.*s", before, line, after, line + context->cursor);

    context->cursor--;

    printf("\e[D\e[1P");

    fflush(stdout);
}

inline static bool is_indent_required(mp_flipper_repl_context_t* context) {
    for(size_t i = 0; context->is_ps2 && i < context->cursor; i++) {
        if(furi_string_get_char(context->line, i) != ' ') {
            return false;
        }
    }

    return context->is_ps2;
}

inline static void handle_autocomplete(mp_flipper_repl_context_t* context) {
    // check if ps2 is active and just a tab character is required
    if(is_indent_required(context)) {
        furi_string_replace_at(context->line, context->cursor, 0, "    ");
        context->cursor += 4;

        print_full_psx(context);

        return;
    }

    const char* new_line = furi_string_get_cstr(context->line);
    FuriString* orig_line = furi_string_alloc_printf("%s", new_line);
    const char* orig_line_str = furi_string_get_cstr(orig_line);

    char* completion = malloc(128 * sizeof(char));

    mp_print_t* print = malloc(sizeof(mp_print_t));

    print->data = mp_flipper_print_data_alloc();
    print->print_strn = mp_flipper_print_strn;

    size_t length = mp_flipper_repl_autocomplete(new_line, context->cursor, print, &completion);

    do {
        if(length == 0) {
            break;
        }

        if(length != AUTOCOMPLETE_MANY_MATCHES) {
            furi_string_printf(
                context->line,
                "%.*s%.*s%s",
                context->cursor,
                orig_line_str,
                length,
                completion,
                orig_line_str + context->cursor);

            context->cursor += length;
        } else {
            printf("%s", mp_flipper_print_get_data(print->data));
        }

        print_full_psx(context);
    } while(false);

    mp_flipper_print_data_free(print->data);
    furi_string_free(orig_line);
    free(completion);
    free(print);
}

inline static void update_history(mp_flipper_repl_context_t* context) {
    mp_flipper_repl_history_t* history = context->history;

    if(!furi_string_empty(context->line) && !furi_string_equal(context->line, history->stack[1])) {
        history->size += history->size == HISTORY_SIZE ? 0 : 1;

        for(size_t i = history->size - 1; i > 1; i--) {
            furi_string_set(history->stack[i], history->stack[i - 1]);
        }

        furi_string_set(history->stack[1], context->line);
    }

    furi_string_reset(history->stack[0]);

    history->pointer = 0;
}

inline static bool continue_with_input(mp_flipper_repl_context_t* context) {
    if(furi_string_empty(context->line)) {
        return false;
    }

    if(!mp_flipper_repl_continue_with_input(furi_string_get_cstr(context->code))) {
        return false;
    }

    return true;
}

void upython_repl_execute() {
    size_t stack;

    const size_t heap_size = memmgr_get_free_heap() * 0.1;
    const size_t stack_size = 2 * 1024;
    uint8_t* heap = malloc(heap_size * sizeof(uint8_t));

    printf("MicroPython (%s, %s) on Flipper Zero\r\n", MICROPY_GIT_TAG, MICROPY_BUILD_DATE);
    printf("Quit: Ctrl+D | Heap: %zu bytes | Stack: %zu bytes\r\n", heap_size, stack_size);
    printf("      To do a reboot, press Left+Back for 5 seconds.\r\n");
    printf("Docs: https://ofabel.github.io/mp-flipper\r\n");

    mp_flipper_repl_context_t* context = mp_flipper_repl_context_alloc();

    mp_flipper_set_root_module_path("/ext");
    mp_flipper_init(heap, heap_size, stack_size, &stack);

    char character = '\0';

    uint8_t* buffer = malloc(sizeof(uint8_t));

    bool exit = false;

    // REPL loop
    do {
        furi_string_reset(context->code);

        context->is_ps2 = false;

        // scan line loop
        do {
            furi_string_reset(context->line);

            context->cursor = 0;

            print_full_psx(context);

            // scan character loop
            do {
                character = getc(stdin);

                // Ctrl + C
                if(character == CliKeyETX) {
                    context->cursor = 0;

                    furi_string_reset(context->line);
                    furi_string_reset(context->code);

                    printf("\r\nKeyboardInterrupt\r\n");

                    break;
                }

                // Ctrl + D
                if(character == CliKeyEOT) {
                    exit = true;

                    break;
                }

                // skip line feed
                if(character == CliKeyLF) {
                    continue;
                }

                // handle carriage return
                if(character == CliKeyCR) {
                    furi_string_push_back(context->code, '\n');
                    furi_string_cat(context->code, context->line);
                    furi_string_trim(context->code);

                    printf("\r\n");

                    break;
                }

                // handle arrow keys
                if(character >= 0x18 && character <= 0x1B) {
                    character = getc(stdin);
                    character = getc(stdin);

                    handle_arrow_keys(character, context);

                    continue;
                }

                // handle tab, do autocompletion
                if(character == CliKeyTab) {
                    handle_autocomplete(context);

                    continue;
                }

                // handle backspace
                if(character == CliKeyBackspace || character == CliKeyDEL) {
                    handle_backspace(context);

                    continue;
                }

                // append at end
                if(context->cursor == furi_string_size(context->line)) {
                    buffer[0] = character;
                    putc(buffer[0], stdout);
                    fflush(stdout);

                    furi_string_push_back(context->line, character);

                    context->cursor++;

                    continue;
                }

                // insert between
                if(context->cursor < furi_string_size(context->line)) {
                    const char temp[2] = {character, 0};
                    furi_string_replace_at(context->line, context->cursor++, 0, temp);

                    printf("\e[4h%c\e[4l", character);
                    fflush(stdout);

                    continue;
                }
            } while(true);

            // Ctrl + D
            if(exit) {
                break;
            }

            update_history(context);
        } while((context->is_ps2 = continue_with_input(context)));

        // Ctrl + D
        if(exit) {
            break;
        }

        mp_flipper_exec_str(furi_string_get_cstr(context->code));
    } while(true);

    mp_flipper_deinit();

    mp_flipper_repl_context_free(context);

    free(heap);
    free(buffer);
}
