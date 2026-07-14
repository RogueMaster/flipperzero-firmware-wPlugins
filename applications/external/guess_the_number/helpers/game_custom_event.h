#pragma once

#include <stddef.h>
#include <stdint.h>
typedef enum {
    GameCustomEventStartscreenUp,
    GameCustomEventStartscreenDown,
    GameCustomEventStartscreenLeft,
    GameCustomEventStartscreenRight,
    GameCustomEventStartscreenOk,
    GameCustomEventStartscreenBack,
    GameCustomEventInstructionsUp,
    GameCustomEventInstructionsDown,
    GameCustomEventInstructionsLeft,
    GameCustomEventInstructionsRight,
    GameCustomEventInstructionsOk,
    GameCustomEventInstructionsBack,
    GameCustomEventPlayUp,
    GameCustomEventPlayDown,
    GameCustomEventPlayLeft,
    GameCustomEventPlayRight,
    GameCustomEventPlayOk,
    GameCustomEventPlayBack,
} GameCustomEvent;

enum GameCustomEventType {
    GameCustomEventMenuVoid,
    GameCustomEventMenuSelected,
};

#pragma pack(push, 1)
typedef union {
    uint32_t packed_value;
    struct {
        uint16_t type;
        int16_t value;
    } content;
} GameCustomEventMenu;
#pragma pack(pop)

static inline uint32_t game_custom_menu_event_pack(uint16_t type, int16_t value) {
    GameCustomEventMenu event = {.content = {.type = type, .value = value}};
    return event.packed_value;
}
static inline void
    game_custom_menu_event_unpack(uint32_t packed_value, uint16_t* type, int16_t* value) {
    GameCustomEventMenu event = {.packed_value = packed_value};
    if(type) *type = event.content.type;
    if(value) *value = event.content.value;
}

static inline uint16_t game_custom_menu_event_get_type(uint32_t packed_value) {
    uint16_t type;
    game_custom_menu_event_unpack(packed_value, &type, NULL);
    return type;
}

static inline int16_t game_custom_menu_event_get_value(uint32_t packed_value) {
    int16_t value;
    game_custom_menu_event_unpack(packed_value, NULL, &value);
    return value;
}
