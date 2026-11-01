/* Cold code: size-optimized on the embedded target. Every KB of binary
 * is a KB of heap the ROM page cache loses (the FAP loads into RAM), and
 * nothing in this file runs per-instruction. */
#if defined(__arm__) && defined(__GNUC__)
#pragma GCC optimize("Os")
#endif

#include "cartridge.h"

void Cartridge::init(
    const u8* bank0_data,
    uint rom_bank_count,
    MBCType mbc_type,
    u8* cart_ram,
    u32 cart_ram_size,
    RomBankProvider rom_provider,
    void* rom_provider_ctx) {
    bank0 = bank0_data;
    bank_count = rom_bank_count > 0 ? rom_bank_count : 2;
    mbc = mbc_type;
    ram = cart_ram;
    ram_size = cart_ram_size;
    provider = rom_provider;
    provider_ctx = rom_provider_ctx;

    ram_enabled = false;
    advanced_banking_mode = false;
    bank_low = 1;
    bank_high = 0;
    ram_bank = 0;
    cur_bank = 0xFFFFFFFFu;

    update_rom_bank();
}

void Cartridge::update_rom_bank() {
    uint bank;

    switch(mbc) {
    case MBCType::None:
        bank = 1;
        break;
    case MBCType::MBC1:
        bank = (bank_high << 5) | bank_low;
        /* bank_low == 0 is translated to 1 at write time */
        break;
    case MBCType::MBC1M:
        /* multicart wiring: only 4 low bits reach the ROM, the high
         * register selects the sub-game */
        bank = (bank_high << 4) | (bank_low & 0x0F);
        break;
    case MBCType::MBC2:
    case MBCType::MBC3:
        bank = bank_low;
        break;
    case MBCType::MBC5:
        /* MBC5 genuinely allows bank 0 in the switchable slot */
        bank = (bank_high << 8) | bank_low;
        break;
    default:
        bank = 1;
        break;
    }

    if(bank_count) bank %= bank_count;

    /* Bank switching is FREE: quarters are unmapped and fetched lazily
     * on first read. Games rewrite the bank register constantly without
     * reading -- eager fetching here was ~86 phantom misses per frame. */
    if(bank == cur_bank) return;
    cur_bank = bank;
    bankN_q[0] = bankN_q[1] = bankN_q[2] = bankN_q[3] = nullptr;
}

void Cartridge::fetch_quarter(uint q) const {
    /* Refresh the LRU stamps of the quarters already mapped so this
     * fetch can never evict them (the provider protects the four most
     * recently returned units; at most 3 are mapped when we get here). */
    for(uint i = 0; i < 4; i++) {
        if(bankN_q[i]) bankN_q[i] = provider(provider_ctx, cur_bank * 4 + i);
    }
    bankN_q[q] = provider(provider_ctx, cur_bank * 4 + q);
}

void Cartridge::write(u16 addr, u8 value) {
    if(addr >= 0xA000) {
        write_ram(addr, value);
        return;
    }

    switch(mbc) {
    case MBCType::None:
        return;

    case MBCType::MBC1M:
    case MBCType::MBC1:
        if(addr < 0x2000) {
            ram_enabled = (value & 0x0F) == 0x0A;
        } else if(addr < 0x4000) {
            bank_low = value & 0x1F;
            if(bank_low == 0) bank_low = 1;
            update_rom_bank();
        } else if(addr < 0x6000) {
            bank_high = value & 0x03;
            if(advanced_banking_mode) {
                ram_bank = value & 0x03;
            }
            update_rom_bank();
        } else {
            advanced_banking_mode = (value & 0x01) != 0;
            ram_bank = advanced_banking_mode ? (bank_high & 0x03) : 0;
            update_rom_bank();
        }
        return;

    case MBCType::MBC2:
        if(addr < 0x4000) {
            /* bit 8 of the address selects RAM-enable vs ROM-bank */
            if(addr & 0x0100) {
                bank_low = value & 0x0F;
                if(bank_low == 0) bank_low = 1;
                update_rom_bank();
            } else {
                ram_enabled = (value & 0x0F) == 0x0A;
            }
        }
        return;

    case MBCType::MBC3:
        if(addr < 0x2000) {
            ram_enabled = (value & 0x0F) == 0x0A;
        } else if(addr < 0x4000) {
            bank_low = value & 0x7F;
            if(bank_low == 0) bank_low = 1;
            update_rom_bank();
        } else if(addr < 0x6000) {
            /* 0x00-0x03: RAM bank. 0x08-0x0C: RTC register (unsupported,
             * reads return 0xFF via ram_bank marker) */
            ram_bank = value;
        } else {
            /* RTC latch: writing 0x00 then 0x01 latches the clock */
            if(rtc_present) {
                if(value == 0x00) rtc.latch_armed = true;
                else if(value == 0x01 && rtc.latch_armed) rtc_latch();
                if(value != 0x00) rtc.latch_armed = false;
            }
        }
        return;

    case MBCType::MBC5:
        if(addr < 0x2000) {
            ram_enabled = (value & 0x0F) == 0x0A;
        } else if(addr < 0x3000) {
            bank_low = value;
            update_rom_bank();
        } else if(addr < 0x4000) {
            bank_high = value & 0x01;
            update_rom_bank();
        } else if(addr < 0x6000) {
            ram_bank = value & 0x0F;
        }
        return;

    default:
        return;
    }
}

auto Cartridge::read_ram(u16 addr) const -> u8 {
    if(!ram || !ram_enabled) return 0xFF;

    if(mbc == MBCType::MBC2) {
        /* 512 half-bytes, mirrored */
        return static_cast<u8>(ram[(addr - 0xA000) & 0x1FF] | 0xF0);
    }

    if(mbc == MBCType::MBC3 && ram_bank > 0x03) {
        if(rtc_present && ram_bank >= 0x08 && ram_bank <= 0x0C)
            return rtc_read((u8)(ram_bank - 0x08));
        return 0xFF;
    }

    u32 idx = static_cast<u32>(addr - 0xA000) + static_cast<u32>(ram_bank & 0x0F) * 0x2000;
    if(idx >= ram_size) idx %= ram_size;
    return ram[idx];
}

auto Cartridge::rtc_seconds() const -> u32 {
    if(rtc.halted || !rtc_now) return rtc.halt_value;
    return rtc_now() - rtc.base;
}

void Cartridge::rtc_latch() {
    u32 t = rtc_seconds();
    rtc.latched[0] = (u8)(t % 60);
    rtc.latched[1] = (u8)((t / 60) % 60);
    rtc.latched[2] = (u8)((t / 3600) % 24);
    u32 days = t / 86400;
    rtc.latched[3] = (u8)(days & 0xFF);
    rtc.latched[4] = (u8)(((days >> 8) & 1) | (rtc.halted ? 0x40 : 0) |
                          (days > 511 ? 0x80 : 0));
}

auto Cartridge::rtc_read(u8 reg) const -> u8 {
    static const u8 mask[5] = {0x3F, 0x3F, 0x1F, 0xFF, 0xC1};
    return (u8)(rtc.latched[reg] | (u8)~mask[reg]); /* unused bits read 1 */
}

void Cartridge::rtc_write(u8 reg, u8 value) {
    /* rebuild the counter from the current live values with one field
     * replaced, then re-derive base */
    u32 t = rtc_seconds();
    u32 s = t % 60, m = (t / 60) % 60, h = (t / 3600) % 24, d = t / 86400;
    switch(reg) {
    case 0: s = value % 60; break;
    case 1: m = value % 60; break;
    case 2: h = value % 24; break;
    case 3: d = (d & 0x100) | value; break;
    case 4:
        d = (d & 0xFF) | ((value & 1) << 8);
        rtc.halted = (value & 0x40) != 0;
        break;
    }
    u32 nt = ((d * 24 + h) * 60 + m) * 60 + s;
    if(rtc.halted) {
        rtc.halt_value = nt;
    } else if(rtc_now) {
        rtc.base = rtc_now() - nt;
    }
}

void Cartridge::write_ram(u16 addr, u8 value) {
    if(!ram || !ram_enabled) return;

    if(mbc == MBCType::MBC2) {
        ram[(addr - 0xA000) & 0x1FF] = value & 0x0F;
        ram_written = true;
        return;
    }

    if(mbc == MBCType::MBC3 && ram_bank > 0x03) {
        if(rtc_present && ram_bank >= 0x08 && ram_bank <= 0x0C)
            rtc_write((u8)(ram_bank - 0x08), value);
        return;
    }

    u32 idx = static_cast<u32>(addr - 0xA000) + static_cast<u32>(ram_bank & 0x0F) * 0x2000;
    if(idx >= ram_size) idx %= ram_size;
    ram[idx] = value;
    ram_written = true;
}

auto Cartridge::parse_mbc(u8 t) -> MBCType {
    switch(t) {
    case 0x00:
    case 0x08:
    case 0x09:
        return MBCType::None;
    case 0x01:
    case 0x02:
    case 0x03:
        return MBCType::MBC1;
    case 0x05:
    case 0x06:
        return MBCType::MBC2;
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
        return MBCType::MBC3;
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
        return MBCType::MBC5;
    default:
        return MBCType::Unsupported;
    }
}

auto Cartridge::has_battery(u8 t) -> bool {
    switch(t) {
    case 0x03: /* MBC1+RAM+BATTERY */
    case 0x06: /* MBC2+BATTERY */
    case 0x09: /* ROM+RAM+BATTERY */
    case 0x0F: /* MBC3+TIMER+BATTERY */
    case 0x10: /* MBC3+TIMER+RAM+BATTERY */
    case 0x13: /* MBC3+RAM+BATTERY */
    case 0x1B: /* MBC5+RAM+BATTERY */
    case 0x1E: /* MBC5+RUMBLE+RAM+BATTERY */
        return true;
    default:
        return false;
    }
}

auto Cartridge::rom_bank_count_from_header(u8 rom_size_byte) -> uint {
    /* 0x00 = 32KB (2 banks), each step doubles */
    if(rom_size_byte <= 0x08) return 2u << rom_size_byte;
    return 2;
}

auto Cartridge::ram_size_from_header(u8 ram_size_byte, MBCType mbc) -> u32 {
    if(mbc == MBCType::MBC2) return 512; /* built-in, not in header */
    switch(ram_size_byte) {
    case 0x00:
        return 0;
    case 0x01:
        return 0x800; /* 2 KB */
    case 0x02:
        return 0x2000; /* 8 KB */
    case 0x03:
        return 0x8000; /* 32 KB */
    case 0x04:
        return 0x20000; /* 128 KB */
    case 0x05:
        return 0x10000; /* 64 KB */
    default:
        return 0;
    }
}
