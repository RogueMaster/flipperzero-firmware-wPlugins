/* Host test harness: runs a ROM headless on the PC and captures the serial
 * output (Blargg's test ROMs print their results through the serial port).
 * Usage: hosttest <rom.gb> [max_frames] [--dump-frame N]
 */

#include "../lib/gbcore/gameboy.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static u8* g_rom = nullptr;
static long g_rom_size = 0;

static char g_serial[4096];
static unsigned g_serial_len = 0;

extern "C" void gb_fatal(const char* msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(2);
}

static void serial_hook(u8 byte) {
    if(g_serial_len < sizeof(g_serial) - 1) {
        g_serial[g_serial_len++] = static_cast<char>(byte);
        g_serial[g_serial_len] = 0;
        fputc(byte, stdout);
        fflush(stdout);
    }
}

static const u8* bank_provider(void* /*ctx*/, uint unit) {
    long offset = static_cast<long>(unit) * 0x1000; /* 4 KB ROM units */
    if(offset + 0x1000 > g_rom_size) offset = 0;
    return g_rom + offset;
}

static FrameBuffer g_last_frame;

static void frame_hook(void* ctx) {
    g_last_frame = static_cast<Gameboy*>(ctx)->get_framebuffer();
}

/* Same dominant-voice heuristic the Flipper frontend uses for the piezo */
static bool pick_voice(Gameboy* gb, ApuVoice* out, int* out_ch) {
    ApuVoice v;
    bool have = false;
    for(uint n = 0; n < 2; n++) {
        gb->apu.get_voice(n, &v);
        if(!v.active) continue;
        if(!have || v.volume > out->volume || (v.volume == out->volume && v.order > out->order)) {
            *out = v;
            *out_ch = (int)n;
            have = true;
        }
    }
    if(!have) {
        gb->apu.get_voice(2, &v);
        if(v.active) {
            *out = v;
            *out_ch = 2;
            have = true;
        }
    }
    if(!have) {
        gb->apu.get_voice(3, &v);
        if(v.active) {
            *out = v;
            *out_ch = 3;
            have = true;
        }
    }
    return have;
}

static void dump_frame_ascii(const FrameBuffer& fb) {
    const char* shades = " .*#";
    /* downsample x2 for terminal readability */
    for(uint y = 0; y < GAMEBOY_HEIGHT; y += 2) {
        for(uint x = 0; x < GAMEBOY_WIDTH; x += 2) {
            printf("%c", shades[fb.get_pixel(x, y)]);
        }
        printf("\n");
    }
}

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(
            stderr,
            "usage: %s <rom.gb> [max_frames] [--dump-frame] [--rowmask] [--dump-audio]\n",
            argv[0]);
        return 1;
    }

    long max_frames = argc > 2 ? atol(argv[2]) : 2000;
    bool dump = false;
    bool rowmask = false;
    bool dump_audio = false;
    bool skiprender = false;
    /* input script: --press <frame>:<BTN>:<hold_frames>, repeatable */
    struct ScriptPress {
        long frame;
        GbButton btn;
        long hold;
    };
    ScriptPress script[32];
    int nscript = 0;
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--dump-frame")) dump = true;
        if(!strcmp(argv[i], "--rowmask")) rowmask = true;
        if(!strcmp(argv[i], "--dump-audio")) dump_audio = true;
        if(!strcmp(argv[i], "--skiprender")) skiprender = true;
        if(!strcmp(argv[i], "--press") && i + 1 < argc && nscript < 32) {
            char name[16];
            long fr, hold;
            if(sscanf(argv[++i], "%ld:%15[^:]:%ld", &fr, name, &hold) == 3) {
                GbButton b = GbButton::A;
                if(!strcmp(name, "A"))
                    b = GbButton::A;
                else if(!strcmp(name, "B"))
                    b = GbButton::B;
                else if(!strcmp(name, "START"))
                    b = GbButton::Start;
                else if(!strcmp(name, "SELECT"))
                    b = GbButton::Select;
                else if(!strcmp(name, "UP"))
                    b = GbButton::Up;
                else if(!strcmp(name, "DOWN"))
                    b = GbButton::Down;
                else if(!strcmp(name, "LEFT"))
                    b = GbButton::Left;
                else if(!strcmp(name, "RIGHT"))
                    b = GbButton::Right;
                script[nscript++] = {fr, b, hold};
            }
        }
    }

    /* Same 144 -> 64 line subsampling the Flipper frontend uses */
    static u8 mask[GAMEBOY_HEIGHT];
    for(uint y = 0; y < 64; y++)
        mask[(y * GAMEBOY_HEIGHT) / 64] = 1;

    FILE* f = fopen(argv[1], "rb");
    if(!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    g_rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_rom = static_cast<u8*>(malloc(static_cast<size_t>(g_rom_size)));
    if(fread(g_rom, 1, static_cast<size_t>(g_rom_size), f) != static_cast<size_t>(g_rom_size)) {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(f);

    gb_serial_hook = serial_hook;

    MBCType mbc = Cartridge::parse_mbc(g_rom[0x147]);
    if(mbc == MBCType::Unsupported) {
        fprintf(stderr, "unsupported mapper 0x%02X\n", g_rom[0x147]);
        return 1;
    }
    uint banks = Cartridge::rom_bank_count_from_header(g_rom[0x148]);
    u32 ram_size = Cartridge::ram_size_from_header(g_rom[0x149], mbc);
    u8* cart_ram = ram_size ? static_cast<u8*>(calloc(1, ram_size)) : nullptr;

    fprintf(
        stderr,
        "rom: %ld bytes, mapper=%d, banks=%u, cart_ram=%u\n",
        g_rom_size,
        static_cast<int>(mbc),
        banks,
        ram_size);

    auto* gb = new Gameboy(g_rom, banks, mbc, cart_ram, ram_size, bank_provider, nullptr);
    gb->set_frame_callback(frame_hook, gb);
    if(rowmask) gb->set_row_mask(mask);
    if(skiprender) gb->set_skip_render(true);

    u32 last_freq = 0;
    int last_ch = -1;
    u8 last_vol = 0;

    for(long frame = 0; frame < max_frames; frame++) {
        for(int i = 0; i < nscript; i++) {
            if(script[i].frame == frame) gb->button_pressed(script[i].btn);
            if(script[i].frame + script[i].hold == frame) gb->button_released(script[i].btn);
        }
        gb->run_to_vblank();

        if(dump_audio) {
            ApuVoice v;
            int ch = -1;
            bool have = pick_voice(gb, &v, &ch);
            u32 f = have ? v.freq_hz : 0;
            u8 vol = have ? v.volume : 0;
            if(f != last_freq || ch != last_ch || vol != last_vol) {
                if(have)
                    printf("f=%05ld ch%d %5u Hz vol=%2u\n", frame, ch, f, vol);
                else
                    printf("f=%05ld silence\n", frame);
                last_freq = f;
                last_ch = ch;
                last_vol = vol;
            }
        }

        if(strstr(g_serial, "Passed") || strstr(g_serial, "Failed")) {
            /* let it print the tail */
            for(int i = 0; i < 30; i++)
                gb->run_to_vblank();
            break;
        }
    }

    if(dump) dump_frame_ascii(g_last_frame);

    printf("\n");
    if(strstr(g_serial, "Passed")) return 0;
    if(strstr(g_serial, "Failed")) return 3;
    return 4; /* no verdict */
}
