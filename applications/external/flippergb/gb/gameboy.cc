#include "gameboy.h"

Gameboy::Gameboy(
    const u8* bank0,
    uint rom_bank_count,
    MBCType mbc,
    u8* cart_ram,
    u32 cart_ram_size,
    RomBankProvider provider,
    void* provider_ctx)
    : cpu(*this)
    , video(*this)
    , mmu(*this)
    , timer(*this) {
    cartridge.init(bank0, rom_bank_count, mbc, cart_ram, cart_ram_size, provider, provider_ctx);

    video.register_vblank_callback(&Gameboy::vblank_trampoline, this);

    /* The DMG boot ROM is not shipped with this emulator. The CPU and IO
     * registers are initialised directly to the well-documented post-boot
     * state instead. */
    cpu.init_post_boot();

    mmu.write(0xFF26, 0x80); /* NR52: APU powered on (post-boot state) */
    mmu.write(0xFF25, 0xF3); /* NR51: all channels routed */
    mmu.write(0xFF24, 0x77); /* NR50: max master volume */
    mmu.write(0xFF40, 0x91); /* LCDC */
    mmu.write(0xFF42, 0x00); /* SCY */
    mmu.write(0xFF43, 0x00); /* SCX */
    mmu.write(0xFF45, 0x00); /* LYC */
    mmu.write(0xFF47, 0xFC); /* BGP */
    mmu.write(0xFF48, 0xFF); /* OBP0 */
    mmu.write(0xFF49, 0xFF); /* OBP1 */
    mmu.write(0xFF4A, 0x00); /* WY */
    mmu.write(0xFF4B, 0x00); /* WX */
}

void Gameboy::vblank_trampoline(void* ctx) {
    Gameboy* self = static_cast<Gameboy*>(ctx);
    if(self->user_frame_cb) self->user_frame_cb(self->user_frame_ctx);
    self->frame_done = true;
}

void Gameboy::button_pressed(GbButton button) {
    input.button_pressed(button);
    /* Request the joypad interrupt (missing upstream); mainly wakes
     * games waiting in HALT/STOP for input. */
    cpu.interrupt_flag.set_bit_to(4, true);
}

void Gameboy::button_released(GbButton button) {
    input.button_released(button);
}

void Gameboy::run_to_vblank() {
    frame_done = false;
    while(!frame_done) {
        tick();
    }
}

void Gameboy::tick() {
    auto cycles = cpu.tick();
    video.tick(cycles);
    timer.tick(cycles.cycles);
    apu.tick(cycles.cycles);
}
