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

    /* Serial link: a master transfer (SC=0x81) with no cable completes
     * after 8 bit-clocks reading 0xFF and raises the serial interrupt.
     * Without this, games that probe the link on demand (Tetris 2P
     * handshake at the title screen!) busy-wait forever. Ticked from
     * Gameboy::sync_peripherals; defined inline in gameboy.h. */
    void serial_tick(uint cycles);

    /* save-state accessors */
    auto wram_ptr() -> u8* { return work_ram; }
    auto oam_ptr() -> u8* { return oam_ram; }
    auto hram_ptr() -> u8* { return high_ram; }
    void export_serial(u8* sb, u8* sc, u32* clk) const {
        *sb = serial_data;
        *sc = serial_control;
        *clk = serial_clocks;
    }
    void import_serial(u8 sb, u8 sc, u32 clk) {
        serial_data = sb;
        serial_control = sc;
        serial_clocks = clk;
    }

private:
    auto read_io(const Address& address) const -> u8;
    void write_io(const Address& address, u8 byte);
    void dma_transfer(u8 byte);

    Gameboy& gb;

    u8 work_ram[0x2000] = {}; /* DMG: 8 KB (was 32 KB upstream) */
    u8 oam_ram[0xA0] = {};
    u8 high_ram[0x80] = {};

    u8 serial_data = 0;
    u8 serial_control = 0;
    uint serial_clocks = 0; /* M-cycles until the running transfer completes */

    friend class Video;
};
