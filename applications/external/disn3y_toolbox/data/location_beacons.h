#pragma once

#include <stdint.h>

typedef enum {
    DroidLocationMarketplace,
    DroidLocationDroidDepot,
    DroidLocationResistance,
    DroidLocationOgas,
    DroidLocationDokOndars,
    DroidLocationFirstOrder,
    DroidLocationCount,
} DroidLocation;

typedef struct {
    const char* name;
    const char* long_name;
    uint8_t location_id;
} DroidLocationInfo;

extern const DroidLocationInfo droid_location_info[];

typedef struct {
    const char* name;
    uint8_t value;
} LocIntervalInfo;

typedef struct {
    const char* name;
    const char* long_name;
    uint8_t value;
} LocRssiInfo;

#define LOC_INTERVAL_COUNT 10
#define LOC_RSSI_COUNT     6

extern const LocIntervalInfo loc_interval_info[];
extern const LocRssiInfo loc_rssi_info[];

// Build a droid location BLE advertisement payload.
// interval: reaction interval value (value * 5 = seconds; min effective is 0x0C
// = 1 min) rssi: expected RSSI threshold byte (e.g. 0xA6 = -90dBm, 0xBA =
// -70dBm) output must have room for at least 10 bytes. Returns the number of
// bytes written.
uint8_t droid_location_beacon_generate(
    DroidLocation location,
    uint8_t interval,
    uint8_t rssi,
    uint8_t* output);
