#include "updi_device.h"

#include "updi_constants.h"

#include <stdio.h>
#include <string.h>

/* Memory map helpers for modern AVR UPDI parts. */
#define MAP_DX(flash_kb, eeprom_b, page)                                                         \
    .nvm_family = UpdiNvmP2, .flash_size = ((uint32_t)(flash_kb) * 1024u), .flash_page = (page), \
    .eeprom_size = (eeprom_b), .eeprom_page = 32u, .flash_base = 0x00800000u,                    \
    .eeprom_base = 0x1400u, .fuse_base = 0x1050u, .sigrow_base = 0x1100u,                        \
    .nvmctrl_base = 0x1000u, .fuse_count = 12u, .addr_bytes = 3u

#define MAP_P0(flash_kb, eeprom_b, page, flash_map)                                              \
    .nvm_family = UpdiNvmP0, .flash_size = ((uint32_t)(flash_kb) * 1024u), .flash_page = (page), \
    .eeprom_size = (eeprom_b), .eeprom_page = 32u, .flash_base = (flash_map),                    \
    .eeprom_base = 0x1400u, .fuse_base = 0x1280u, .sigrow_base = 0x1100u,                        \
    .nvmctrl_base = 0x1000u, .fuse_count = 10u, .addr_bytes = 2u

#define MAP_EA(flash_kb, eeprom_b, page)                                                         \
    .nvm_family = UpdiNvmP3, .flash_size = ((uint32_t)(flash_kb) * 1024u), .flash_page = (page), \
    .eeprom_size = (eeprom_b), .eeprom_page = 8u, .flash_base = 0x00800000u,                     \
    .eeprom_base = 0x1400u, .fuse_base = 0x1050u, .sigrow_base = 0x1100u,                        \
    .nvmctrl_base = 0x1000u, .fuse_count = 11u, .addr_bytes = 3u

#define MAP_DU(flash_kb, eeprom_b, page)                                                         \
    .nvm_family = UpdiNvmP4, .flash_size = ((uint32_t)(flash_kb) * 1024u), .flash_page = (page), \
    .eeprom_size = (eeprom_b), .eeprom_page = 32u, .flash_base = 0x00800000u,                    \
    .eeprom_base = 0x1400u, .fuse_base = 0x1050u, .sigrow_base = 0x1100u,                        \
    .nvmctrl_base = 0x1000u, .fuse_count = 12u, .addr_bytes = 3u

static const UpdiDeviceProfile updi_devices[] = {
    /* AVR128DA — P:2 */
    {"AVR128DA64", {0x1E, 0x97, 0x07}, MAP_DX(128, 512, 512)},
    {"AVR128DA48", {0x1E, 0x97, 0x08}, MAP_DX(128, 512, 512)},
    {"AVR128DA32", {0x1E, 0x97, 0x09}, MAP_DX(128, 512, 512)},
    {"AVR128DA28", {0x1E, 0x97, 0x0A}, MAP_DX(128, 512, 512)},
    {"AVR128DA64S", {0x1E, 0x97, 0x0F}, MAP_DX(128, 512, 512)},
    {"AVR128DA48S", {0x1E, 0x97, 0x10}, MAP_DX(128, 512, 512)},
    {"AVR128DA32S", {0x1E, 0x97, 0x11}, MAP_DX(128, 512, 512)},
    {"AVR128DA28S", {0x1E, 0x97, 0x12}, MAP_DX(128, 512, 512)},

    /* AVR128DB — P:2 */
    {"AVR128DB64", {0x1E, 0x97, 0x0B}, MAP_DX(128, 512, 512)},
    {"AVR128DB48", {0x1E, 0x97, 0x0C}, MAP_DX(128, 512, 512)},
    {"AVR128DB32", {0x1E, 0x97, 0x0D}, MAP_DX(128, 512, 512)},
    {"AVR128DB28", {0x1E, 0x97, 0x0E}, MAP_DX(128, 512, 512)},

    /* AVR64DB — P:2 */
    {"AVR64DB64", {0x1E, 0x96, 0x16}, MAP_DX(64, 512, 512)},
    {"AVR64DB48", {0x1E, 0x96, 0x17}, MAP_DX(64, 512, 512)},
    {"AVR64DB32", {0x1E, 0x96, 0x18}, MAP_DX(64, 512, 512)},
    {"AVR64DB28", {0x1E, 0x96, 0x19}, MAP_DX(64, 512, 512)},

    /* AVR64DA — P:2 */
    {"AVR64DA64", {0x1E, 0x96, 0x0C}, MAP_DX(64, 512, 512)},
    {"AVR64DA48", {0x1E, 0x96, 0x0D}, MAP_DX(64, 512, 512)},
    {"AVR64DA32", {0x1E, 0x96, 0x0E}, MAP_DX(64, 512, 512)},
    {"AVR64DA28", {0x1E, 0x96, 0x0F}, MAP_DX(64, 512, 512)},

    /* AVR32DA — P:2 */
    {"AVR32DA48", {0x1E, 0x95, 0x34}, MAP_DX(32, 512, 512)},
    {"AVR32DA32", {0x1E, 0x95, 0x33}, MAP_DX(32, 512, 512)},
    {"AVR32DA28", {0x1E, 0x95, 0x32}, MAP_DX(32, 512, 512)},

    /* AVR DD — P:2 */
    {"AVR64DD32", {0x1E, 0x96, 0x1B}, MAP_DX(64, 256, 512)},
    {"AVR64DD28", {0x1E, 0x96, 0x1A}, MAP_DX(64, 256, 512)},
    {"AVR64DD20", {0x1E, 0x96, 0x1C}, MAP_DX(64, 256, 512)},
    {"AVR64DD14", {0x1E, 0x96, 0x1D}, MAP_DX(64, 256, 512)},
    {"AVR32DD32", {0x1E, 0x95, 0x3A}, MAP_DX(32, 256, 512)},
    {"AVR32DD28", {0x1E, 0x95, 0x39}, MAP_DX(32, 256, 512)},
    {"AVR32DD20", {0x1E, 0x95, 0x3B}, MAP_DX(32, 256, 512)},
    {"AVR32DD14", {0x1E, 0x95, 0x3C}, MAP_DX(32, 256, 512)},
    {"AVR16DD32", {0x1E, 0x94, 0x34}, MAP_DX(16, 256, 512)},
    {"AVR16DD28", {0x1E, 0x94, 0x33}, MAP_DX(16, 256, 512)},
    {"AVR16DD20", {0x1E, 0x94, 0x35}, MAP_DX(16, 256, 512)},
    {"AVR16DD14", {0x1E, 0x94, 0x36}, MAP_DX(16, 256, 512)},

    /* megaAVR 0 — P:0 */
    {"ATmega4809", {0x1E, 0x96, 0x51}, MAP_P0(48, 256, 128, 0x4000u)},
    {"ATmega4808", {0x1E, 0x96, 0x50}, MAP_P0(48, 256, 128, 0x4000u)},
    {"ATmega3209", {0x1E, 0x95, 0x53}, MAP_P0(32, 256, 128, 0x4000u)},
    {"ATmega3208", {0x1E, 0x95, 0x52}, MAP_P0(32, 256, 128, 0x4000u)},
    {"ATmega1609", {0x1E, 0x94, 0x26}, MAP_P0(16, 256, 64, 0x0000u)},
    {"ATmega1608", {0x1E, 0x94, 0x27}, MAP_P0(16, 256, 64, 0x0000u)},
    {"ATmega809", {0x1E, 0x93, 0x2A}, MAP_P0(8, 256, 64, 0x0000u)},
    {"ATmega808", {0x1E, 0x93, 0x26}, MAP_P0(8, 256, 64, 0x0000u)},

    /* tinyAVR 1/2 — P:0 */
    {"ATtiny3226", {0x1E, 0x95, 0x28}, MAP_P0(32, 256, 128, 0x8000u)},
    {"ATtiny3224", {0x1E, 0x95, 0x29}, MAP_P0(32, 256, 128, 0x8000u)},
    {"ATtiny1626", {0x1E, 0x94, 0x2D}, MAP_P0(16, 256, 64, 0x8000u)},
    {"ATtiny1624", {0x1E, 0x94, 0x2C}, MAP_P0(16, 256, 64, 0x8000u)},
    {"ATtiny1616", {0x1E, 0x94, 0x21}, MAP_P0(16, 256, 64, 0x8000u)},
    {"ATtiny1614", {0x1E, 0x94, 0x22}, MAP_P0(16, 256, 64, 0x8000u)},
    {"ATtiny816", {0x1E, 0x93, 0x21}, MAP_P0(8, 128, 64, 0x8000u)},
    {"ATtiny814", {0x1E, 0x93, 0x22}, MAP_P0(8, 128, 64, 0x8000u)},
    {"ATtiny416", {0x1E, 0x92, 0x21}, MAP_P0(4, 128, 64, 0x8000u)},
    {"ATtiny414", {0x1E, 0x92, 0x22}, MAP_P0(4, 128, 64, 0x8000u)},
    {"ATtiny412", {0x1E, 0x92, 0x23}, MAP_P0(4, 128, 64, 0x8000u)},
    {"ATtiny214", {0x1E, 0x91, 0x21}, MAP_P0(2, 64, 64, 0x0000u)},
    {"ATtiny212", {0x1E, 0x91, 0x20}, MAP_P0(2, 64, 64, 0x0000u)},

    /* AVR EA — P:3 */
    {"AVR64EA48", {0x1E, 0x96, 0x20}, MAP_EA(64, 512, 128)},
    {"AVR64EA32", {0x1E, 0x96, 0x21}, MAP_EA(64, 512, 128)},
    {"AVR64EA28", {0x1E, 0x96, 0x22}, MAP_EA(64, 512, 128)},
    {"AVR32EA48", {0x1E, 0x95, 0x3D}, MAP_EA(32, 512, 128)},
    {"AVR32EA32", {0x1E, 0x95, 0x3E}, MAP_EA(32, 512, 128)},
    {"AVR32EA28", {0x1E, 0x95, 0x3F}, MAP_EA(32, 512, 128)},
    {"AVR16EA48", {0x1E, 0x94, 0x37}, MAP_EA(16, 512, 64)},
    {"AVR16EA32", {0x1E, 0x94, 0x38}, MAP_EA(16, 512, 64)},
    {"AVR16EA28", {0x1E, 0x94, 0x39}, MAP_EA(16, 512, 64)},

    /* AVR DU — P:4 */
    {"AVR64DU28", {0x1E, 0x96, 0x23}, MAP_DU(64, 512, 512)},
    {"AVR64DU32", {0x1E, 0x96, 0x24}, MAP_DU(64, 512, 512)},
    {"AVR32DU14", {0x1E, 0x95, 0x40}, MAP_DU(32, 512, 512)},
    {"AVR32DU20", {0x1E, 0x95, 0x41}, MAP_DU(32, 512, 512)},
    {"AVR32DU28", {0x1E, 0x95, 0x42}, MAP_DU(32, 512, 512)},
    {"AVR32DU32", {0x1E, 0x95, 0x43}, MAP_DU(32, 512, 512)},
    {"AVR16DU14", {0x1E, 0x94, 0x3A}, MAP_DU(16, 512, 512)},
    {"AVR16DU20", {0x1E, 0x94, 0x3B}, MAP_DU(16, 512, 512)},
    {"AVR16DU28", {0x1E, 0x94, 0x3C}, MAP_DU(16, 512, 512)},
    {"AVR16DU32", {0x1E, 0x94, 0x3D}, MAP_DU(16, 512, 512)},
};

const UpdiDeviceProfile* updi_device_lookup(const uint8_t signature[3]) {
    for(size_t i = 0; i < sizeof(updi_devices) / sizeof(updi_devices[0]); i++) {
        if(memcmp(updi_devices[i].signature, signature, 3) == 0) return &updi_devices[i];
    }
    return NULL;
}

const char* updi_device_name(const uint8_t signature[3]) {
    const UpdiDeviceProfile* profile = updi_device_lookup(signature);
    return profile != NULL ? profile->name : NULL;
}

void updi_target_set_signature(UpdiTargetInfo* info, const uint8_t signature[3]) {
    memcpy(info->signature, signature, 3);
    const UpdiDeviceProfile* profile = updi_device_lookup(signature);
    info->profile = profile;
    info->known = profile != NULL;
    if(profile != NULL) {
        snprintf(info->name, sizeof(info->name), "%s", profile->name);
    } else {
        snprintf(info->name, sizeof(info->name), "Unknown");
    }
}

UpdiNvmFamily updi_nvm_family_from_sib(const uint8_t sib[32]) {
    if(sib[UPDI_SIB_NVM_INDEX] != 'P' || sib[UPDI_SIB_NVM_INDEX + 1] != ':') {
        return UpdiNvmNone;
    }
    char dig = (char)sib[UPDI_SIB_NVM_INDEX + 2];
    if(dig == '0' || dig == '2' || dig == '3' || dig == '4') {
        return (UpdiNvmFamily)dig;
    }
    return UpdiNvmNone;
}

bool updi_nvm_family_supported(UpdiNvmFamily family) {
    return family == UpdiNvmP0 || family == UpdiNvmP2 || family == UpdiNvmP3 ||
           family == UpdiNvmP4;
}
