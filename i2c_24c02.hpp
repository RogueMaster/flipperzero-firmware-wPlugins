#pragma once

#include <stdint.h>
#include <stdbool.h>

// 24Cxx EEPROM I2C base addresses (7-bit)
#define EEPROM_24CXX_BASE_ADDR 0x50
#define EEPROM_24CXX_MAX_ADDR  0x57

// I2C operation timeout (ms)
#define EEPROM_I2C_TIMEOUT 100

// Address width: chips up to 24C16 use 1-byte addressing;
// chips from 24C32 onward use 2-byte addressing.
typedef enum {
    EEPROM_ADDR_1BYTE = 1,
    EEPROM_ADDR_2BYTE = 2,
} EEPROMAddrWidth;

class EEPROM24Cxx {
private:
    uint8_t _i2c_addr_8bit;
    EEPROMAddrWidth _addr_width;
    uint8_t _page_size;

public:
    // addr_width : EEPROM_ADDR_1BYTE for <=24C16, EEPROM_ADDR_2BYTE for >=24C32
    // page_size  : 8 for 24C01/02, 16 for 24C04-16, 32 for 24C32/64,
    //              64 for 24C128/256, 128 for 24C512
    EEPROM24Cxx(
        uint8_t i2c_address_7bit,
        EEPROMAddrWidth addr_width = EEPROM_ADDR_1BYTE,
        uint8_t page_size = 8);

    bool init();
    bool isAvailable();

    // All addresses are uint16_t — covers up to 64 KB (24C512)
    bool readByte(uint16_t memory_addr, uint8_t& data);
    bool writeByte(uint16_t memory_addr, uint8_t data);
    bool readBytes(uint16_t start_addr, uint8_t* buffer, uint16_t length);
    bool writeBytes(uint16_t start_addr, const uint8_t* buffer, uint16_t length);

    bool eraseAll(uint32_t total_size);
    bool eraseRange(uint16_t start_addr, uint16_t length);

    void setAddress(uint8_t i2c_address_7bit);
    uint8_t getAddress() const;

    // Update chip parameters at runtime when the user changes chip type
    void setChipParams(EEPROMAddrWidth addr_width, uint8_t page_size);
    uint8_t getPageSize() const {
        return _page_size;
    }
};
