#pragma once

#include "../rfs_types.h"
#include <lib/subghz/devices/devices.h>

/*
 * Firmware compatibility adapter (Specification Section 5.3).
 * Everything that differs between firmware families is isolated here so the rest of the
 * application stays firmware-neutral.
 */

typedef enum {
    RfsFirmwareOfficial,
    RfsFirmwareMomentum,
    RfsFirmwareUnleashed,
    RfsFirmwareRogueMaster,
    RfsFirmwareOther,
} RfsFirmwareFamily;

/** The firmware's transmit decision for a frequency (firmware-neutral form). */
typedef enum {
    RfsTxPermAllowed,
    RfsTxPermBlockedRegionNotProvisioned,
    RfsTxPermBlockedRegion,
    RfsTxPermBlockedDefaultRange,
    RfsTxPermUnsupported,
} RfsTxPermission;

typedef struct {
    RfsFirmwareFamily family;
    char origin[24];
    char version[32];
    char branch[32];
    char githash[16];
    char build_date[16];
    uint8_t target;
    bool has_check_tx; /* firmware reports why a transmission is blocked */
} RfsFirmwareInfo;

void rfs_firmware_detect(RfsFirmwareInfo* info);
const char* rfs_firmware_family_name(RfsFirmwareFamily family);

/** Ask the active firmware whether the device may transmit on this frequency.
 *  Never overrides the firmware: the result only reports its decision. */
RfsTxPermission rfs_firmware_check_tx(const SubGhzDevice* device, uint32_t frequency);

const char* rfs_tx_permission_text(RfsTxPermission permission);
