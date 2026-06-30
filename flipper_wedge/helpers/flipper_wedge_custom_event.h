#pragma once

typedef enum {
    FlipperWedgeCustomEventStartscreenUp,
    FlipperWedgeCustomEventStartscreenDown,
    FlipperWedgeCustomEventStartscreenLeft,
    FlipperWedgeCustomEventStartscreenRight,
    FlipperWedgeCustomEventStartscreenBack,
    FlipperWedgeCustomEventTestType,

    // Scan events
    FlipperWedgeCustomEventNfcDetected,
    FlipperWedgeCustomEventRfidDetected,
    FlipperWedgeCustomEventScanTimeout,
    FlipperWedgeCustomEventDisplayDone,
    FlipperWedgeCustomEventCooldownDone,

    // Mode change
    FlipperWedgeCustomEventModeChange,

    // Settings
    FlipperWedgeCustomEventOpenSettings,
} FlipperWedgeCustomEvent;

enum FlipperWedgeCustomEventType {
    // Reserve first 100 events for button types and indexes, starting from 0
    FlipperWedgeCustomEventMenuVoid,
    FlipperWedgeCustomEventMenuSelected,
};

#pragma pack(push, 1)
typedef union {
    uint32_t packed_value;
    struct {
        uint16_t type;
        int16_t value;
    } content;
} FlipperWedgeCustomEventMenu;
#pragma pack(pop)

static inline uint32_t flipper_wedge_custom_menu_event_pack(uint16_t type, int16_t value) {
    FlipperWedgeCustomEventMenu event = {.content = {.type = type, .value = value}};
    return event.packed_value;
}
static inline void
    flipper_wedge_custom_menu_event_unpack(uint32_t packed_value, uint16_t* type, int16_t* value) {
    FlipperWedgeCustomEventMenu event = {.packed_value = packed_value};
    if(type) *type = event.content.type;
    if(value) *value = event.content.value;
}

static inline uint16_t flipper_wedge_custom_menu_event_get_type(uint32_t packed_value) {
    uint16_t type;
    flipper_wedge_custom_menu_event_unpack(packed_value, &type, NULL);
    return type;
}

static inline int16_t flipper_wedge_custom_menu_event_get_value(uint32_t packed_value) {
    int16_t value;
    flipper_wedge_custom_menu_event_unpack(packed_value, NULL, &value);
    return value;
}
