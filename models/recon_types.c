#include "recon_types.h"

static const char* const asset_type_names[AssetTypeCount] = {
    [AssetTypeDoor] = "Door",
    [AssetTypeReader] = "RFID Reader",
    [AssetTypeCamera] = "Camera",
    [AssetTypeBle] = "BLE Device",
    [AssetTypeRf] = "RF Device",
    [AssetTypeUnknown] = "Unknown",
};

static const char* const evidence_type_names[EvidenceTypeCount] = {
    [EvidenceNote] = "Note",
    [EvidenceRf] = "RF Capture",
    [EvidenceNfc] = "NFC File",
    [EvidenceBle] = "BLE Observation",
};

static const char* const relation_type_names[RelTypeCount] = {
    [RelReadsBadge] = "reads badge",
    [RelControls] = "controls",
    [RelObserves] = "observes",
    [RelConnectsTo] = "connects to",
    [RelNearby] = "nearby",
};

const char* asset_type_name(AssetType type) {
    if(type >= AssetTypeCount) return "Unknown";
    return asset_type_names[type];
}

const char* evidence_type_name(EvidenceType type) {
    if(type >= EvidenceTypeCount) return "Note";
    return evidence_type_names[type];
}

const char* relation_type_name(RelationType type) {
    if(type >= RelTypeCount) return "linked";
    return relation_type_names[type];
}
