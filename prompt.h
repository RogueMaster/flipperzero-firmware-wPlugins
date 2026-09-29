#pragma once

#include <gui/gui.h>
#include <stdbool.h>
#include <stddef.h>

// Show the on-screen keyboard and block until the user accepts or backs out.
// `buf` is both the starting text and where the result lands.
// Returns true when text was accepted, false when the user cancelled.
bool prompt_text(Gui* gui, const char* header, char* buf, size_t cap, size_t min_len);
