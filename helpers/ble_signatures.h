#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Tracker taxonomy + BLE advertisement signatures.
 *
 * The Flipper's stock BLE stack is peripheral/advertising only and cannot scan
 * - furi_hal_bt exposes advertising and RF test mode, but no GAP observer role
 * and no way to decode an advertisement - so the raw parsing happens on a
 * BLE-capable ESP32 companion (see esp32/ghosttag_esp32). The ESP32 classifies each advert and streams a single
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
 *
 * Apple Find My    : Manufacturer data, company 0x004C, payload type 0x12.
 *                    The LENGTH byte after the type separates the two states:
 *                    0x19 or longer is "separated" - the tag has lost its
 *                    owner and is broadcasting a full rotating key for any
 *                    passing phone to relay. That is the state a tag planted
 *                    on somebody is in, and the only one graded a threat.
 *                    The short form means the owner is right there.
 * Apple nearby     : company 0x004C, payload type 0x07 (proximity pairing,
 *                    e.g. AirPods) or 0x10 (nearby info, e.g. a phone).
 * Tile             : 16-bit service UUID / service data 0xFEED.
 * Samsung SmartTag : service data UUID 0xFD5A (SmartThings Find) ONLY.
 *                    Company ID 0x0075 is Samsung Electronics and is on every
 *                    Samsung phone, watch, TV and pair of earbuds in the room,
 *                    so matching it reported half a train carriage as
 *                    SmartTags. It is deliberately not used.
 * Chipolo          : ONE Spot rides Apple Find My; classic uses company 0x0157.
 */
#define GHOSTTAG_APPLE_COMPANY_ID 0x004C
#define GHOSTTAG_APPLE_TYPE_FINDMY 0x12
#define GHOSTTAG_APPLE_TYPE_PAIRING 0x07
#define GHOSTTAG_APPLE_TYPE_NEARBY 0x10
#define GHOSTTAG_APPLE_FINDMY_SEPARATED_LEN 0x19
#define GHOSTTAG_TILE_UUID 0xFEED
#define GHOSTTAG_SAMSUNG_UUID 0xFD5A
#define GHOSTTAG_CHIPOLO_COMPANY_ID 0x0157

/** Long human label, e.g. "Apple Find My". */
const char* tracker_type_name(TrackerType type);

/** Short list label, e.g. "AirTag". */
const char* tracker_type_short(TrackerType type);

/** True for tracker types that can realistically be used to stalk a person. */
bool tracker_type_is_threat(TrackerType type);

/** Validate/clamp a wire code into a TrackerType. */
TrackerType tracker_type_from_code(uint8_t code);
