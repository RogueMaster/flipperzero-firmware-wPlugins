#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "action.h"
#include "link.h"
#include "mode.h"
#include "option.h"

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

typedef enum {
    KeymapWaitKeep,
    KeymapWaitStart,
    KeymapWaitStop,
} KeymapWait;

typedef struct Keymap Keymap;

Keymap* keymap_alloc(void);

void keymap_free(Keymap* keymap);

Action keymap_handle(Keymap* keymap, KeymapKey key, KeymapEvent event);

Action keymap_set_connected(Keymap* keymap, bool connected);

Action keymap_time_out(Keymap* keymap);

void keymap_set_option(Keymap* keymap, Option option, bool on);

KeymapWait keymap_wait(const Keymap* keymap);

uint32_t keymap_wait_ms(const Keymap* keymap);

Mode keymap_mode(const Keymap* keymap);

Link keymap_link(const Keymap* keymap);

bool keymap_legend(const Keymap* keymap);

bool keymap_option(const Keymap* keymap, Option option);

Option keymap_cursor(const Keymap* keymap);
