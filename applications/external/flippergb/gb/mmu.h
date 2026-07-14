#pragma once

#include "address.h"
#include "definitions.h"

class Gameboy;

/* Serial output hook, used by the host test harness to capture Blargg test
 * ROM output. Null on the Flipper build. */
extern "C" {
extern void (*gb_serial_hook)(u8 byte);
}

class MMU {
public:
    MMU(Gameboy& inGb);

    auto read(const Address& address) const -> u8;
    void write(const Address& address, u8 byte);

private:
    auto read_io(const Address& address) const -> u8;
    void write_io(const Address& address, u8 byte);
    void dma_transfer(u8 byte);

    Gameboy& gb;

    u8 work_ram[0x2000] = {}; /* DMG: 8 KB (was 32 KB upstream) */
    u8 oam_ram[0xA0] = {};
    u8 high_ram[0x80] = {};

    u8 serial_data = 0;

    friend class Video;
};
