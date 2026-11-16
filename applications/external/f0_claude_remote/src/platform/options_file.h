#pragma once

#include <stdbool.h>

#include "../core/option.h"

typedef struct OptionsFile OptionsFile;

OptionsFile* options_file_alloc(void);

void options_file_free(OptionsFile* options_file);

bool options_file_read_option(OptionsFile* options_file, Option option, bool fallback);

void options_file_set_option(OptionsFile* options_file, Option option, bool on);
