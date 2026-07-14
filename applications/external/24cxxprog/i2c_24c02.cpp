#include "i2c_24c02.hpp"
#include "furi_hal_i2c.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
EEPROM24Cxx::EEPROM24Cxx(uint8_t i2c_address_7bit, EEPROMAddrWidth addr_width, uint8_t page_size)
    : _i2c_addr_8bit(i2c_address_7bit << 1)
    , _addr_width(addr_width)
    , _page_size(page_size) {
}

// ---------------------------------------------------------------------------
// Runtime reconfiguration (called when the user picks a different chip type)
// ---------------------------------------------------------------------------
void EEPROM24Cxx::setChipParams(EEPROMAddrWidth addr_width, uint8_t page_size) {
    _addr_width = addr_width;
    _page_size = page_size;
}

// ---------------------------------------------------------------------------
// Address helpers
// ---------------------------------------------------------------------------
void EEPROM24Cxx::setAddress(uint8_t i2c_address_7bit) {
    _i2c_addr_8bit = i2c_address_7bit << 1;
}

uint8_t EEPROM24Cxx::getAddress() const {
    return _i2c_addr_8bit >> 1;
}

// ---------------------------------------------------------------------------
// Availability check
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::isAvailable() {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_is_device_ready(
        &furi_hal_i2c_handle_external, _i2c_addr_8bit, EEPROM_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool EEPROM24Cxx::init() {
    return isAvailable();
}

// ---------------------------------------------------------------------------
// Single-byte read
//
// Protocol (applies to ALL chip sizes):
//   START  DEV_ADDR+W  [ADDR_H]  ADDR_L
//   RESTART  DEV_ADDR+R  DATA  STOP
//
// For 1-byte-address chips (<=24C16) ADDR_H is omitted.
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::readByte(uint16_t memory_addr, uint8_t& data) {
    // Build address bytes
    uint8_t addr_buf[2];
    uint8_t addr_len;
    if(_addr_width == EEPROM_ADDR_2BYTE) {
        addr_buf[0] = (uint8_t)(memory_addr >> 8);
        addr_buf[1] = (uint8_t)(memory_addr & 0xFF);
        addr_len = 2;
    } else {
        addr_buf[0] = (uint8_t)(memory_addr & 0xFF);
        addr_len = 1;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ok_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false,
        addr_buf,
        addr_len,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        EEPROM_I2C_TIMEOUT);

    bool ok_rx = false;
    if(ok_tx) {
        ok_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            _i2c_addr_8bit,
            false,
            &data,
            1,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            EEPROM_I2C_TIMEOUT);
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok_tx && ok_rx;
}

// ---------------------------------------------------------------------------
// Sequential read of `length` bytes starting at `start_addr`
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::readBytes(uint16_t start_addr, uint8_t* buffer, uint16_t length) {
    if(length == 0 || buffer == nullptr) return false;

    uint8_t addr_buf[2];
    uint8_t addr_len;
    if(_addr_width == EEPROM_ADDR_2BYTE) {
        addr_buf[0] = (uint8_t)(start_addr >> 8);
        addr_buf[1] = (uint8_t)(start_addr & 0xFF);
        addr_len = 2;
    } else {
        addr_buf[0] = (uint8_t)(start_addr & 0xFF);
        addr_len = 1;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ok_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false,
        addr_buf,
        addr_len,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        EEPROM_I2C_TIMEOUT);

    bool ok_rx = false;
    if(ok_tx) {
        ok_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            _i2c_addr_8bit,
            false,
            buffer,
            length,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            EEPROM_I2C_TIMEOUT);
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok_tx && ok_rx;
}

// ---------------------------------------------------------------------------
// Single-byte write
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::writeByte(uint16_t memory_addr, uint8_t data) {
    uint8_t buf[3]; // up to 2 addr bytes + 1 data byte
    uint8_t len;
    if(_addr_width == EEPROM_ADDR_2BYTE) {
        buf[0] = (uint8_t)(memory_addr >> 8);
        buf[1] = (uint8_t)(memory_addr & 0xFF);
        buf[2] = data;
        len = 3;
    } else {
        buf[0] = (uint8_t)(memory_addr & 0xFF);
        buf[1] = data;
        len = 2;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ok = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false,
        buf,
        len,
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        EEPROM_I2C_TIMEOUT);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(ok) furi_delay_ms(10); // tWR write-cycle time
    return ok;
}

// ---------------------------------------------------------------------------
// Page-aware multi-byte write
//
// The EEPROM's internal write pointer wraps within a page.  We must split
// writes at page boundaries so we never send data that crosses one in a
// single I2C transaction.
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::writeBytes(uint16_t start_addr, const uint8_t* buffer, uint16_t length) {
    if(length == 0 || buffer == nullptr) return false;

    // Maximum tx buffer: 2 addr bytes + one full page of data
    // The largest page we support is 128 bytes (24C512), so 130 bytes total.
    uint8_t tx_buf[2 + 128];
    uint16_t written = 0;

    while(written < length) {
        uint16_t cur_addr = start_addr + written;
        // Bytes remaining until the next page boundary
        uint16_t page_offset = cur_addr % _page_size;
        uint16_t space_in_page = _page_size - page_offset;
        uint16_t chunk = (length - written < space_in_page) ? (length - written) : space_in_page;

        // Build the TX buffer: [addrH,] addrL, data...
        uint8_t header_len;
        if(_addr_width == EEPROM_ADDR_2BYTE) {
            tx_buf[0] = (uint8_t)(cur_addr >> 8);
            tx_buf[1] = (uint8_t)(cur_addr & 0xFF);
            header_len = 2;
        } else {
            tx_buf[0] = (uint8_t)(cur_addr & 0xFF);
            header_len = 1;
        }
        memcpy(&tx_buf[header_len], buffer + written, chunk);

        furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
        bool ok = furi_hal_i2c_tx_ext(
            &furi_hal_i2c_handle_external,
            _i2c_addr_8bit,
            false,
            tx_buf,
            header_len + chunk,
            FuriHalI2cBeginStart,
            FuriHalI2cEndStop,
            EEPROM_I2C_TIMEOUT);
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);

        if(!ok) return false;

        furi_delay_ms(10); // tWR — wait for internal write cycle
        written += chunk;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Erase entire chip (fill with 0xFF)
// total_size is passed in so the driver doesn't need to know the chip size.
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::eraseAll(uint32_t total_size) {
    uint8_t page_buf[128];
    memset(page_buf, 0xFF, sizeof(page_buf));

    for(uint32_t addr = 0; addr < total_size; addr += _page_size) {
        uint16_t chunk = (total_size - addr < _page_size) ? (uint16_t)(total_size - addr) :
                                                            _page_size;
        if(!writeBytes((uint16_t)addr, page_buf, chunk)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Erase a byte range (fill with 0xFF)
// ---------------------------------------------------------------------------
bool EEPROM24Cxx::eraseRange(uint16_t start_addr, uint16_t length) {
    if(length == 0) return false;
    uint8_t page_buf[128];
    memset(page_buf, 0xFF, sizeof(page_buf));
    return writeBytes(start_addr, page_buf, length);
}
