#pragma once

#include <stdbool.h>
#include <stddef.h>

void settings_load(char* out, size_t cap);
bool settings_save(const char* dir);
