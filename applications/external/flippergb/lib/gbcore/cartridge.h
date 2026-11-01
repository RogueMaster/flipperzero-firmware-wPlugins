#pragma once

#include "definitions.h"

/* Cartridge with pluggable ROM page provider.
 *
 * Instead of holding the whole ROM in RAM (impossible on Flipper Zero for
 * anything above 32 KB), the cartridge asks the platform for pointers to
 * 8 KB ROM pages (page n = ROM offset n * 0x2000) whenever the game
 * switches banks. 8 KB granularity -- half a MBC bank -- doubles how many
 * cache slots fit in the same RAM and halves the SD stall of a cache miss,
 * which matters a lot for bank-switch heavy games (Pokemon switches banks
 * for music/code/data every frame). On the desktop test build the provider
 * just returns `rom + page * 0x2000`; on the Flipper it is backed by an
 * LRU cache streaming from the SD card.
 *
 * Supported mappers: ROM only, MBC1 (incl. upper bits / mode select),
 * MBC2 (built-in 512x4 RAM), MBC3 (no RTC), MBC5.
 */

/* The provider hands out 4 KB ROM units (unit n = ROM offset n * 0x1000)
 * and guarantees the FOUR most recently returned units stay valid. */
using RomBankProvider = const u8* (*)(void* ctx, uint unit);

enum class MBCType : u8 {
    None,
    MBC1,
    MBC1M, /* MBC1 multicart wiring (bank_high shifts by 4, not 5) */
    MBC2,
    MBC3,
    MBC5,
    Unsupported,
};

/* Wall-clock hook for the MBC3 RTC: returns seconds since an arbitrary
 * epoch. Host build: time(). Flipper: furi_hal_rtc timestamp. */
using RtcNowProvider = u32 (*)(void);

struct Mbc3Rtc {
    /* live counters derived from now() - base, plus the latched copy */
    u32 base = 0; /* now() when the RTC was at 0s (adjusted on writes) */
    bool halted = false;
    u32 halt_value = 0; /* frozen seconds while halted */
    u8 latched[5] = {}; /* S, M, H, DL, DH */
    bool latch_armed = false;
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
        if(addr < 0x8000) {
            /* The switchable bank is four LAZILY mapped 4 KB quarters:
             * nothing is fetched at bank-switch time. Measured on Pokemon
             * Red: the game rewrites the bank register ~200x per frame
             * (trampolines/ISR save-restore) while actually READING from
             * only ~17 units -- eager mapping caused ~86 phantom cache
             * misses per frame (the whole 100 ms lag). */
            uint q = (uint)(addr - 0x4000) >> 12;
            if(!bankN_q[q]) fetch_quarter(q);
            return bankN_q[q][addr & 0x0FFF];
        }
        /* 0xA000 - 0xBFFF: cartridge RAM */
        return read_ram(addr);
    }

    void write(u16 addr, u8 value);

    auto get_ram() -> u8* { return ram; }
    auto get_ram_size() const -> u32 { return ram_size; }

    /* battery-RAM dirty tracking (frontend autosave) */
    auto ram_dirty() const -> bool { return ram_written; }
    void clear_ram_dirty() { ram_written = false; }

    /* MBC3 RTC */
    void set_rtc_provider(RtcNowProvider p) { rtc_now = p; }
    auto has_rtc() const -> bool { return rtc_present; }
    void set_rtc_present(bool p) { rtc_present = p; }
    auto rtc_state() -> Mbc3Rtc& { return rtc; }

    /* save-state accessors */
    struct BankState {
        u8 bank_low, bank_high, ram_bank;
        bool ram_enabled, advanced_mode;
    };
    void export_banks(BankState* st) const {
        st->bank_low = (u8)bank_low;
        st->bank_high = (u8)bank_high;
        st->ram_bank = (u8)ram_bank;
        st->ram_enabled = ram_enabled;
        st->advanced_mode = advanced_banking_mode;
    }
    void import_banks(const BankState* st) {
        bank_low = st->bank_low;
        bank_high = st->bank_high;
        ram_bank = st->ram_bank;
        ram_enabled = st->ram_enabled;
        advanced_banking_mode = st->advanced_mode;
        cur_bank = 0xFFFFFFFFu; /* force remap */
        update_rom_bank();
    }

    /* Header helpers (operate on the first bank) */
    static auto parse_mbc(u8 cartridge_type_byte) -> MBCType;
    static auto has_battery(u8 cartridge_type_byte) -> bool;
    static auto rom_bank_count_from_header(u8 rom_size_byte) -> uint;
    static auto ram_size_from_header(u8 ram_size_byte, MBCType mbc) -> u32;

private:
    auto read_ram(u16 addr) const -> u8;
    void write_ram(u16 addr, u8 value);
    void fetch_quarter(uint q) const;
    auto rtc_read(u8 reg) const -> u8;
    void rtc_write(u8 reg, u8 value);
    void rtc_latch();
    auto rtc_seconds() const -> u32;
    void update_rom_bank();

    const u8* bank0 = nullptr;
    mutable const u8* bankN_q[4] = {}; /* lazy 4 KB quarters of the bank */
    uint cur_bank = 0xFFFFFFFFu; /* currently selected bank (memo) */

    u8* ram = nullptr;
    u32 ram_size = 0;

    RomBankProvider provider = nullptr;
    void* provider_ctx = nullptr;

    MBCType mbc = MBCType::None;
    uint bank_count = 2;

    bool ram_enabled = false;
    bool ram_written = false;
    bool rtc_present = false;
    RtcNowProvider rtc_now = nullptr;
    Mbc3Rtc rtc;
    bool advanced_banking_mode = false; /* MBC1 mode 1 */
    uint bank_low = 1; /* MBC1: 5 bits, MBC3: 7 bits, MBC5: 8 bits */
    uint bank_high = 0; /* MBC1: 2 bits, MBC5: 9th bit */
    uint ram_bank = 0;
};
