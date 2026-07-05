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

    void set_skip_render(bool skip) {
        video.skip_render = skip;
    }

    /* Optional display-line mask (see Video::row_mask). The array must stay
     * valid for the lifetime of the emulator. */
    void set_row_mask(const u8* mask) {
        video.set_row_mask(mask);
    }
    auto get_framebuffer() const -> const FrameBuffer& {
        return video.get_framebuffer();
    }

    auto get_cartridge_ram() -> u8* {
        return cartridge.get_ram();
    }
    auto get_cartridge_ram_size() const -> u32 {
        return cartridge.get_ram_size();
    }

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

    static void vblank_trampoline(void* ctx);
    volatile bool frame_done = false;

    void (*user_frame_cb)(void*) = nullptr;
    void* user_frame_ctx = nullptr;
};
