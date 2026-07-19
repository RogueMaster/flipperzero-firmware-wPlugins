#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Tracker taxonomy + BLE advertisement signatures.
 *
 * The Flipper's stock BLE stack is peripheral/advertising only and cannot scan,
 * so the raw advertisement parsing happens on the ESP32 companion (see
 * esp32/ghosttag_esp32). The ESP32 classifies each advert and streams a single
 * digit TrackerType code to the Flipper over UART. The signatures below are the
 * contract both sides agree on and are documented here for reference.
 */

typedef enum {
    TrackerTypeUnknown = 0,
    TrackerTypeAppleFindMy = 1, // AirTag / Find My accessory in "separated" mode
    TrackerTypeAirTagPaired = 2, // Apple device broadcasting nearby/owner-connected
    TrackerTypeTile = 3,
    TrackerTypeSamsungSmartTag = 4,
    TrackerTypeChipolo = 5,
    TrackerTypeCount = 6,
} TrackerType;

/* ---- Advertisement signatures (parsing lives on the ESP32) ----
 * Apple Find My    : Manufacturer-Specific Data, Company ID 0x004C, payload type 0x12.
 * Apple nearby     : Company ID 0x004C, payload type 0x07/0x10 (general Apple kit).
 * Tile             : 16-bit Service UUID / Service Data 0xFEED.
 * Samsung SmartTag : Service Data UUID 0xFD5A (SmartThings Find) or Company ID 0x0075.
 * Chipolo          : ONE Spot rides Apple Find My; classic uses Company ID 0x0157.
 */
#define GHOSTTAG_APPLE_COMPANY_ID 0x004C
#define GHOSTTAG_APPLE_TYPE_FINDMY 0x12
#define GHOSTTAG_APPLE_TYPE_NEARBY 0x07
#define GHOSTTAG_TILE_UUID 0xFEED
#define GHOSTTAG_SAMSUNG_UUID 0xFD5A
#define GHOSTTAG_SAMSUNG_COMPANY_ID 0x0075
#define GHOSTTAG_CHIPOLO_COMPANY_ID 0x0157

/** Long human label, e.g. "Apple Find My". */
const char* tracker_type_name(TrackerType type);

/** Short list label, e.g. "AirTag". */
const char* tracker_type_short(TrackerType type);

/** True for tracker types that can realistically be used to stalk a person. */
bool tracker_type_is_threat(TrackerType type);

/** Validate/clamp a wire code into a TrackerType. */
TrackerType tracker_type_from_code(uint8_t code);
