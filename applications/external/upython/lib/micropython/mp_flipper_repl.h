#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "py/mpprint.h"

#include "mp_flipper_runtime.h"

bool mp_flipper_repl_continue_with_input(const char* input);

size_t mp_flipper_repl_autocomplete(const char* str, size_t len, const mp_print_t* print, char** compl_str);
