#include "mmu.h"
#include "gameboy.h"
#include "input.h"
#include "timer.h"
#include "cpu.h"
#include "video.h"

void (*gb_serial_hook)(u8 byte) = nullptr;

MMU::MMU(Gameboy& inGb)
    : gb(inGb) {
}

auto MMU::read(const Address& address) const -> u8 {
    u16 a = address.value();

    /* Cartridge ROM (boot ROM is skipped: CPU starts with post-boot state) */
    if(a < 0x8000) return gb.cartridge.read(a);

    /* VRAM */
    if(a < 0xA000) return gb.video.vram_read(static_cast<u16>(a - 0x8000));

    /* External (cartridge) RAM */
    if(a < 0xC000) return gb.cartridge.read(a);

    /* Internal work RAM */
    if(a < 0xE000) return work_ram[a - 0xC000];

    /* Echo RAM */
    if(a < 0xFE00) return work_ram[a - 0xE000];

    /* OAM */
    if(a < 0xFEA0) return oam_ram[a - 0xFE00];

    /* Unusable region */
    if(a < 0xFF00) return 0xFF;

    /* Mapped IO */
    if(a < 0xFF80) return read_io(address);

    /* Zero page RAM */
    if(a < 0xFFFF) return high_ram[a - 0xFF80];

    /* Interrupt enable register */
    return gb.cpu.interrupt_enabled.value();
}

void MMU::write(const Address& address, u8 byte) {
    u16 a = address.value();

    if(a < 0x8000) {
        gb.cartridge.write(a, byte);
        return;
    }

    if(a < 0xA000) {
        gb.video.vram_write(static_cast<u16>(a - 0x8000), byte);
        return;
    }

    if(a < 0xC000) {
        gb.cartridge.write(a, byte);
        return;
    }

    if(a < 0xE000) {
        work_ram[a - 0xC000] = byte;
        return;
    }

    if(a < 0xFE00) {
        work_ram[a - 0xE000] = byte;
        return;
    }

    if(a < 0xFEA0) {
        oam_ram[a - 0xFE00] = byte;
        return;
    }

    if(a < 0xFF00) return; /* unusable */

    if(a < 0xFF80) {
        write_io(address, byte);
        return;
    }

    if(a < 0xFFFF) {
        high_ram[a - 0xFF80] = byte;
        return;
    }

    gb.cpu.interrupt_enabled.set(byte);
}

auto MMU::read_io(const Address& address) const -> u8 {
    u16 a = address.value();

    /* Sound registers + wave RAM */
    if(a >= 0xFF10 && a <= 0xFF3F) return gb.apu.read(a);

    switch(address.value()) {
    case 0xFF00:
        return gb.input.get_input();

    case 0xFF01:
        return serial_data;

    case 0xFF02:
        return 0xFF;

    case 0xFF04:
        return gb.timer.get_divider();

    case 0xFF05:
        return gb.timer.get_timer();

    case 0xFF06:
        return gb.timer.get_timer_modulo();

    case 0xFF07:
        return gb.timer.get_timer_control();

    case 0xFF0F:
        return gb.cpu.interrupt_flag.value();

    case 0xFF40:
        return gb.video.control_byte;

    case 0xFF41:
        return gb.video.lcd_status.value();

    case 0xFF42:
        return gb.video.scroll_y.value();

    case 0xFF43:
        return gb.video.scroll_x.value();

    case 0xFF44:
        return gb.video.line.value();

    case 0xFF45:
        return gb.video.ly_compare.value();

    case 0xFF47:
        return gb.video.bg_palette.value();

    case 0xFF48:
        return gb.video.sprite_palette_0.value();

    case 0xFF49:
        return gb.video.sprite_palette_1.value();

    case 0xFF4A:
        return gb.video.window_y.value();

    case 0xFF4B:
        return gb.video.window_x.value();

    case 0xFF4D: /* CGB speed switch: report normal speed */
        return 0x00;

    default:
        /* Audio registers, CGB registers and unmapped IO */
        return 0xFF;
    }
}

void MMU::write_io(const Address& address, u8 byte) {
    u16 a = address.value();

    /* Sound registers + wave RAM */
    if(a >= 0xFF10 && a <= 0xFF3F) {
        gb.apu.write(a, byte);
        return;
    }

    switch(address.value()) {
    case 0xFF00:
        gb.input.write(byte);
        return;

    case 0xFF01:
        serial_data = byte;
        return;

    case 0xFF02:
        /* Serial control: transfer start with internal clock -> deliver
         * the byte immediately (enough for link-less games and for the
         * Blargg test ROMs which print through the serial port) */
        if((byte & 0x81) == 0x81 && gb_serial_hook) gb_serial_hook(serial_data);
        return;

    case 0xFF04:
        gb.timer.reset_divider();
        return;

    case 0xFF05:
        gb.timer.set_timer(byte);
        return;

    case 0xFF06:
        gb.timer.set_timer_modulo(byte);
        return;

    case 0xFF07:
        gb.timer.set_timer_control(byte);
        return;

    case 0xFF0F:
        gb.cpu.interrupt_flag.set(byte);
        return;

    case 0xFF40:
        gb.video.control_byte = byte;
        return;

    case 0xFF41:
        gb.video.lcd_status.set(byte);
        return;

    case 0xFF42:
        gb.video.scroll_y.set(byte);
        return;

    case 0xFF43:
        gb.video.scroll_x.set(byte);
        return;

    case 0xFF44:
        gb.video.line.set(0x0);
        return;

    case 0xFF45:
        gb.video.ly_compare.set(byte);
        return;

    case 0xFF46:
        dma_transfer(byte);
        return;

    case 0xFF47:
        gb.video.bg_palette.set(byte);
        return;

    case 0xFF48:
        gb.video.sprite_palette_0.set(byte);
        return;

    case 0xFF49:
        gb.video.sprite_palette_1.set(byte);
        return;

    case 0xFF4A:
        gb.video.window_y.set(byte);
        return;

    case 0xFF4B:
        gb.video.window_x.set(byte);
        return;

    default:
        /* Audio registers, CGB registers and unmapped IO: ignored */
        return;
    }
}

void MMU::dma_transfer(u8 byte) {
    u16 start_address = static_cast<u16>(byte) * 0x100;

    for(u8 i = 0x0; i <= 0x9F; i++) {
        oam_ram[i] = read(static_cast<u16>(start_address + i));
    }
}
