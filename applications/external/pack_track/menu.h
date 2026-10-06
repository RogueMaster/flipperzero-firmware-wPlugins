#pragma once

#include <gui/gui.h>
#include <stddef.h>
#include <stdint.h>

// Show a blocking list picker. Returns the chosen index, or -1 if the user
// backed out.
int32_t menu_pick(Gui* gui, const char* header, const char* const* items, size_t count);
