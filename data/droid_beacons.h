#pragma once

#include <stdbool.h>
#include <stdint.h>

// Droid personality chip affiliations
typedef enum {
    DroidAffiliationScoundrel,
    DroidAffiliationResistance,
    DroidAffiliationFirstOrder,
    DroidAffiliationCount,
} DroidAffiliation;

// Personality chip identifiers
typedef enum {
    DroidPersonalityNoneRSeries,
    DroidPersonalityNoneBBSeries,
    DroidPersonalityBlue,
    DroidPersonalityGray,
    DroidPersonalityRed,
    DroidPersonalityOrange,
    DroidPersonalityPurple,
    DroidPersonalityBlack,
    DroidPersonalityCB23,
    DroidPersonalityYellow,
    DroidPersonalityNoneC1Series,
    DroidPersonalityNoneDO,
    DroidPersonalityBlue2,
    DroidPersonalityNoneBDUnits,
    DroidPersonalityNoneALTUnits,
    DroidPersonalityDrumKit,
    DroidPersonalityHalloween,
    DroidPersonalityR2H15,
    DroidPersonalityCount,
} DroidPersonality;

typedef struct {
    const char* chip_color;  // NULL when no color (e.g. default droid chip)
    const char* droid;       // NULL when no specific droid
    DroidAffiliation affiliation;
    uint8_t chip_id;  // zz value
} DroidPersonalityInfo;

extern const char* const droid_affiliation_names[];
extern const DroidPersonalityInfo droid_personality_info[];

// Build a droid personality BLE advertisement payload.
// Returns the number of bytes written to output.
// output must have room for at least 10 bytes.
uint8_t droid_beacon_generate(DroidPersonality personality, bool paired, uint8_t* output);
