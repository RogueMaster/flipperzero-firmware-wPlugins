#include "droid_beacons.h"

#include <stddef.h>

const char* const droid_affiliation_names[] = {
    [DroidAffiliationScoundrel] = "Scoundrel",
    [DroidAffiliationResistance] = "Resistance",
    [DroidAffiliationFirstOrder] = "First Order",
};

// Affiliation ID used to compute the yy byte: 0x80 + (aff_id * 2)
static const uint8_t droid_affiliation_ids[] = {
    [DroidAffiliationScoundrel] = 1,
    [DroidAffiliationResistance] = 5,
    [DroidAffiliationFirstOrder] = 9,
};

const DroidPersonalityInfo droid_personality_info[] = {
    [DroidPersonalityNoneRSeries] = {NULL, "R-Series", DroidAffiliationScoundrel, 0x01},
    [DroidPersonalityNoneBBSeries] = {NULL, "BB-Series", DroidAffiliationScoundrel, 0x02},
    [DroidPersonalityBlue] = {"Blue", NULL, DroidAffiliationResistance, 0x03},
    [DroidPersonalityGray] = {"Gray", NULL, DroidAffiliationScoundrel, 0x04},
    [DroidPersonalityRed] = {"Red", NULL, DroidAffiliationFirstOrder, 0x05},
    [DroidPersonalityOrange] = {"Orange", NULL, DroidAffiliationResistance, 0x06},
    [DroidPersonalityPurple] = {"Purple", NULL, DroidAffiliationScoundrel, 0x07},
    [DroidPersonalityBlack] = {"Black", NULL, DroidAffiliationFirstOrder, 0x08},
    [DroidPersonalityCB23] = {"Red", "CB-23", DroidAffiliationScoundrel, 0x09},
    [DroidPersonalityYellow] = {"Yellow", NULL, DroidAffiliationResistance, 0x0A},
    [DroidPersonalityNoneC1Series] = {NULL, "C1-Series", DroidAffiliationResistance, 0x0B},
    [DroidPersonalityNoneDO] = {NULL, "D-O", DroidAffiliationResistance, 0x0C},
    [DroidPersonalityBlue2] = {"Blue", NULL, DroidAffiliationScoundrel, 0x0D},
    [DroidPersonalityNoneBDUnits] = {NULL, "BD Units", DroidAffiliationResistance, 0x0E},
    [DroidPersonalityNoneALTUnits] = {NULL, "A-LT Units", DroidAffiliationScoundrel, 0x0F},
    [DroidPersonalityDrumKit] = {"White", "Drum Kit", DroidAffiliationScoundrel, 0x10},
    [DroidPersonalityHalloween] = {"Orange", "Halloween", DroidAffiliationResistance, 0x01},
    [DroidPersonalityR2H15] = {"Green", "R2-H15", DroidAffiliationResistance, 0x01},
};

uint8_t droid_beacon_generate(DroidPersonality personality, bool paired, uint8_t* output) {
    const DroidPersonalityInfo* info = &droid_personality_info[personality];
    uint8_t yy = 0x80 + (droid_affiliation_ids[info->affiliation] * 2);
    uint8_t zz = info->chip_id;

    uint8_t pos = 0;
    output[pos++] = 0x09;                  // AD length: 9 bytes follow
    output[pos++] = 0xFF;                  // AD type: manufacturer specific
    output[pos++] = 0x83;                  // Manufacturer ID low byte (0x0183)
    output[pos++] = 0x01;                  // Manufacturer ID high byte
    output[pos++] = 0x03;                  // Droid identifier
    output[pos++] = 0x04;                  // Data length (4 bytes follow)
    output[pos++] = 0x44;                  // 0x40 + number of bytes of data including this byte
    output[pos++] = paired ? 0x81 : 0x80;  // 0x80 unpaired, 0x81 paired
    output[pos++] = yy;
    output[pos++] = zz;

    return pos;
}
