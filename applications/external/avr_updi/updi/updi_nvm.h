#pragma once

#include "updi_device.h"
#include "updi_link.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool updi_nvm_chip_erase(UpdiLink* link, const UpdiDeviceProfile* profile);
bool updi_nvm_write_flash_page(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page);
bool updi_nvm_read_memory(UpdiLink* link, uint32_t abs_address, uint8_t* data, size_t size);
bool updi_nvm_write_eeprom_page(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page,
    size_t length);
bool updi_nvm_write_fuse(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint8_t fuse_index,
    uint8_t value);
