#pragma once

#include "definitions.h"

/* Cartridge with pluggable ROM bank provider.
 *
 * Instead of holding the whole ROM in RAM (impossible on Flipper Zero for
 * anything above 32 KB), the cartridge asks the platform for a pointer to a
 * 16 KB bank whenever the game switches banks. On the desktop test build the
 * provider just returns `rom + bank * 0x4000`; on the Flipper it is backed
 * by an LRU cache streaming from the SD card.
 *
 * Supported mappers: ROM only, MBC1 (incl. upper bits / mode select),
 * MBC2 (built-in 512x4 RAM), MBC3 (no RTC), MBC5.
 */

using RomBankProvider = const u8* (*)(void* ctx, uint bank);

enum class MBCType : u8 {
    None,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    Unsupported,
};

class Cartridge {
public:
    /* bank0 must stay valid for the lifetime of the cartridge */
    void init(
        const u8* bank0_data,
        uint rom_bank_count,
        MBCType mbc_type,
        u8* cart_ram,
        u32 cart_ram_size,
        RomBankProvider provider,
        void* provider_ctx);

    auto read(u16 addr) const -> u8 {
        if(addr < 0x4000) return bank0[addr];
        if(addr < 0x8000) return bankN[addr - 0x4000];
        /* 0xA000 - 0xBFFF: cartridge RAM */
        return read_ram(addr);
    }

    void write(u16 addr, u8 value);

    auto get_ram() -> u8* {
        return ram;
    }
    auto get_ram_size() const -> u32 {
        return ram_size;
    }

    /* Header helpers (operate on the first bank) */
    static auto parse_mbc(u8 cartridge_type_byte) -> MBCType;
    static auto has_battery(u8 cartridge_type_byte) -> bool;
    static auto rom_bank_count_from_header(u8 rom_size_byte) -> uint;
    static auto ram_size_from_header(u8 ram_size_byte, MBCType mbc) -> u32;

private:
    auto read_ram(u16 addr) const -> u8;
    void write_ram(u16 addr, u8 value);
    void update_rom_bank();

    const u8* bank0 = nullptr;
    const u8* bankN = nullptr;

    u8* ram = nullptr;
    u32 ram_size = 0;

    RomBankProvider provider = nullptr;
    void* provider_ctx = nullptr;

    MBCType mbc = MBCType::None;
    uint bank_count = 2;

    bool ram_enabled = false;
    bool advanced_banking_mode = false; /* MBC1 mode 1 */
    uint bank_low = 1; /* MBC1: 5 bits, MBC3: 7 bits, MBC5: 8 bits */
    uint bank_high = 0; /* MBC1: 2 bits, MBC5: 9th bit */
    uint ram_bank = 0;
};
