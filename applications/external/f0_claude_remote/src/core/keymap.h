#pragma once

#include <stdbool.h>
#include <stdint.h>

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

Action keymap_time_out(Keymap* keymap);

uint32_t keymap_wait_ms(const Keymap* keymap);

Mode keymap_mode(const Keymap* keymap);
