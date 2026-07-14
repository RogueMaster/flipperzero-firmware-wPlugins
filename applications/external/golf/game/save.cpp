#include "game.hpp"

#if !SAVE_TO_FLASH_CHIP

#include "lib/EEPROM.h"

static constexpr uint16_t EEPROM_START_ADDRESS = 0;

static inline uint8_t eeprom_read(int addr) {
    return EEPROM.read((int)(uintptr_t)addr);
}

static inline void eeprom_update(int addr, uint8_t value) {
    EEPROM.update((int)(uintptr_t)addr, value);
}
#else

#if GEN_FXSAVE
static uint8_t FX_SAVE[4096 * 2];
#endif

static void fx_read_save_bytes(uint24_t addr, uint8_t* p, size_t n) {
#if GEN_FXSAVE
    memcpy(p, &FX_SAVE[addr], n);
#else
    FX::readSaveBytes(addr, p, n);
    FX::waitWhileBusy();
#endif
}

static void fx_erase_save_block(uint16_t page) {
#if GEN_FXSAVE
    myassert((page & 0xf) == 0);
    myassert(page * 256 + 4096 <= sizeof(FX_SAVE));
    memset(&FX_SAVE[page * 256], 0xff, 4096);
#else
    FX::eraseSaveBlock(page);
    FX::waitWhileBusy();
#endif
}

static void fx_write_save_page(uint16_t page, uint8_t* p) {
#if GEN_FXSAVE
    myassert(page * 256 + 256 <= sizeof(FX_SAVE));
    for(int i = 0; i < 256; ++i)
        FX_SAVE[page * 256 + i] &= p[i];
#else
    FX::writeSavePage(page, p);
    FX::waitWhileBusy();
#endif
}

#endif

uint16_t checksum() {
    // CRC16
    uint8_t x;
    uint16_t crc = 0xffff;
    for(uint16_t i = 0; i < sizeof(course_save_data) - 2; ++i) {
        x = (crc >> 8) ^ ((uint8_t*)&savedata)[i];
        x ^= x >> 4;
        crc = (crc << 8) ^ (uint16_t(x) << 12) ^ (uint16_t(x) << 5) ^ (uint16_t(x) << 0);
    }
    return crc;
}

static char const SAVE_IDENT[8] PROGMEM = {'A', 'R', 'D', 'U', 'G', 'O', 'L', 'F'};

void load() {
#if SAVE_TO_FLASH_CHIP
    fx_read_save_bytes(fx_course * 64, (uint8_t*)&savedata, 64);
#else
    for(uint8_t i = 0; i < sizeof(course_save_data); ++i)
        ((uint8_t*)&savedata)[i] = eeprom_read(EEPROM_START_ADDRESS + i);
#endif

    bool clear = false;

    for(uint8_t i = 0; i < 8; ++i)
        if(savedata.ident[i] != (char)pgm_read_byte(&SAVE_IDENT[i])) clear = true;
    if(savedata.checksum != checksum()) clear = true;

    if(clear) {
        // clear everything except identifier and checksum
        memset((uint8_t*)&savedata + 8, 0xff, sizeof(course_save_data) - 8 - 2);
        // don't overwrite identifier when saving to flash chip: it serves
        // as an indicator that we're saving to the correct location and
        // not erasing something else on the chip. the identifier comes
        // pre-written in the save bin file
#if !SAVE_TO_FLASH_CHIP
        for(uint8_t i = 0; i < 8; ++i)
            savedata.ident[i] = (char)pgm_read_byte(&SAVE_IDENT[i]);
#endif
        savedata.num_played = 0;
        savedata.checksum = checksum();
    }
}

void save() {
    // refuse to save without valid identifier
    for(uint8_t i = 0; i < 8; ++i)
        if(savedata.ident[i] != (char)pgm_read_byte(&SAVE_IDENT[i])) return;

#if SAVE_TO_FLASH_CHIP

    static_assert(sizeof(fs) >= 256, "");

    // save pages on flash chip must be erased in blocks of 4KB
    // before being rewritten. to do this we keep two duplicate blocks,
    // erase one, copy from the other while updating relevant page,
    // then copy back to maintain the duplicate

    uint8_t page = fx_course >> 2;
    uint8_t index = fx_course << 6;

    // 1. erase first block
    fx_erase_save_block(0);

    // 2. copy from second block to first block (modify relevant page)
    for(uint8_t p = 0; p < 16; ++p) {
        uint8_t* pd = (uint8_t*)&fs[0];

        // read page from second block
        fx_read_save_bytes(uint16_t(p) * 256 + 4096, pd, 256);

        // overwrite page data if necessary
        if(p == page) memcpy(&pd[index], &savedata, 64);

        // write page to first block
        fx_write_save_page(p, pd);
    }

    // 3. erase second block
    fx_erase_save_block(16);

    // 4. copy from first block to second block
    for(uint8_t p = 0; p < 16; ++p) {
        uint8_t* pd = (uint8_t*)&fs[0];

        // read page from first block and write to second block
        fx_read_save_bytes(uint16_t(p) * 256, pd, 256);
        fx_write_save_page(p + 16, pd);
    }

#else
    for(uint8_t i = 0; i < sizeof(course_save_data); ++i)
        eeprom_update(EEPROM_START_ADDRESS + i, ((uint8_t*)&savedata)[i]);
    (void)EEPROM.commit();
#endif
}
