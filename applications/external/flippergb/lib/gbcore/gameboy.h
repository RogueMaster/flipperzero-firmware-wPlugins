#pragma once

#include "input.h"
#include "cpu.h"
#include "video.h"
#include "timer.h"
#include "mmu.h"
#include "cartridge.h"
#include "apu.h"

class Gameboy {
public:
    /* bank0: pointer to the first 16 KB of ROM (stays resident).
     * provider: returns pointers to 16 KB switchable banks. */
    Gameboy(
        const u8* bank0,
        uint rom_bank_count,
        MBCType mbc,
        u8* cart_ram,
        u32 cart_ram_size,
        RomBankProvider provider,
        void* provider_ctx);

    /* Runs the emulator until the next vblank (one full frame). */
    void run_to_vblank();

    /* Called on every vblank BEFORE the framebuffer is cleared for the next
     * frame: this is where the frontend must convert/copy the image. */
    void set_frame_callback(void (*cb)(void*), void* ctx) {
        user_frame_cb = cb;
        user_frame_ctx = ctx;
    }

    void button_pressed(GbButton button);
    void button_released(GbButton button);

    void set_skip_render(bool skip) { video.skip_render = skip; }

    /* Flush pending peripheral cycles. Called automatically every
     * PERIPH_BATCH M-cycles and before any IO register read/write so
     * LY/STAT/DIV/TIMA/IF are always fresh where the game can see them. */
    void sync_peripherals() {
        uint n = pending_cycles;
        if(n) {
            pending_cycles = 0;
            video.tick(Cycles(n));
            timer.tick(n);
            apu.tick(n);
            mmu.serial_tick(n);
        }
    }

    /* Optional display-line mask (see Video::row_mask). The array must stay
     * valid for the lifetime of the emulator. */
    void set_row_mask(const u8* mask) { video.set_row_mask(mask); }
    auto get_framebuffer() const -> const FrameBuffer& { return video.get_framebuffer(); }

    auto get_cartridge_ram() -> u8* { return cartridge.get_ram(); }
    auto get_cartridge_ram_size() const -> u32 { return cartridge.get_ram_size(); }

    Cartridge cartridge;

    CPU cpu;
    friend class CPU;

    Video video;
    friend class Video;

    MMU mmu;
    friend class MMU;

    Timer timer;
    friend class Timer;

    Apu apu;

    Input input;

private:
    void tick();

    /* Peripheral catch-up batching (the single biggest hot-loop win on the
     * Cortex-M4): instead of stepping the PPU/timer/APU state machines
     * after every CPU instruction, cycles accumulate and are flushed every
     * PERIPH_BATCH M-cycles -- or immediately whenever the CPU touches an
     * IO register. Interrupt delivery is delayed by at most PERIPH_BATCH
     * M-cycles (128 T-cycles), far below what games can observe, while IO
     * reads always see exact values thanks to the sync points. */
    static const uint PERIPH_BATCH = 32;
    uint pending_cycles = 0;

    static void vblank_trampoline(void* ctx);
    volatile bool frame_done = false;

    void (*user_frame_cb)(void*) = nullptr;
    void* user_frame_ctx = nullptr;
};

/* ==================== hot-path inline implementations ====================
 *
 * Everything below runs once (or more) per emulated instruction. These
 * bodies need the complete Gameboy definition, and they are defined here
 * -- instead of in their .cc files -- so the core can inline them without
 * LTO. Before this, each emulated instruction paid 6-8 cross-TU function
 * calls (opcode fetch through the MMU, PPU/timer/APU ticks), which
 * dominated the frame time on the Cortex-M4.
 *
 * Deliberately NOT inlined: the full MMU read/write dispatch. Inlining it
 * into the ~300 opcode call sites costs ~15 KB of code (= RAM on the
 * Flipper, where the binary loads into the same heap as the ROM cache).
 * Only the instruction-fetch fast path (PC in ROM, true for virtually
 * every instruction) is inlined below. */

inline void Video::tick(Cycles cycles) {
    cycle_counter += cycles.cycles;

    /* Fast path: no mode boundary crossed. Boundary work (interrupts,
     * scanline rendering) is out-of-line in video.cc. The counter wrap
     * uses subtraction instead of the old modulo: per-instruction cycle
     * increments (<= 6) can never overshoot a whole extra period. */
    switch(current_mode) {
    case VideoMode::ACCESS_OAM:
        if(cycle_counter >= CLOCKS_PER_SCANLINE_OAM) {
            cycle_counter -= CLOCKS_PER_SCANLINE_OAM;
            lcd_status.set_bit_to(1, true);
            lcd_status.set_bit_to(0, true);
            current_mode = VideoMode::ACCESS_VRAM;
        }
        break;
    case VideoMode::ACCESS_VRAM:
        if(cycle_counter >= CLOCKS_PER_SCANLINE_VRAM) {
            cycle_counter -= CLOCKS_PER_SCANLINE_VRAM;
            mode_transition_vram_end();
        }
        break;
    case VideoMode::HBLANK:
        if(cycle_counter >= CLOCKS_PER_HBLANK) {
            cycle_counter -= CLOCKS_PER_HBLANK;
            mode_transition_hblank_end();
        }
        break;
    case VideoMode::VBLANK:
        if(cycle_counter >= CLOCKS_PER_SCANLINE) {
            cycle_counter -= CLOCKS_PER_SCANLINE;
            mode_transition_vblank_line();
        }
        break;
    }
}

inline void Timer::tick(uint cycles) {
    /* DIV increments at 16384 Hz = every 64 M-cycles */
    div_clocks += cycles;
    if(div_clocks >= 64) {
        divider.set(static_cast<u8>(divider.value() + (div_clocks >> 6)));
        div_clocks &= 63;
    }

    /* Accumulate T-cycles only while the timer is enabled (accumulating
     * with TAC off built an unbounded backlog that drained as a burst of
     * spurious overflows when the timer was enabled). The sub-period
     * remainder is kept across TAC toggles, matching hardware phase
     * behaviour more closely. */
    if(!timer_control.check_bit(2)) return;
    clocks += cycles * 4; /* M-cycles -> T-cycles */

    uint clock_limit = clocks_needed_to_increment();
    /* a long instruction can cross more than one period of the fastest
     * (16 T-cycle) timer rate; the old modulo silently dropped those */
    while(clocks >= clock_limit) {
        clocks -= clock_limit;

        u8 old_timer_counter = timer_counter.value();
        timer_counter.increment();

        if(timer_counter.value() < old_timer_counter) {
            gb.cpu.interrupt_flag.set_bit_to(2, true);
            timer_counter.set(timer_modulo.value());
        }
    }
}

inline void MMU::serial_tick(uint cycles) {
    if(!serial_clocks) return;
    if(serial_clocks > cycles) {
        serial_clocks -= cycles;
        return;
    }
    /* transfer complete: no link partner, the line reads all 1s */
    serial_clocks = 0;
    serial_data = 0xFF;
    serial_control &= 0x7F; /* clear the busy bit */
    gb.cpu.interrupt_flag.set_bit_to(3, true);
}

inline auto CPU::get_byte_from_pc() -> u8 {
    u16 a = pc.value();
    pc.increment();
    /* PC sits in cartridge ROM for virtually every instruction: read the
     * mapped page directly and skip the full MMU dispatch */
    if(a < 0x8000) return gb.cartridge.read(a);
    return gb.mmu.read(Address(a));
}

inline auto CPU::get_signed_byte_from_pc() -> s8 {
    return static_cast<s8>(get_byte_from_pc());
}

inline auto CPU::get_word_from_pc() -> u16 {
    u8 low_byte = get_byte_from_pc();
    u8 high_byte = get_byte_from_pc();
    return static_cast<u16>((high_byte << 8) | low_byte);
}

inline void Gameboy::tick() {
    pending_cycles += cpu.tick().cycles;
    if(pending_cycles >= PERIPH_BATCH) sync_peripherals();
}
