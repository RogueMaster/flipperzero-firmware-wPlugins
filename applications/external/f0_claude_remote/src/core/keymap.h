#pragma once

#include <stdbool.h>

#include "action.h"
#include "mode.h"

typedef enum {
    KeymapKeyUp,
    KeymapKeyDown,
    KeymapKeyLeft,
    KeymapKeyRight,
    KeymapKeyOk,
    KeymapKeyBack,
    KeymapKeyMAX,
} KeymapKey;

typedef enum {
    KeymapEventPress,
    KeymapEventShort,
    KeymapEventLong,
    KeymapEventRepeat,
    KeymapEventRelease,
    KeymapEventMAX,
} KeymapEvent;

typedef struct Keymap Keymap;

Keymap* keymap_alloc(void);

void keymap_free(Keymap* keymap);

Action keymap_handle(Keymap* keymap, KeymapKey key, KeymapEvent event);

Action keymap_set_connected(Keymap* keymap, bool connected);

Mode keymap_mode(const Keymap* keymap);
