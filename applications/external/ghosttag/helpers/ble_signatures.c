#include "ble_signatures.h"

const char* tracker_type_name(TrackerType type) {
    switch(type) {
    case TrackerTypeAppleFindMy:
        return "Apple Find My";
    case TrackerTypeAirTagPaired:
        return "Apple (paired)";
    case TrackerTypeTile:
        return "Tile";
    case TrackerTypeSamsungSmartTag:
        return "Samsung SmartTag";
    case TrackerTypeChipolo:
        return "Chipolo";
    case TrackerTypeUnknown:
    default:
        return "Unknown BLE";
    }
}

const char* tracker_type_short(TrackerType type) {
    switch(type) {
    case TrackerTypeAppleFindMy:
        return "AirTag";
    case TrackerTypeAirTagPaired:
        return "Apple";
    case TrackerTypeTile:
        return "Tile";
    case TrackerTypeSamsungSmartTag:
        return "SmartTag";
    case TrackerTypeChipolo:
        return "Chipolo";
    case TrackerTypeUnknown:
    default:
        return "Unknown";
    }
}

bool tracker_type_is_threat(TrackerType type) {
    switch(type) {
    case TrackerTypeAppleFindMy:
    case TrackerTypeTile:
    case TrackerTypeSamsungSmartTag:
    case TrackerTypeChipolo:
        return true;
    // A paired/owner-present Apple device is usually the owner themselves.
    case TrackerTypeAirTagPaired:
    case TrackerTypeUnknown:
    default:
        return false;
    }
}

TrackerType tracker_type_from_code(uint8_t code) {
    if(code >= TrackerTypeCount) return TrackerTypeUnknown;
    return (TrackerType)code;
}
