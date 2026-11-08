#include "keymap_input.h"

static const KeymapKey keymap_input_keys[InputKeyMAX] = {
    [InputKeyUp] = KeymapKeyUp,
    [InputKeyDown] = KeymapKeyDown,
    [InputKeyRight] = KeymapKeyRight,
    [InputKeyLeft] = KeymapKeyLeft,
    [InputKeyOk] = KeymapKeyOk,
    [InputKeyBack] = KeymapKeyBack,
};

static const KeymapEvent keymap_input_events[InputTypeMAX] = {
    [InputTypePress] = KeymapEventPress,
    [InputTypeRelease] = KeymapEventRelease,
    [InputTypeShort] = KeymapEventShort,
    [InputTypeLong] = KeymapEventLong,
    [InputTypeRepeat] = KeymapEventRepeat,
};

Action keymap_input_handle(Keymap* keymap, const InputEvent* event) {
    return keymap_handle(keymap, keymap_input_keys[event->key], keymap_input_events[event->type]);
}
