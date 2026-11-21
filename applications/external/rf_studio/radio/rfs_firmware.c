#include "rfs_firmware.h"

#include <toolbox/version.h>

/* Firmware that explains transmit refusals (Momentum) ships this header. Detecting it at compile
 * time lets one source tree build against both official and Momentum SDKs. */
#if defined(__has_include)
#if __has_include(<lib/subghz/devices/tx.h>)
#include <lib/subghz/devices/tx.h>
#define RFS_FW_HAS_CHECK_TX
#endif
#endif

static RfsFirmwareFamily rfs_firmware_family_from_origin(const char* origin) {
    if(!origin) return RfsFirmwareOther;
    if(strstr(origin, "Momentum")) return RfsFirmwareMomentum;
    if(strstr(origin, "Unleashed")) return RfsFirmwareUnleashed;
    if(strstr(origin, "RogueMaster")) return RfsFirmwareRogueMaster;
    if(strstr(origin, "Official")) return RfsFirmwareOfficial;
    return RfsFirmwareOther;
}

static void rfs_copy(char* dst, size_t size, const char* src) {
    strlcpy(dst, src ? src : "?", size);
}

void rfs_firmware_detect(RfsFirmwareInfo* info) {
    memset(info, 0, sizeof(RfsFirmwareInfo));
    const Version* version = version_get();
    if(version) {
        rfs_copy(info->origin, sizeof(info->origin), version_get_firmware_origin(version));
        rfs_copy(info->version, sizeof(info->version), version_get_version(version));
        rfs_copy(info->branch, sizeof(info->branch), version_get_gitbranch(version));
        rfs_copy(info->githash, sizeof(info->githash), version_get_githash(version));
        rfs_copy(info->build_date, sizeof(info->build_date), version_get_builddate(version));
        info->target = version_get_target(version);
    } else {
        rfs_copy(info->origin, sizeof(info->origin), "Unknown");
    }
    info->family = rfs_firmware_family_from_origin(info->origin);
#ifdef RFS_FW_HAS_CHECK_TX
    info->has_check_tx = true;
#endif
}

const char* rfs_firmware_family_name(RfsFirmwareFamily family) {
    switch(family) {
    case RfsFirmwareOfficial:
        return "Official";
    case RfsFirmwareMomentum:
        return "Momentum";
    case RfsFirmwareUnleashed:
        return "Unleashed";
    case RfsFirmwareRogueMaster:
        return "RogueMaster";
    default:
        return "Other";
    }
}

RfsTxPermission rfs_firmware_check_tx(const SubGhzDevice* device, uint32_t frequency) {
    if(!device) return RfsTxPermUnsupported;
#ifdef RFS_FW_HAS_CHECK_TX
    switch(subghz_devices_check_tx(device, frequency)) {
    case SubGhzTxAllowed:
        return RfsTxPermAllowed;
    case SubGhzTxBlockedRegionNotProvisioned:
        return RfsTxPermBlockedRegionNotProvisioned;
    case SubGhzTxBlockedRegion:
        return RfsTxPermBlockedRegion;
    case SubGhzTxBlockedDefault:
        return RfsTxPermBlockedDefaultRange;
    default:
        return RfsTxPermUnsupported;
    }
#else
    /* Without a firmware transmit check, report hardware validity only. The firmware still
     * refuses blocked transmissions when the radio is switched to TX. */
    return subghz_devices_is_frequency_valid(device, frequency) ? RfsTxPermAllowed :
                                                                  RfsTxPermUnsupported;
#endif
}

const char* rfs_tx_permission_text(RfsTxPermission permission) {
    switch(permission) {
    case RfsTxPermAllowed:
        return "Allowed";
    case RfsTxPermBlockedRegionNotProvisioned:
        return "Region not set";
    case RfsTxPermBlockedRegion:
        return "Blocked in region";
    case RfsTxPermBlockedDefaultRange:
        return "Needs extended range";
    default:
        return "Not supported";
    }
}
