/* FlipGB - Game Boy (DMG) emulator frontend for Flipper Zero.
 *
 * Frontend responsibilities:
 *  - ROM picker (system file browser), ROM header parsing
 *  - ROM bank streaming from SD with an LRU cache (whole ROM loaded to RAM
 *    when it fits)
 *  - 160x144 2bpp -> 128x64 1bpp ordered-dither downscale
 *  - input mapping (OK=A, Back=B, Up+Down together = emulator menu)
 *  - emulator menu: Start/Select injection, frameskip, save SRAM, exit
 *  - battery save (.sav) persistence next to the ROM
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <stdlib.h>
#include <string.h>
#include <new>

#include "gb/gameboy.h"

#define SCREEN_W    128
#define SCREEN_H    64
#define BUFFER_SIZE (SCREEN_W * SCREEN_H / 8)

#define GB_FRAME_US  16742 /* 59.73 Hz */
#define BANK_SIZE    0x4000u
/* Heap kept free for the GUI/system while playing. 10 KB is enough for the
 * direct-draw takeover + input subscription + background services; the old
 * 24 KB reserve was more than one whole ROM bank of wasted headroom and
 * pushed big cartridges (1 MB MBC3 + 32 KB SRAM) into "Not enough RAM". */
#define HEAP_RESERVE (10u * 1024u)
#define ALLOC_MARGIN 1024u /* never take the very last heap block */

/* On the Flipper malloc() does NOT return NULL on failure: it crashes the
 * whole firmware with "out of memory". Every allocation whose size depends
 * on the ROM must go through this wrapper, which checks the largest free
 * heap block first (total free heap is not enough: fragmentation can make
 * a big contiguous allocation impossible even with plenty of free bytes). */
static void* safe_malloc(size_t size) {
    if(memmgr_heap_get_max_free_block() < size + ALLOC_MARGIN) return NULL;
    return malloc(size);
}

/* ------------------------------------------------------------------ state */

/* GB button bits used between the input callback and the main loop */
enum {
    KBIT_UP = 1 << 0,
    KBIT_DOWN = 1 << 1,
    KBIT_LEFT = 1 << 2,
    KBIT_RIGHT = 1 << 3,
    KBIT_A = 1 << 4,
    KBIT_B = 1 << 5,
};

/* ROM bank cache.
 *
 * Every switchable bank lives in its own 16 KB heap block: there is no
 * "whole ROM in one contiguous malloc" fast path any more, because a large
 * contiguous allocation is exactly what fails (and used to crash the
 * firmware) on a fragmented heap. Instead:
 *
 *  - as many 16 KB slots as the heap safely affords are allocated up front;
 *  - if every switchable bank got a slot, the ROM is fully resident: bank
 *    lookup is a direct O(1) index and the SD file is closed;
 *  - otherwise the slots form an LRU cache streaming banks from SD.
 */
typedef struct {
    File* file; /* NULL once the ROM is fully resident */
    u8* bank0;
    u8** slots; /* num_slots pointers to 16 KB blocks */
    u16* slot_bank; /* which bank each slot holds (0 = empty) */
    u32* slot_use; /* LRU stamps (streaming mode only) */
    u32 use_counter;
    u16 num_slots;
    u16 banks;
    bool fully_loaded; /* slots[i] permanently holds bank i+1 */
} RomCache;

typedef struct {
    uint8_t screen[BUFFER_SIZE]; /* 1bpp page-format, bit set = light */

    Gui* gui;
    Canvas* canvas;
    FuriMutex* fb_mutex;

    volatile uint8_t keys; /* KBIT_* currently pressed */
    volatile bool menu_requested;
    volatile bool menu_active;
    volatile bool exit_requested;

    Gameboy* gb;
    RomCache rom;
    u8* cart_ram;
    u32 cart_ram_size;
    bool has_battery;

    /* sound (single-tone piezo fed with the dominant APU voice) */
    bool sound_enabled;
    bool speaker_acquired;
    uint32_t tone_freq; /* currently playing tone, 0 = silent */
    uint8_t tone_vol; /* 0..15, scaled by master volume */

    /* menu */
    int menu_cursor;
    uint8_t menu_last_keys;
    int frameskip_setting; /* -1 = auto, 0..4 fixed */
    int inject_start_frames;
    int inject_select_frames;
    char rom_title[17];
    char status_msg[32];

    /* pacing */
    uint32_t emu_ms_ema; /* x16 fixed point */
    int auto_skip;
    int skip_phase;
} AppState;

static AppState* g_app = NULL;

static volatile uint32_t s_input_cb_inflight = 0;
static volatile uint32_t s_fb_cb_inflight = 0;

static inline void wait_inflight_zero(volatile uint32_t* counter) {
    while(__atomic_load_n(counter, __ATOMIC_ACQUIRE) != 0) {
        furi_delay_ms(1);
    }
}

extern "C" [[noreturn]] void gb_fatal(const char* msg) {
    furi_crash(msg);
}

/* ------------------------------------------------------- rom bank provider */

static const u8* rom_bank_provider(void* ctx, uint bank) {
    RomCache* rc = (RomCache*)ctx;

    if(bank == 0) return rc->bank0;

    if(rc->fully_loaded) {
        return rc->slots[bank - 1]; /* O(1), the whole ROM is resident */
    }

    /* cache lookup */
    u16 lru = 0;
    u32 lru_use = 0xFFFFFFFFu;
    for(u16 i = 0; i < rc->num_slots; i++) {
        if(rc->slot_bank[i] == bank) {
            rc->slot_use[i] = ++rc->use_counter;
            return rc->slots[i];
        }
        if(rc->slot_use[i] < lru_use) {
            lru_use = rc->slot_use[i];
            lru = i;
        }
    }

    /* miss: stream the bank from SD into the LRU slot */
    storage_file_seek(rc->file, (u32)bank * BANK_SIZE, true);
    size_t got = storage_file_read(rc->file, rc->slots[lru], BANK_SIZE);
    if(got < BANK_SIZE) {
        /* short read (SD hiccup / truncated ROM): open-bus instead of
         * stale data from whatever bank lived here before */
        memset(rc->slots[lru] + got, 0xFF, BANK_SIZE - got);
    }
    rc->slot_bank[lru] = (u16)bank;
    rc->slot_use[lru] = ++rc->use_counter;
    return rc->slots[lru];
}

/* -------------------------------------------------------------- rendering */

/* Destination -> source coordinate maps (128 <- 160, 64 <- 144) */
static uint8_t s_xmap[SCREEN_W];
static uint8_t s_ymap[SCREEN_H];

/* Scanlines that actually survive the 144 -> 64 downscale. Handed to the
 * emulator core so the PPU skips the other 80 lines entirely (~55% less
 * rendering work per frame). */
static u8 s_rowmask[GAMEBOY_HEIGHT];

/* light/dark decision per (y-parity, x-parity, shade): 2x2 ordered dither.
 * white -> lit, light gray -> 3/4 lit, dark gray -> 1/4 lit, black -> off */
static uint8_t s_dither[2][2][4];

static void init_scale_maps(void) {
    for(int x = 0; x < SCREEN_W; x++)
        s_xmap[x] = (uint8_t)((x * (int)GAMEBOY_WIDTH) / SCREEN_W);
    for(int y = 0; y < SCREEN_H; y++)
        s_ymap[y] = (uint8_t)((y * (int)GAMEBOY_HEIGHT) / SCREEN_H);

    memset(s_rowmask, 0, sizeof(s_rowmask));
    for(int y = 0; y < SCREEN_H; y++)
        s_rowmask[s_ymap[y]] = 1;

    for(int py = 0; py < 2; py++) {
        for(int px = 0; px < 2; px++) {
            s_dither[py][px][0] = 1;
            s_dither[py][px][1] = (px == 1 && py == 1) ? 0 : 1;
            s_dither[py][px][2] = (px == 0 && py == 0) ? 1 : 0;
            s_dither[py][px][3] = 0;
        }
    }
}

/* Called by the core on every vblank, before the framebuffer is cleared.
 * Converts 4 shades -> 1 bit with a 2x2 ordered dither. */
static void frame_callback(void* ctx) {
    AppState* app = (AppState*)ctx;
    const u8* raw = app->gb->get_framebuffer().raw(); /* packed 2bpp */

    furi_mutex_acquire(app->fb_mutex, FuriWaitForever);

    uint8_t* dst = app->screen;
    for(int y = 0; y < SCREEN_H; y++) {
        /* storage is compacted to the 64 displayed rows in ascending order
         * (GB_FB_ROWS=64 + row mask), so displayed row y == storage slot y */
        uint base = (uint)y * GAMEBOY_WIDTH;
        uint8_t bit = (uint8_t)(1u << (y & 7));
        uint8_t nbit = (uint8_t)~bit;
        uint8_t* row = dst + (y >> 3) * SCREEN_W;
        const uint8_t(*dither)[4] = s_dither[y & 1];

        for(int x = 0; x < SCREEN_W; x++) {
            uint i = base + s_xmap[x];
            uint s = (raw[i >> 2] >> ((i & 3) << 1)) & 0x3;
            if(dither[x & 1][s])
                row[x] |= bit;
            else
                row[x] &= nbit;
        }
    }

    furi_mutex_release(app->fb_mutex);
}

static void framebuffer_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    __atomic_fetch_add(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);

    AppState* app = (AppState*)context;
    (void)orientation;

    if(!app || !data || size < BUFFER_SIZE || app->menu_active) {
        /* in menu mode the canvas content (drawn with canvas_*) passes
         * through untouched */
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }

    if(furi_mutex_acquire(app->fb_mutex, 0) != FuriStatusOk) {
        __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
        return;
    }

    /* screen buffer: bit=1 means light; display buffer: bit=1 means dark */
    const uint8_t* src = app->screen;
    for(size_t i = 0; i < BUFFER_SIZE; i++) {
        data[i] = (uint8_t)(src[i] ^ 0xFF);
    }

    furi_mutex_release(app->fb_mutex);
    __atomic_fetch_sub(&s_fb_cb_inflight, 1, __ATOMIC_RELAXED);
}

/* ------------------------------------------------------------------ input */

static void input_events_callback(const void* value, void* ctx) {
    if(!value || !ctx) return;

    __atomic_fetch_add(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);

    AppState* app = (AppState*)ctx;
    const InputEvent* event = (const InputEvent*)value;

    uint8_t bit = 0;
    switch(event->key) {
    case InputKeyUp:
        bit = KBIT_UP;
        break;
    case InputKeyDown:
        bit = KBIT_DOWN;
        break;
    case InputKeyLeft:
        bit = KBIT_LEFT;
        break;
    case InputKeyRight:
        bit = KBIT_RIGHT;
        break;
    case InputKeyOk:
        bit = KBIT_A;
        break;
    case InputKeyBack:
        bit = KBIT_B;
        break;
    default:
        break;
    }

    if(bit) {
        if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
            uint8_t keys = (uint8_t)__atomic_or_fetch((uint8_t*)&app->keys, bit, __ATOMIC_RELAXED);
            /* Up+Down together: physically impossible on a real GB d-pad,
             * so it is our reserved menu gesture */
            if((keys & (KBIT_UP | KBIT_DOWN)) == (KBIT_UP | KBIT_DOWN)) {
                app->menu_requested = true;
            }
        } else if(event->type == InputTypeRelease) {
            (void)__atomic_fetch_and((uint8_t*)&app->keys, (uint8_t)~bit, __ATOMIC_RELAXED);
        }
    }

    __atomic_fetch_sub(&s_input_cb_inflight, 1, __ATOMIC_RELAXED);
}

/* Apply the current key snapshot to the emulated joypad (edge based) */
static void apply_input(AppState* app, uint8_t* last_applied) {
    uint8_t now = app->menu_active ? 0 : app->keys;
    uint8_t changed = (uint8_t)(now ^ *last_applied);
    if(!changed && !app->inject_start_frames && !app->inject_select_frames) return;

    struct {
        uint8_t bit;
        GbButton btn;
    } map[] = {
        {KBIT_UP, GbButton::Up},
        {KBIT_DOWN, GbButton::Down},
        {KBIT_LEFT, GbButton::Left},
        {KBIT_RIGHT, GbButton::Right},
        {KBIT_A, GbButton::A},
        {KBIT_B, GbButton::B},
    };

    for(auto& m : map) {
        if(!(changed & m.bit)) continue;
        if(now & m.bit)
            app->gb->button_pressed(m.btn);
        else
            app->gb->button_released(m.btn);
    }

    /* menu-injected Start/Select presses (held for a few frames) */
    if(app->inject_start_frames > 0) {
        app->inject_start_frames--;
        if(app->inject_start_frames == 0) app->gb->button_released(GbButton::Start);
    }
    if(app->inject_select_frames > 0) {
        app->inject_select_frames--;
        if(app->inject_select_frames == 0) app->gb->button_released(GbButton::Select);
    }

    *last_applied = now;
}

/* ------------------------------------------------------------------ sound */

/* The Flipper piezo plays one frequency at one volume at a time, so the
 * 4 APU channels are reduced to the "dominant voice":
 *   1. the louder of the two pulse channels (they carry the melody in
 *      almost every GB soundtrack); newer trigger wins ties (lead line)
 *   2. otherwise the wave channel (bass/secondary melody)
 *   3. otherwise noise (percussion), mapped to a short low buzz
 * Updated once per emulated frame (~60 Hz), like a tracker row. */
static void sound_update(AppState* app, bool force_silent) {
    if(!app->speaker_acquired) return;

    ApuVoice best = {false, 0, 0, 0};
    bool have = false;

    if(!force_silent && app->sound_enabled && !app->menu_active) {
        ApuVoice v;
        /* pulse 1 / pulse 2 */
        for(uint n = 0; n < 2; n++) {
            app->gb->apu.get_voice(n, &v);
            if(!v.active) continue;
            if(!have || v.volume > best.volume ||
               (v.volume == best.volume && v.order > best.order)) {
                best = v;
                have = true;
            }
        }
        /* wave */
        if(!have) {
            app->gb->apu.get_voice(2, &v);
            if(v.active) {
                best = v;
                have = true;
            }
        }
        /* noise: LFSR clock -> percussive buzz in the piezo's low range */
        if(!have) {
            app->gb->apu.get_voice(3, &v);
            if(v.active) {
                uint32_t f = v.freq_hz >> 5;
                if(f < 80) f = 80;
                if(f > 400) f = 400;
                v.freq_hz = f;
                best = v;
                have = true;
            }
        }
    }

    if(have) {
        uint32_t f = best.freq_hz;
        if(f < 40 || f > 12000) {
            have = false; /* outside anything the piezo can render */
        } else {
            uint8_t master = app->gb->apu.master_volume(); /* 0..7 */
            uint8_t vol = (uint8_t)((best.volume * (master + 1)) >> 3); /* 0..15 */
            if(vol == 0) {
                have = false;
            } else if(f != app->tone_freq || vol != app->tone_vol) {
                furi_hal_speaker_start((float)f, (float)vol / 15.0f);
                app->tone_freq = f;
                app->tone_vol = vol;
            }
        }
    }

    if(!have && app->tone_freq) {
        furi_hal_speaker_stop();
        app->tone_freq = 0;
        app->tone_vol = 0;
    }
}

/* ------------------------------------------------------------------- save */

static void save_path_for_rom(FuriString* rom_path, FuriString* out) {
    /* parentheses bypass the C11 _Generic macro (not usable from C++) */
    (furi_string_set)(out, rom_path);
    furi_string_cat_str(out, ".sav");
}

static bool save_sram(AppState* app, Storage* storage, FuriString* sav_path) {
    if(!app->cart_ram || !app->cart_ram_size || !app->has_battery) return false;

    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, furi_string_get_cstr(sav_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(f, app->cart_ram, app->cart_ram_size) == app->cart_ram_size;
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

static void load_sram(AppState* app, Storage* storage, FuriString* sav_path) {
    if(!app->cart_ram || !app->cart_ram_size) return;

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, furi_string_get_cstr(sav_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(f, app->cart_ram, app->cart_ram_size);
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* ------------------------------------------------------------------- menu */

#define MENU_ITEMS 7

static void menu_draw(AppState* app) {
    Canvas* c = app->canvas;
    canvas_reset(c);
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, app->rom_title[0] ? app->rom_title : "FlipGB");
    canvas_draw_line(c, 0, 12, 127, 12);

    static const char* labels[MENU_ITEMS] = {
        "Continue",
        "Press START",
        "Press SELECT",
        "Frameskip",
        "Sound",
        "Save SRAM",
        "Exit",
    };

    canvas_set_font(c, FontSecondary);

    for(int i = 0; i < MENU_ITEMS; i++) {
        /* 7 px pitch keeps all 7 items on the 64 px screen */
        int y = 20 + i * 7;
        if(i == app->menu_cursor) {
            canvas_draw_str(c, 2, y, ">");
        }
        canvas_draw_str(c, 10, y, labels[i]);

        if(i == 3) {
            char buf[12];
            if(app->frameskip_setting < 0)
                snprintf(buf, sizeof(buf), "auto(%d)", app->auto_skip);
            else
                snprintf(buf, sizeof(buf), "%d", app->frameskip_setting);
            canvas_draw_str(c, 72, y, buf);
        }
        if(i == 4) {
            canvas_draw_str(
                c, 72, y, !app->speaker_acquired ? "n/a" : (app->sound_enabled ? "on" : "off"));
        }
    }

    if(app->status_msg[0]) {
        canvas_draw_str(c, 70, 10, app->status_msg);
    } else {
        /* free heap indicator: helps spotting memory pressure on device */
        char rambuf[16];
        snprintf(rambuf, sizeof(rambuf), "%uk free", (unsigned)(memmgr_get_free_heap() / 1024u));
        canvas_draw_str_aligned(c, 126, 10, AlignRight, AlignBottom, rambuf);
    }

    canvas_commit(c);
}

/* returns true while the menu stays open */
static bool menu_tick(AppState* app, Storage* storage, FuriString* sav_path) {
    uint8_t keys = app->keys;
    uint8_t pressed = (uint8_t)(keys & ~app->menu_last_keys);
    app->menu_last_keys = keys;

    if(pressed & KBIT_UP) {
        app->menu_cursor = (app->menu_cursor + MENU_ITEMS - 1) % MENU_ITEMS;
        app->status_msg[0] = 0;
    }
    if(pressed & KBIT_DOWN) {
        app->menu_cursor = (app->menu_cursor + 1) % MENU_ITEMS;
        app->status_msg[0] = 0;
    }

    if(pressed & (KBIT_LEFT | KBIT_RIGHT)) {
        if(app->menu_cursor == 3) {
            /* cycle: auto, 0, 1, 2, 3, 4 */
            int v = app->frameskip_setting;
            if(pressed & KBIT_RIGHT)
                v = (v >= 4) ? -1 : v + 1;
            else
                v = (v <= -1) ? 4 : v - 1;
            app->frameskip_setting = v;
        }
        if(app->menu_cursor == 4 && app->speaker_acquired) {
            app->sound_enabled = !app->sound_enabled;
        }
    }

    if(pressed & KBIT_B) return false; /* Back closes the menu */

    if(pressed & KBIT_A) {
        switch(app->menu_cursor) {
        case 0:
            return false;
        case 1:
            app->gb->button_pressed(GbButton::Start);
            app->inject_start_frames = 8;
            return false;
        case 2:
            app->gb->button_pressed(GbButton::Select);
            app->inject_select_frames = 8;
            return false;
        case 3:
            break;
        case 4:
            if(app->speaker_acquired) app->sound_enabled = !app->sound_enabled;
            break;
        case 5:
            if(app->has_battery) {
                bool ok = save_sram(app, storage, sav_path);
                snprintf(app->status_msg, sizeof(app->status_msg), ok ? "saved!" : "error");
            } else {
                snprintf(app->status_msg, sizeof(app->status_msg), "no battery");
            }
            break;
        case 6:
            app->exit_requested = true;
            return false;
        }
    }

    menu_draw(app);
    return true;
}

/* -------------------------------------------------------------- rom setup */

typedef enum {
    RomLoadOk,
    RomLoadIoError,
    RomLoadCgbOnly,
    RomLoadBadMapper,
    RomLoadNoMem,
} RomLoadResult;

static RomLoadResult
    rom_load(AppState* app, Storage* storage, const char* path, MBCType* out_mbc) {
    RomCache* rc = &app->rom;

    rc->file = storage_file_alloc(storage);
    if(!storage_file_open(rc->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return RomLoadIoError;
    }

    /* bank 0 stays resident and contains the header */
    rc->bank0 = (u8*)safe_malloc(BANK_SIZE);
    if(!rc->bank0) return RomLoadNoMem;
    memset(rc->bank0, 0xFF, BANK_SIZE);
    if(storage_file_read(rc->file, rc->bank0, BANK_SIZE) < 0x150) {
        return RomLoadIoError;
    }

    /* header */
    u8 cgb_flag = rc->bank0[0x143];
    if(cgb_flag == 0xC0) return RomLoadCgbOnly;

    MBCType mbc = Cartridge::parse_mbc(rc->bank0[0x147]);
    if(mbc == MBCType::Unsupported) return RomLoadBadMapper;
    *out_mbc = mbc;

    memcpy(app->rom_title, &rc->bank0[0x134], 16);
    app->rom_title[16] = 0;
    for(int i = 0; i < 16; i++) {
        char ch = app->rom_title[i];
        if(ch != 0 && (ch < 0x20 || ch > 0x7E)) app->rom_title[i] = 0;
    }

    rc->banks = (u16)Cartridge::rom_bank_count_from_header(rc->bank0[0x148]);
    app->has_battery = Cartridge::has_battery(rc->bank0[0x147]);

    /* cartridge RAM */
    app->cart_ram_size = Cartridge::ram_size_from_header(rc->bank0[0x149], mbc);
    if(app->cart_ram_size) {
        app->cart_ram = (u8*)safe_malloc(app->cart_ram_size);
        if(!app->cart_ram) return RomLoadNoMem;
        memset(app->cart_ram, 0, app->cart_ram_size);
    }

    /* Allocate as many 16 KB bank slots as the heap safely affords.
     *
     * IMPORTANT: the emulator core (Gameboy: 8K VRAM + 8K WRAM + packed
     * framebuffer + CPU state, ~22 KB) plus the GUI takeover (mutex, canvas,
     * input subscription) are allocated AFTER the ROM cache, so room for
     * them is reserved up front. Each slot is its own allocation: no huge
     * contiguous block is ever requested, which makes the loader immune to
     * heap fragmentation (the old single-malloc full-ROM path could crash
     * the firmware even when the total free heap looked sufficient). */
    u32 switchable = (u32)(rc->banks - 1);
    const size_t reserve = HEAP_RESERVE + sizeof(Gameboy) + 1024 /* alloc slack */;

    /* slot bookkeeping arrays (a few bytes per slot) */
    size_t free_heap = memmgr_get_free_heap();
    u32 max_slots = (u32)(free_heap / BANK_SIZE) + 1;
    if(max_slots > switchable) max_slots = switchable;
    if(max_slots < 1) max_slots = 1;

    rc->slots = (u8**)safe_malloc(max_slots * sizeof(u8*));
    rc->slot_bank = (u16*)safe_malloc(max_slots * sizeof(u16));
    rc->slot_use = (u32*)safe_malloc(max_slots * sizeof(u32));
    if(!rc->slots || !rc->slot_bank || !rc->slot_use) return RomLoadNoMem;

    rc->num_slots = 0;
    while((u32)rc->num_slots < max_slots) {
        if(memmgr_get_free_heap() < reserve + BANK_SIZE + ALLOC_MARGIN) break;
        u8* slot = (u8*)safe_malloc(BANK_SIZE);
        if(!slot) break;
        rc->slots[rc->num_slots] = slot;
        rc->slot_bank[rc->num_slots] = 0; /* bank 0 never lives in a slot */
        rc->slot_use[rc->num_slots] = 0;
        rc->num_slots++;
    }

    /* not even one 16 KB slot fits: fail gracefully with the
     * "Not enough RAM" dialog instead of crashing inside malloc() */
    if(rc->num_slots == 0) return RomLoadNoMem;

    if((u32)rc->num_slots >= switchable) {
        /* every switchable bank fits: preload the whole ROM and close the
         * SD file (O(1) bank switching, zero stutter, frees the handle) */
        storage_file_seek(rc->file, BANK_SIZE, true);
        for(u32 i = 0; i < switchable; i++) {
            size_t got = storage_file_read(rc->file, rc->slots[i], BANK_SIZE);
            if(got < BANK_SIZE) memset(rc->slots[i] + got, 0xFF, BANK_SIZE - got);
            rc->slot_bank[i] = (u16)(i + 1);
        }
        rc->fully_loaded = true;
        storage_file_close(rc->file);
        storage_file_free(rc->file);
        rc->file = NULL;
    }

    return RomLoadOk;
}

static void rom_free(AppState* app) {
    RomCache* rc = &app->rom;
    for(u16 i = 0; i < rc->num_slots; i++)
        if(rc->slots[i]) free(rc->slots[i]);
    if(rc->slots) free(rc->slots);
    if(rc->slot_bank) free(rc->slot_bank);
    if(rc->slot_use) free(rc->slot_use);
    if(rc->bank0) free(rc->bank0);
    if(rc->file) {
        storage_file_close(rc->file);
        storage_file_free(rc->file);
    }
    memset(rc, 0, sizeof(*rc));
}

/* ------------------------------------------------------------------- main */

extern "C" int32_t flipgb_app(void* p) {
    UNUSED(p);

    AppState* app = (AppState*)malloc(sizeof(AppState));
    if(!app) return -1;
    memset(app, 0, sizeof(AppState));
    app->frameskip_setting = -1; /* auto */
    g_app = app;

    init_scale_maps();

    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    DialogsApp* dialogs = (DialogsApp*)furi_record_open(RECORD_DIALOGS);

    FuriString* rom_path = furi_string_alloc_set_str("/ext");
    FuriString* sav_path = furi_string_alloc();

    Gui* gui = NULL;
    Canvas* canvas = NULL;
    FuriPubSub* input_events = NULL;
    FuriPubSubSubscription* input_sub = NULL;
    bool fb_cb_added = false;

    do {
        /* --- pick a ROM with the stock file browser (normal GUI mode) --- */
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, ".gb", NULL);
        browser_options.base_path = "/ext";
        browser_options.hide_ext = false;

        if(!dialog_file_browser_show(dialogs, rom_path, rom_path, &browser_options)) {
            break; /* cancelled */
        }

        MBCType mbc = MBCType::None;
        RomLoadResult res = rom_load(app, storage, furi_string_get_cstr(rom_path), &mbc);

        if(res != RomLoadOk) {
            DialogMessage* msg = dialog_message_alloc();
            const char* text = "ROM load failed";
            if(res == RomLoadCgbOnly) text = "Game Boy Color only\nROM: not supported";
            if(res == RomLoadBadMapper) text = "Unsupported mapper";
            if(res == RomLoadNoMem) text = "Not enough RAM";
            dialog_message_set_text(msg, text, 64, 30, AlignCenter, AlignCenter);
            dialog_message_set_buttons(msg, NULL, "OK", NULL);
            dialog_message_show(dialogs, msg);
            dialog_message_free(msg);
            break;
        }

        save_path_for_rom(rom_path, sav_path);
        load_sram(app, storage, sav_path);

        /* --- construct the emulator --- */
        app->fb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!app->fb_mutex) break;

        /* rom_load reserved room for this, but double-check anyway: on the
         * Flipper an unchecked new/malloc crashes the firmware on OOM */
        void* gb_mem = safe_malloc(sizeof(Gameboy));
        if(!gb_mem) {
            DialogMessage* msg = dialog_message_alloc();
            dialog_message_set_text(msg, "Not enough RAM", 64, 30, AlignCenter, AlignCenter);
            dialog_message_set_buttons(msg, NULL, "OK", NULL);
            dialog_message_show(dialogs, msg);
            dialog_message_free(msg);
            break;
        }
        app->gb = new(gb_mem) Gameboy(
            app->rom.bank0,
            app->rom.banks,
            mbc,
            app->cart_ram,
            app->cart_ram_size,
            rom_bank_provider,
            &app->rom);
        app->gb->set_frame_callback(frame_callback, app);
        app->gb->set_row_mask(s_rowmask);

        /* --- sound: piezo plays the dominant APU voice --- */
        app->speaker_acquired = furi_hal_speaker_acquire(50);
        app->sound_enabled = app->speaker_acquired;

        /* --- take over the display --- */
        gui = (Gui*)furi_record_open(RECORD_GUI);
        if(!gui) break;
        app->gui = gui;

        gui_add_framebuffer_callback(gui, framebuffer_commit_callback, app);
        fb_cb_added = true;

        canvas = gui_direct_draw_acquire(gui);
        if(!canvas) break;
        app->canvas = canvas;

        input_events = (FuriPubSub*)furi_record_open(RECORD_INPUT_EVENTS);
        if(!input_events) break;
        input_sub = furi_pubsub_subscribe(input_events, input_events_callback, app);
        if(!input_sub) break;

        /* --- main loop --- */
        uint8_t last_applied_keys = 0;
        uint32_t frame_deadline_us = 0;
        uint32_t last_now_ms = furi_get_tick();
        app->emu_ms_ema = 16 << 4;

        while(!app->exit_requested) {
            /* ------ menu mode ------ */
            if(app->menu_requested) {
                app->menu_requested = false;
                app->menu_active = true;
                app->menu_cursor = 0;
                app->menu_last_keys = app->keys; /* no spurious edges from held keys */
                app->status_msg[0] = 0;
                /* lift all buttons for the game, mute while paused */
                apply_input(app, &last_applied_keys);
                sound_update(app, true);
                menu_draw(app);

                while(app->menu_active && !app->exit_requested) {
                    if(!menu_tick(app, storage, sav_path)) {
                        app->menu_active = false;
                    }
                    furi_delay_ms(33);
                }
                frame_deadline_us = 0;
                last_now_ms = furi_get_tick();
                continue;
            }

            /* ------ decide frameskip ------ */
            int skip_n = app->frameskip_setting >= 0 ? app->frameskip_setting : app->auto_skip;
            bool render_this = app->skip_phase == 0;
            app->skip_phase = (app->skip_phase + 1) % (skip_n + 1);

            /* ------ run one emulated frame ------ */
            apply_input(app, &last_applied_keys);
            app->gb->set_skip_render(!render_this);

            uint32_t t0 = furi_get_tick();
            app->gb->run_to_vblank();
            uint32_t emu_ms = furi_get_tick() - t0;

            /* refresh the piezo with this frame's dominant APU voice */
            sound_update(app, false);

            /* EMA of the cost of one emulated frame (x16 fixed point) */
            app->emu_ms_ema += ((emu_ms << 4) - app->emu_ms_ema) / 8;
            uint32_t ema_ms = app->emu_ms_ema >> 4;
            app->auto_skip = ema_ms <= 17 ? 0 : (int)((ema_ms - 1) / 17);
            if(app->auto_skip > 4) app->auto_skip = 4;

            if(render_this && !app->menu_active) {
                canvas_commit(canvas);
            }

            /* ------ pacing: aim for 59.73 Hz wall time ------ */
            uint32_t now = furi_get_tick();
            uint32_t elapsed_us = (now - last_now_ms) * 1000u;
            last_now_ms = now;

            if(frame_deadline_us > elapsed_us) {
                frame_deadline_us -= elapsed_us;
            } else {
                frame_deadline_us = 0;
            }
            frame_deadline_us += GB_FRAME_US;

            /* cap the backlog so a long stall doesn't fast-forward */
            if(frame_deadline_us > 4 * GB_FRAME_US) frame_deadline_us = 4 * GB_FRAME_US;

            if(frame_deadline_us > GB_FRAME_US + 2000) {
                uint32_t sleep_ms = (frame_deadline_us - GB_FRAME_US) / 1000u;
                furi_delay_ms(sleep_ms);
            } else if(emu_ms == 0) {
                furi_delay_ms(1);
            }
        }

        /* auto-save battery RAM on exit */
        if(app->has_battery) save_sram(app, storage, sav_path);
    } while(false);

    /* --- teardown --- */
    if(app->speaker_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        app->speaker_acquired = false;
    }
    if(input_sub && input_events) furi_pubsub_unsubscribe(input_events, input_sub);
    wait_inflight_zero(&s_input_cb_inflight);
    if(input_events) furi_record_close(RECORD_INPUT_EVENTS);

    if(fb_cb_added) gui_remove_framebuffer_callback(gui, framebuffer_commit_callback, app);
    wait_inflight_zero(&s_fb_cb_inflight);

    if(gui) {
        if(canvas) gui_direct_draw_release(gui);
        furi_record_close(RECORD_GUI);
    }

    if(app->gb) {
        app->gb->~Gameboy(); /* placement-new counterpart */
        free(app->gb);
    }
    rom_free(app);
    if(app->cart_ram) free(app->cart_ram);
    if(app->fb_mutex) furi_mutex_free(app->fb_mutex);

    furi_string_free(rom_path);
    furi_string_free(sav_path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(app);
    g_app = NULL;

    return 0;
}
