#include "location_beacons.h"

const DroidLocationInfo droid_location_info[] = {
    [DroidLocationMarketplace] = {"Market", "Marketplace", 0x01},
    [DroidLocationDroidDepot] = {"Depot", "Droid Depot", 0x02},
    [DroidLocationResistance] = {"Resist.", "Resistance", 0x03},
    [DroidLocationOgas] = {"Cantina", "Oga's Cantina", 0x05},
    [DroidLocationDokOndars] = {"Dok O's", "Dok-Ondar's", 0x06},
    [DroidLocationFirstOrder] = {"1st Order", "First Order", 0x07},
};

const LocIntervalInfo loc_interval_info[] = {
    {"1min", 0x0C}, //
    {"2min", 0x18}, //
    {"3min", 0x24}, //
    {"4min", 0x30}, //
    {"5min", 0x3C}, //
    {"6min", 0x48}, //
    {"7min", 0x54}, //
    {"8min", 0x60}, //
    {"9min", 0x6C}, //
    {"10min", 0x78}, //
};

const LocRssiInfo loc_rssi_info[] = {
    {"Min", "Min\nRSSI: -28 dBm", 0x9C}, //
    {"Close", "Close\nRSSI: -38 dBm", 0xA6}, //
    {"Nearby", "Nearby\nRSSI: -48 dBm", 0xB0}, //
    {"Far", "Far\nRSSI: -58 dBm", 0xBA}, //
    {"Very Far", "Very Far\nRSSI: -68 dBm", 0xC4}, //
    {"Max", "Max\nRSSI: -78 dBm", 0xCE}, //
};

uint8_t droid_location_beacon_generate(
    DroidLocation location,
    uint8_t interval,
    uint8_t rssi,
    uint8_t* output) {
    const DroidLocationInfo* info = &droid_location_info[location];

    uint8_t pos = 0;
    output[pos++] = 0x09; // AD length: 9 bytes follow
    output[pos++] = 0xFF; // AD type: manufacturer specific
    output[pos++] = 0x83; // Manufacturer ID low byte (0x0183)
    output[pos++] = 0x01; // Manufacturer ID high byte
    output[pos++] = 0x0A; // Location beacon type
    output[pos++] = 0x04; // Data length (4 bytes follow)
    output[pos++] = info->location_id;
    output[pos++] = interval;
    output[pos++] = rssi;
    output[pos++] = 0x01; // Uncertain field; 0 or 1 accepted

    return pos;
}
