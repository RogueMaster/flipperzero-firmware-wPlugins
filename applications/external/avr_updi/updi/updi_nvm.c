#include "updi_nvm.h"

#include "updi_constants.h"

#include <furi.h>

#include <stdio.h>
#include <string.h>

/* Shared status/command offsets differ slightly by family. */
typedef struct {
    uint8_t status_off;
    uint8_t error_mask;
    uint8_t error_pos;
    uint8_t eebusy;
    uint8_t fbusy;
    uint8_t nocmd;
    uint8_t chip_erase;
} UpdiNvmRegs;

static UpdiNvmRegs updi_nvm_regs(UpdiNvmFamily family) {
    UpdiNvmRegs regs = {
        .status_off = 0x02,
        .error_mask = 0x70,
        .error_pos = 4,
        .eebusy = 0x02,
        .fbusy = 0x01,
        .nocmd = 0x00,
        .chip_erase = 0x20,
    };
    if(family == UpdiNvmP0) {
        regs.error_mask = 0x04;
        regs.error_pos = 2;
        regs.chip_erase = 0x05;
    } else if(family == UpdiNvmP3 || family == UpdiNvmP4) {
        regs.status_off = 0x06;
        regs.eebusy = 0x01;
        regs.fbusy = 0x02;
    }
    return regs;
}

static void updi_nvm_error_code(UpdiLink* link, uint8_t code) {
    const char* name = "error";
    if(code == 1)
        name = "invalid command";
    else if(code == 2)
        name = "write protected";
    else if(code == 3)
        name = "command collision";
    snprintf(link->error, link->error_size, "NVM %s (%u)", name, code);
}

static bool updi_nvm_wait(UpdiLink* link, const UpdiDeviceProfile* profile, uint32_t timeout_ms) {
    UpdiNvmRegs regs = updi_nvm_regs(profile->nvm_family);
    uint32_t start = furi_get_tick();
    uint32_t limit = furi_ms_to_ticks(timeout_ms);
    while(true) {
        uint8_t status = 0;
        if(!updi_link_ld8(link, profile->nvmctrl_base + regs.status_off, &status)) return false;
        uint8_t code = (uint8_t)((status & regs.error_mask) >> regs.error_pos);
        if(code != 0) {
            updi_nvm_error_code(link, code);
            return false;
        }
        if((status & (regs.eebusy | regs.fbusy)) == 0) return true;
        if((furi_get_tick() - start) >= limit) {
            snprintf(link->error, link->error_size, "NVM busy timeout");
            return false;
        }
        furi_delay_ms(1);
    }
}

static bool updi_nvm_command(UpdiLink* link, const UpdiDeviceProfile* profile, uint8_t command) {
    return updi_link_st8(link, profile->nvmctrl_base + 0x00u, command);
}

static bool updi_nvm_write_words_page(
    UpdiLink* link,
    uint32_t abs_address,
    const uint8_t* page,
    uint16_t page_size) {
    for(uint32_t offset = 0; offset < page_size; offset += 64) {
        uint16_t chunk = (uint16_t)(page_size - offset);
        if(chunk > 64) chunk = 64;
        if((chunk % 2u) != 0) {
            snprintf(link->error, link->error_size, "Odd flash page size");
            return false;
        }
        if(!updi_link_write_words(link, abs_address + offset, page + offset, chunk)) return false;
    }
    return true;
}

bool updi_nvm_chip_erase(UpdiLink* link, const UpdiDeviceProfile* profile) {
    UpdiNvmRegs regs = updi_nvm_regs(profile->nvm_family);
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, regs.chip_erase)) return false;
    if(!updi_nvm_wait(link, profile, 2000)) return false;
    return updi_nvm_command(link, profile, regs.nocmd);
}

static bool updi_nvm_write_flash_p0(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page) {
    uint32_t abs = profile->flash_base + byte_address;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, 0x04)) return false; /* PAGE_BUFFER_CLR */
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_write_words_page(link, abs, page, profile->flash_page)) return false;
    if(!updi_nvm_command(link, profile, 0x03)) return false; /* ERASE_WRITE_PAGE */
    if(!updi_nvm_wait(link, profile, 500)) return false;
    return updi_nvm_command(link, profile, 0x00);
}

static bool updi_nvm_write_flash_p2(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page) {
    uint32_t abs = profile->flash_base + byte_address;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, 0x08)) return false; /* FLPER */
    if(!updi_link_st8(link, abs, 0xFF)) return false;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, 0x00)) return false;

    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, 0x02)) return false; /* FLWR */
    if(!updi_nvm_write_words_page(link, abs, page, profile->flash_page)) return false;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    return updi_nvm_command(link, profile, 0x00);
}

static bool updi_nvm_write_flash_p3(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page) {
    uint32_t abs = profile->flash_base + byte_address;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, 0x0F)) return false; /* PAGE_BUFFER_CLR */
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_write_words_page(link, abs, page, profile->flash_page)) return false;
    if(!updi_nvm_command(link, profile, 0x05)) return false; /* PAGE_ERASE_WRITE */
    if(!updi_nvm_wait(link, profile, 500)) return false;
    return updi_nvm_command(link, profile, 0x00);
}

bool updi_nvm_write_flash_page(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page) {
    if(profile == NULL || page == NULL) {
        snprintf(link->error, link->error_size, "Bad flash page");
        return false;
    }
    if((byte_address % profile->flash_page) != 0 || byte_address >= profile->flash_size) {
        snprintf(link->error, link->error_size, "Bad flash page");
        return false;
    }

    switch(profile->nvm_family) {
    case UpdiNvmP0:
        return updi_nvm_write_flash_p0(link, profile, byte_address, page);
    case UpdiNvmP2:
        return updi_nvm_write_flash_p2(link, profile, byte_address, page);
    case UpdiNvmP3:
        return updi_nvm_write_flash_p3(link, profile, byte_address, page);
    case UpdiNvmP4:
        return updi_nvm_write_flash_p2(link, profile, byte_address, page);
    default:
        snprintf(link->error, link->error_size, "Unsupported NVM");
        return false;
    }
}

bool updi_nvm_read_memory(UpdiLink* link, uint32_t abs_address, uint8_t* data, size_t size) {
    while(size > 0) {
        size_t chunk = size > UPDI_MAX_REPEAT ? UPDI_MAX_REPEAT : size;
        if(!updi_link_read(link, abs_address, data, chunk)) return false;
        abs_address += (uint32_t)chunk;
        data += chunk;
        size -= chunk;
    }
    return true;
}

bool updi_nvm_write_eeprom_page(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint32_t byte_address,
    const uint8_t* page,
    size_t length) {
    if(profile == NULL || page == NULL || length == 0 || length > profile->eeprom_page) {
        snprintf(link->error, link->error_size, "Bad EEPROM page");
        return false;
    }
    if(byte_address + length > profile->eeprom_size) {
        snprintf(link->error, link->error_size, "EEPROM past end");
        return false;
    }

    uint32_t abs = profile->eeprom_base + byte_address;
    uint8_t cmd = 0x13; /* P2/P4 EEPROM erase/write */
    uint8_t clr = 0x00;

    if(profile->nvm_family == UpdiNvmP0) {
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_nvm_command(link, profile, 0x04)) return false; /* buffer clear */
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_link_write_bytes(link, abs, page, length)) return false;
        if(!updi_nvm_command(link, profile, 0x03)) return false; /* erase-write page */
        if(!updi_nvm_wait(link, profile, 500)) return false;
        return updi_nvm_command(link, profile, 0x00);
    }

    if(profile->nvm_family == UpdiNvmP3) {
        clr = 0x1F;
        cmd = 0x15;
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_nvm_command(link, profile, clr)) return false;
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_link_write_bytes(link, abs, page, length)) return false;
        if(!updi_nvm_command(link, profile, cmd)) return false;
        if(!updi_nvm_wait(link, profile, 500)) return false;
        return updi_nvm_command(link, profile, 0x00);
    }

    /* P:2 / P:4 — command then write bytes */
    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, cmd)) return false;
    if(!updi_link_write_bytes(link, abs, page, length)) return false;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    return updi_nvm_command(link, profile, 0x00);
}

bool updi_nvm_write_fuse(
    UpdiLink* link,
    const UpdiDeviceProfile* profile,
    uint8_t fuse_index,
    uint8_t value) {
    if(profile == NULL || fuse_index >= profile->fuse_count) {
        snprintf(link->error, link->error_size, "Bad fuse index");
        return false;
    }

    uint32_t abs = profile->fuse_base + fuse_index;

    if(profile->nvm_family == UpdiNvmP0) {
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_link_st8(link, profile->nvmctrl_base + 0x08u, (uint8_t)abs)) return false;
        if(!updi_link_st8(link, profile->nvmctrl_base + 0x09u, (uint8_t)(abs >> 8))) return false;
        if(!updi_link_st8(link, profile->nvmctrl_base + 0x06u, value)) return false;
        if(!updi_nvm_command(link, profile, 0x07)) return false; /* WRITE_FUSE */
        if(!updi_nvm_wait(link, profile, 500)) return false;
        return updi_nvm_command(link, profile, 0x00);
    }

    /* Dx / EA / DU treat fuses like EEPROM-style NVM at fuse_base. */
    uint8_t cmd = profile->nvm_family == UpdiNvmP3 ? 0x15 : 0x13;
    if(profile->nvm_family == UpdiNvmP3) {
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_nvm_command(link, profile, 0x1F)) return false;
        if(!updi_nvm_wait(link, profile, 500)) return false;
        if(!updi_link_write_bytes(link, abs, &value, 1)) return false;
        if(!updi_nvm_command(link, profile, cmd)) return false;
        if(!updi_nvm_wait(link, profile, 500)) return false;
        return updi_nvm_command(link, profile, 0x00);
    }

    if(!updi_nvm_wait(link, profile, 500)) return false;
    if(!updi_nvm_command(link, profile, cmd)) return false;
    if(!updi_link_write_bytes(link, abs, &value, 1)) return false;
    if(!updi_nvm_wait(link, profile, 500)) return false;
    return updi_nvm_command(link, profile, 0x00);
}
