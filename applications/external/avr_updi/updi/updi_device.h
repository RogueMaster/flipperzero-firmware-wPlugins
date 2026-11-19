#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UPDI_DEVICE_NAME_LEN 16

typedef enum {
    UpdiNvmNone = 0,
    UpdiNvmP0 = '0',
    UpdiNvmP2 = '2',
    UpdiNvmP3 = '3',
    UpdiNvmP4 = '4',
} UpdiNvmFamily;

typedef struct {
    const char* name;
    uint8_t signature[3];
    UpdiNvmFamily nvm_family;
    uint32_t flash_size;
    uint16_t flash_page;
    uint32_t eeprom_size;
    uint16_t eeprom_page;
    uint32_t flash_base;
    uint32_t eeprom_base;
    uint32_t fuse_base;
    uint32_t sigrow_base;
    uint32_t nvmctrl_base;
    uint8_t fuse_count;
    uint8_t addr_bytes; /* 2 for P:0, 3 for P:2/3/4 */
} UpdiDeviceProfile;

typedef struct {
    char name[UPDI_DEVICE_NAME_LEN];
    uint8_t signature[3];
    bool known;
    bool locked;
    char nvm[4];
    char sib[33];
    const UpdiDeviceProfile* profile;
} UpdiTargetInfo;

const UpdiDeviceProfile* updi_device_lookup(const uint8_t signature[3]);
const char* updi_device_name(const uint8_t signature[3]);
void updi_target_set_signature(UpdiTargetInfo* info, const uint8_t signature[3]);
UpdiNvmFamily updi_nvm_family_from_sib(const uint8_t sib[32]);
bool updi_nvm_family_supported(UpdiNvmFamily family);
