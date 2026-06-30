#include "py/repl.h"
#include "py/mpprint.h"

#include "mp_flipper_repl.h"

inline bool mp_flipper_repl_continue_with_input(const char* input) {
    return mp_repl_continue_with_input(input);
}

inline size_t mp_flipper_repl_autocomplete(const char* str, size_t len, const mp_print_t* print, char** compl_str) {
    return mp_repl_autocomplete(str, len, print, compl_str);
}
