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

#include "lib/gbcore/gameboy.h"

#define SCREEN_W    128
#define SCREEN_H    64
#define BUFFER_SIZE (SCREEN_W * SCREEN_H / 8)

#define GB_FRAME_US  16742 /* 59.73 Hz */
#define BANK_SIZE    0x4000u /* MBC bank: 16 KB (bank 0 stays resident whole) */
#define UNIT_SIZE    0x1000u /* cache granularity: 4 KB = quarter bank */
/* Heap kept free for the GUI/system while playing. 10 KB is enough for the
 * direct-draw takeover + input subscription + background services; the old
 * 24 KB reserve was more than one whole ROM bank of wasted headroom and
 * pushed big cartridges (1 MB MBC3 + 32 KB SRAM) into "Not enough RAM". */
#define HEAP_RESERVE (10u * 1024u)
#define ALLOC_MARGIN 1024u /* never take the very last heap block */

#define CFG_DIR    "/ext/apps_data/flipgb"
#define CFG_PATH   CFG_DIR "/flipgb.cfg"
#define CFG_MAGIC  0x31424746u /* 'FGB1' */
#define SS_MAGIC   0x53424746u /* 'FGBS' */
#define SS_VERSION 1u

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

/* ROM page cache.
 *
 * Every 8 KB ROM page lives in its own heap block: there is no "whole ROM
 * in one contiguous malloc" fast path, because a large contiguous
 * allocation is exactly what fails (and used to crash the firmware) on a
 * fragmented heap. 8 KB granularity (half a MBC bank) doubles the number
 * of cache slots per KB of heap and halves the SD stall of a miss, which
 * is what makes bank-switch heavy games (Pokemon) playable while
 * streaming.
 *
 *  - as many 8 KB slots as the heap safely affords are allocated up front;
 *  - if every switchable page got a slot, the ROM is fully resident: page
 *    lookup is a direct O(1) index and the SD file is closed;
 *  - otherwise the slots form an LRU cache streaming pages from SD.
 */
typedef struct {
    File* file; /* NULL once the ROM is fully resident */
    u8* bank0;
    u8** slots; /* num_slots pointers to 4 KB blocks */
    u16* slot_unit; /* which ROM unit each slot holds (0 = empty) */
    u32* slot_use; /* LRU stamps (streaming mode only) */
    u16* slot_hits; /* hit counters: hottest slot is eviction-protected */
    u32 use_counter;
    u32 miss_count; /* diagnostics: SD unit misses */
    u16 num_slots;
    u16 banks;
    bool fully_loaded; /* slots[i] permanently holds unit i+4 */
} RomCache;

typedef struct {
    uint8_t screen[BUFFER_SIZE]; /* 1bpp page-format, bit set = light */

    Gui* gui;
    Canvas* canvas;
    FuriMutex* fb_mutex;

    volatile uint8_t keys; /* KBIT_* currently pressed */
    uint8_t keys_blocked; /* held keys suppressed until physical release */
    volatile bool menu_requested;
    volatile bool menu_active;
    volatile bool exit_requested;

    Gameboy* gb;
    void* gb_mem; /* raw block for the Gameboy, reserved during rom_load */
    RomCache rom;
    u8* cart_ram;
    u32 cart_ram_size;
    bool has_battery;

    /* settings */
    uint8_t scale_mode; /* 0 = fit (squash), 1 = 1:1 center crop */
    uint8_t dither_mode; /* 0 = standard 2x2, 1 = temporal, 2 = high contrast */
    uint8_t dither_phase; /* temporal mode: alternates per rendered frame */
    uint8_t okhold_start; /* long-press OK also injects Start */
    volatile bool okhold_request;

    /* diagnostics */
    uint32_t last_miss_count;
    uint32_t miss_ema; /* x16 fixed point, SD misses per frame */

    uint8_t cart_type; /* raw header byte 0x147 */

    /* sound (single-tone piezo fed with the dominant APU voice) */
    uint8_t volume_setting; /* 0 = off, 1..4 = 25/50/75/100% */
    bool speaker_available; /* probed once at startup */
    bool speaker_acquired; /* currently held (released after silence) */
    uint16_t silent_frames;
    uint32_t tone_freq; /* currently playing tone, 0 = silent */
    uint8_t tone_vol; /* 0..15, scaled by master volume */
    uint8_t tone_setting; /* volume_setting the current tone was started with */

    uint8_t ok_hold_frames; /* long-press OK detection (polled) */

    /* menu */
    int menu_cursor;
    int menu_top; /* first visible row (scrolling menu) */
    uint8_t menu_last_keys;
    int frameskip_setting; /* -1 = auto, 0..4 fixed */
    int inject_start_frames;
    int inject_select_frames;
    char rom_title[17];
    char status_msg[32];

    /* pacing */
    uint32_t emu_us_ema; /* EMA of one frame's emulation cost, microseconds */
    int auto_skip;
    int skip_phase;
    int skip_dwell; /* frames the desired skip level disagreed (hysteresis) */
    uint32_t fb_hash; /* last committed framebuffer hash (skip idle commits) */
    bool fb_dirty;
} AppState;

static AppState* g_app = NULL;

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

/* Age the hit counters so the "hottest unit" protection adapts when the
 * game's hot bank changes. Runs on hit AND miss paths. */
static inline void rc_decay_hits(RomCache* rc) {
    if((rc->use_counter & 63) == 0) {
        for(u16 j = 0; j < rc->num_slots; j++)
            rc->slot_hits[j] >>= 1;
    }
}

/* Stream `unit` from SD into a victim slot and return the slot index.
 *
 * Eviction is LRU with protections:
 *  - never evict the THREE most recently used slots: the cartridge maps
 *    up to 3 quarters when a lazy fetch happens (their stamps are
 *    refreshed right before), so mapped pointers can never dangle;
 *  - with >= 6 slots, also keep the hottest slot (most hits: the music
 *    driver's unit is touched every frame and pure LRU evicted it right
 *    before each use). */
static u16 rc_fill(RomCache* rc, uint unit) {
    /* top-3 use stamps + hottest */
    u16 m1 = 0xFFFF, m2 = 0xFFFF, m3 = 0xFFFF;
    u32 u1 = 0, u2 = 0, u3 = 0;
    u16 hottest = 0;
    for(u16 i = 0; i < rc->num_slots; i++) {
        u32 u = rc->slot_use[i];
        if(m1 == 0xFFFF || u > u1) {
            m3 = m2;
            u3 = u2;
            m2 = m1;
            u2 = u1;
            m1 = i;
            u1 = u;
        } else if(m2 == 0xFFFF || u > u2) {
            m3 = m2;
            u3 = u2;
            m2 = i;
            u2 = u;
        } else if(m3 == 0xFFFF || u > u3) {
            m3 = i;
            u3 = u;
        }
        if(rc->slot_hits[i] > rc->slot_hits[hottest]) hottest = i;
    }
    bool protect_hot = rc->num_slots >= 6;

    u16 lru = 0xFFFF;
    u32 lru_use = 0xFFFFFFFFu;
    for(u16 i = 0; i < rc->num_slots; i++) {
        if(i == m1 || i == m2 || i == m3) continue;
        if(protect_hot && i == hottest) continue;
        if(rc->slot_use[i] < lru_use) {
            lru_use = rc->slot_use[i];
            lru = i;
        }
    }
    if(lru == 0xFFFF) {
        /* all candidates excluded: sacrifice the hottest (it is never one
         * of the 3 MRU in this branch, so mapped quarters stay safe) */
        lru = hottest;
        if(lru == m1 || lru == m2 || lru == m3) {
            /* degenerate 4-slot case: pick the oldest of the MRU trio */
            lru = m3;
        }
    }

    rc->miss_count++;
    storage_file_seek(rc->file, (u32)unit * UNIT_SIZE, true);
    size_t got = storage_file_read(rc->file, rc->slots[lru], UNIT_SIZE);
    if(got < UNIT_SIZE) {
        /* short read (SD hiccup / truncated ROM): open-bus instead of
         * stale data from whatever unit lived here before */
        memset(rc->slots[lru] + got, 0xFF, UNIT_SIZE - got);
    }
    rc->slot_unit[lru] = (u16)unit;
    rc->slot_use[lru] = ++rc->use_counter;
    rc->slot_hits[lru] = 0; /* heat comes from real hits only */
    rc_decay_hits(rc);
    return lru;
}

static const u8* rom_bank_provider(void* ctx, uint unit) {
    RomCache* rc = (RomCache*)ctx;

    /* units 0-3 are the four quarters of the resident bank 0 */
    if(unit < 4) return rc->bank0 + unit * UNIT_SIZE;

    if(rc->fully_loaded) {
        return rc->slots[unit - 4]; /* O(1), the whole ROM is resident */
    }

    /* cache lookup */
    for(u16 i = 0; i < rc->num_slots; i++) {
        if(rc->slot_unit[i] == unit) {
            rc->slot_use[i] = ++rc->use_counter;
            if(rc->slot_hits[i] < 0xFFFF) rc->slot_hits[i]++;
            rc_decay_hits(rc);
            return rc->slots[i];
        }
    }

    return rc->slots[rc_fill(rc, unit)];
}

/* -------------------------------------------------------------- rendering */

/* Destination -> source coordinate maps (128 <- 160, 64 <- 144) */
static uint8_t s_xmap[SCREEN_W];
static uint8_t s_ymap[SCREEN_H];

/* Scanlines that actually survive the 144 -> 64 downscale (fit mode) or
 * the 1:1 center crop (rows 40-103). Handed to the emulator core so the
 * PPU skips the other 80 lines entirely (~55% less rendering work). */
static u8 s_rowmask_fit[GAMEBOY_HEIGHT];
static u8 s_rowmask_crop[GAMEBOY_HEIGHT];

/* Dither LUTs: map one packed 2bpp framebuffer byte (4 consecutive GB
 * pixels) to 4 light/dark bits (bit j = pixel j).
 *  - s_dither4[phase][y_parity]: 2x2 ordered dither (white -> lit,
 *    light gray -> 3/4 lit, dark gray -> 1/4 lit, black -> off).
 *    phase 1 shifts the pattern diagonally: alternating phases between
 *    rendered frames makes the Flipper LCD's slow pixels average the
 *    two patterns into perceived grayscale ("temporal" mode).
 *  - s_dither_hc: high-contrast thresholding (grays snap to black/white),
 *    crisper text in UI-heavy games. */
static uint8_t s_dither4[2][2][256];

/* High-contrast needs no table: pixel j is light iff its shade <= 1,
 * i.e. the high bit of its 2-bit field is 0. */
static inline uint8_t hc4(uint8_t packed) {
    uint8_t four = 0;
    if(!(packed & 0x02)) four |= 1;
    if(!(packed & 0x08)) four |= 2;
    if(!(packed & 0x20)) four |= 4;
    if(!(packed & 0x80)) four |= 8;
    return four;
}

static void init_scale_maps(void) {
    for(int x = 0; x < SCREEN_W; x++)
        s_xmap[x] = (uint8_t)((x * (int)GAMEBOY_WIDTH) / SCREEN_W);
    for(int y = 0; y < SCREEN_H; y++)
        s_ymap[y] = (uint8_t)((y * (int)GAMEBOY_HEIGHT) / SCREEN_H);

    memset(s_rowmask_fit, 0, sizeof(s_rowmask_fit));
    for(int y = 0; y < SCREEN_H; y++)
        s_rowmask_fit[s_ymap[y]] = 1;
    memset(s_rowmask_crop, 0, sizeof(s_rowmask_crop));
    for(int y = 0; y < SCREEN_H; y++)
        s_rowmask_crop[40 + y] = 1; /* center 64 rows */

    for(int ph = 0; ph < 2; ph++) {
        for(int py = 0; py < 2; py++) {
            for(int v = 0; v < 256; v++) {
                uint8_t bits = 0;
                for(int j = 0; j < 4; j++) {
                    int s = (v >> (j * 2)) & 3;
                    int px = (j & 1) ^ ph;
                    int epy = py ^ ph;
                    bool light;
                    switch(s) {
                    case 0:
                        light = true;
                        break;
                    case 1:
                        light = !(px == 1 && epy == 1);
                        break;
                    case 2:
                        light = (px == 0 && epy == 0);
                        break;
                    default:
                        light = false;
                        break;
                    }
                    if(light) bits |= (uint8_t)(1 << j);
                }
                s_dither4[ph][py][v] = bits;
            }
        }
    }
}

/* Called by the core on every vblank, before the framebuffer is cleared.
 * Converts 4 shades -> 1 bit with a 2x2 ordered dither.
 *
 * Byte-oriented: destination pixels 4g..4g+3 map to source pixels
 * 5g..5g+3 (the 160->128 downscale drops every 5th column), so each
 * destination nibble comes from 8 consecutive source bits, converted with
 * a single LUT lookup instead of per-pixel shade extraction. */
static void frame_callback(void* ctx) {
    AppState* app = (AppState*)ctx;
    const u8* raw = app->gb->get_framebuffer().raw(); /* packed 2bpp */

    furi_mutex_acquire(app->fb_mutex, FuriWaitForever);

    uint8_t* dst = app->screen;
    bool crop = app->scale_mode == 1;
    uint8_t phase = 0;
    if(app->dither_mode == 1) {
        app->dither_phase ^= 1; /* temporal: alternate per rendered frame */
        phase = app->dither_phase;
    }
    bool hicon = app->dither_mode == 2;
    for(int y = 0; y < SCREEN_H; y++) {
        /* storage is compacted to the 64 displayed rows in ascending order
         * (GB_FB_ROWS=64 + row mask), so displayed row y == storage slot y */
        const u8* src = raw + (uint)y * (GAMEBOY_WIDTH / 4);
        uint8_t bit = (uint8_t)(1u << (y & 7));
        uint8_t nbit = (uint8_t)~bit;
        uint8_t* row = dst + (y >> 3) * SCREEN_W;
        const uint8_t* dlut = s_dither4[phase][y & 1];

        if(crop) {
            /* 1:1 center crop: 128 of 160 columns (skip 16 px each side),
             * byte-aligned: one packed source byte = 4 destination pixels */
            const u8* csrc = src + 4;
            for(int g = 0; g < SCREEN_W / 4; g++) {
                uint8_t four = hicon ? hc4(csrc[g]) : dlut[csrc[g]];
                uint8_t* p = row + g * 4;
                p[0] = (four & 1) ? (uint8_t)(p[0] | bit) : (uint8_t)(p[0] & nbit);
                p[1] = (four & 2) ? (uint8_t)(p[1] | bit) : (uint8_t)(p[1] & nbit);
                p[2] = (four & 4) ? (uint8_t)(p[2] | bit) : (uint8_t)(p[2] & nbit);
                p[3] = (four & 8) ? (uint8_t)(p[3] | bit) : (uint8_t)(p[3] & nbit);
            }
            continue;
        }

        for(int g = 0; g < SCREEN_W / 4; g++) {
            uint spx = (uint)g * 5; /* first source pixel of this group */
            uint so = spx >> 2;
            uint sh = (spx & 3) * 2;
            uint packed = ((uint)src[so] | ((uint)src[so + 1] << 8)) >> sh;
            uint8_t four = hicon ? hc4((uint8_t)packed) : dlut[packed & 0xFF];

            uint8_t* p = row + g * 4;
            p[0] = (four & 1) ? (uint8_t)(p[0] | bit) : (uint8_t)(p[0] & nbit);
            p[1] = (four & 2) ? (uint8_t)(p[1] | bit) : (uint8_t)(p[1] & nbit);
            p[2] = (four & 4) ? (uint8_t)(p[2] | bit) : (uint8_t)(p[2] & nbit);
            p[3] = (four & 8) ? (uint8_t)(p[3] | bit) : (uint8_t)(p[3] & nbit);
        }
    }

    /* cheap content hash: unchanged frames skip the 2-3 ms canvas_commit.
     * NOTE: this MUST run -- fb_dirty gates every game commit. */
    {
        const uint32_t* w = (const uint32_t*)app->screen;
        uint32_t h = 2166136261u;
        for(uint i = 0; i < BUFFER_SIZE / 4; i++) {
            h ^= w[i];
            h *= 16777619u;
        }
        if(h != app->fb_hash) {
            app->fb_hash = h;
            app->fb_dirty = true;
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

#define OKHOLD_FRAMES 45 /* ~750 ms at 59.73 Hz */

/* Read all 6 buttons straight from the GPIO input data registers.
 *
 * Why not the input-events pubsub: the firmware's input service makes an
 * UNBOUNDED wait on the FreeRTOS timer daemon (priority 2) every time a
 * key is released (input.c: furi_timer_stop + spin on is_running). Our
 * emulator loop at priority 16 starves that daemon whenever a game runs
 * at/below real-time speed, so the input service could park forever mid-
 * release: every event -- including the release of a held direction and
 * the menu gesture -- died, wedging the game with a stuck button until
 * reboot. Polling the pins directly has no dependency on any service.
 *
 * input_pins[] is the input service's own pin/polarity table (exported
 * to FAPs): polarity is data-driven (OK is active-HIGH on PH3/BOOT0, the
 * rest active-low). furi_hal_gpio_read() is a bare IDR load: passive, no
 * side effects, safe alongside the service's EXTI configuration. */
static uint8_t poll_keys(void) {
    uint8_t keys = 0;
    for(size_t i = 0; i < input_pins_count; i++) {
        const InputPin* p = &input_pins[i];
        if(furi_hal_gpio_read(p->gpio) == p->inverted) continue; /* not pressed */
        switch(p->key) {
        case InputKeyUp:
            keys |= KBIT_UP;
            break;
        case InputKeyDown:
            keys |= KBIT_DOWN;
            break;
        case InputKeyLeft:
            keys |= KBIT_LEFT;
            break;
        case InputKeyRight:
            keys |= KBIT_RIGHT;
            break;
        case InputKeyOk:
            keys |= KBIT_A;
            break;
        case InputKeyBack:
            keys |= KBIT_B;
            break;
        default:
            break;
        }
    }
    return keys;
}

/* Sample the pad and derive gestures. Called once per emulated frame and
 * once per 16 ms menu tick. app->keys is REPLACED with ground truth on
 * every call: a stuck bit cannot survive longer than one frame no matter
 * what the rest of the system does. */
static void input_poll(AppState* app) {
    uint8_t prev = app->keys;
    uint8_t keys = poll_keys();
    app->keys = keys;

    /* Up+Down = menu gesture (physically impossible on a real GB d-pad).
     * Edge-triggered so holding it across a menu close can't reopen it. */
    const uint8_t combo = KBIT_UP | KBIT_DOWN;
    if(!app->menu_active && (keys & combo) == combo && (prev & combo) != combo) {
        app->menu_requested = true;
    }

    /* optional long-press OK -> Start (frame counter: the OS InputTypeLong
     * event came from the starvable timer daemon, so we count ourselves) */
    if(app->okhold_start && !app->menu_active && (keys & KBIT_A)) {
        if(app->ok_hold_frames < 255) app->ok_hold_frames++;
        if(app->ok_hold_frames == OKHOLD_FRAMES) app->okhold_request = true;
    } else {
        app->ok_hold_frames = 0;
    }
}

/* Apply the current key snapshot to the emulated joypad (edge based) */
static void apply_input(AppState* app, uint8_t* last_applied) {
    /* keys_blocked: the A/B press that operated the menu must not leak
     * into the game as a fresh press when the menu closes; blocked bits
     * clear automatically on physical release */
    app->keys_blocked &= app->keys;
    uint8_t now = app->menu_active ? 0 : (uint8_t)(app->keys & ~app->keys_blocked);
    /* Opposite d-pad directions are physically impossible on a real Game
     * Boy (single rocker): games have NO logic for that state and can
     * corrupt their movement state machines (stuck "walking" forever --
     * this froze SML when several buttons were mashed together). The
     * Flipper's pad CAN produce them: drop both sides, like hardware.
     * Up+Down is also our menu gesture. */
    if((now & (KBIT_UP | KBIT_DOWN)) == (KBIT_UP | KBIT_DOWN))
        now &= (uint8_t) ~(KBIT_UP | KBIT_DOWN);
    if((now & (KBIT_LEFT | KBIT_RIGHT)) == (KBIT_LEFT | KBIT_RIGHT))
        now &= (uint8_t) ~(KBIT_LEFT | KBIT_RIGHT);
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

/* -------------------------------------------------------- storage helpers */

/* Write/read in small chunks, yielding between them. A single large
 * synchronous storage call from the app thread while the SD is slow
 * (wear leveling stalls, FAT free-cluster scans on fragmented cards)
 * could stall the whole app for seconds and starve system threads --
 * state saves froze the device intermittently. Chunking bounds each
 * storage transaction and the yields keep the system responsive. */
static bool file_write_chunked(File* f, const void* data, size_t size) {
    const u8* p = (const u8*)data;
    while(size) {
        size_t n = size > 2048 ? 2048 : size;
        if(storage_file_write(f, p, n) != n) return false;
        p += n;
        size -= n;
        furi_delay_ms(1);
    }
    return true;
}

static bool file_read_chunked(File* f, void* data, size_t size) {
    u8* p = (u8*)data;
    while(size) {
        size_t n = size > 2048 ? 2048 : size;
        if(storage_file_read(f, p, n) != n) return false;
        p += n;
        size -= n;
        furi_delay_ms(1);
    }
    return true;
}

/* ----------------------------------------------------------------- config */

typedef struct {
    uint32_t magic;
    uint8_t volume;
    uint8_t frameskip; /* 0xFF = auto */
    uint8_t scale;
    uint8_t okhold;
    uint8_t dither;
} FlipGbCfg;

static void config_load(AppState* app, Storage* storage) {
    FlipGbCfg cfg;
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CFG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(f, &cfg, sizeof(cfg)) == sizeof(cfg) && cfg.magic == CFG_MAGIC) {
            if(cfg.volume <= 4) app->volume_setting = cfg.volume;
            app->frameskip_setting =
                cfg.frameskip == 0xFF ? -1 : (cfg.frameskip <= 4 ? cfg.frameskip : -1);
            app->scale_mode = cfg.scale ? 1 : 0;
            app->okhold_start = cfg.okhold ? 1 : 0;
            if(cfg.dither <= 2) app->dither_mode = cfg.dither;
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

static void config_save(AppState* app, Storage* storage) {
    storage_simply_mkdir(storage, CFG_DIR);
    FlipGbCfg cfg = {
        CFG_MAGIC,
        app->volume_setting,
        (uint8_t)(app->frameskip_setting < 0 ? 0xFF : app->frameskip_setting),
        app->scale_mode,
        app->okhold_start,
        app->dither_mode,
    };
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CFG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &cfg, sizeof(cfg));
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* -------------------------------------------------------------------- rtc */

/* Wall clock for the MBC3 RTC (Pokemon Gold/Silver day/night etc.) */
static u32 rtc_now_provider(void) {
    return (u32)furi_hal_rtc_get_timestamp();
}

/* -------------------------------------------------------------- savestate */

typedef struct {
    uint32_t magic, version;
    CPU::State cpu;
    uint8_t iflag, ie;
    /* video */
    uint8_t lcdc, stat, scy, scx, ly, lyc, wy, wx, bgp, obp0, obp1, vmode;
    uint32_t vcounter;
    /* timer */
    uint8_t tdiv, tima, tma, tac;
    uint32_t tclocks, tdivclocks;
    /* serial */
    uint8_t sb, sc;
    uint32_t sclk;
    Cartridge::BankState banks;
    Mbc3Rtc rtc;
    Apu::State apu;
    uint32_t cart_ram_size;
} SsHead;

static void ss_path_for_rom(FuriString* rom_path, FuriString* out) {
    (furi_string_set)(out, rom_path);
    furi_string_cat_str(out, ".ss1");
}

static bool save_state(AppState* app, Storage* storage, FuriString* ss_path) {
    Gameboy* gb = app->gb;
    SsHead h{};
    h.magic = SS_MAGIC;
    h.version = SS_VERSION;
    gb->cpu.export_state(&h.cpu);
    h.iflag = gb->cpu.interrupt_flag.value();
    h.ie = gb->cpu.interrupt_enabled.value();
    h.lcdc = gb->video.control_byte;
    h.stat = gb->video.lcd_status.value();
    h.scy = gb->video.scroll_y.value();
    h.scx = gb->video.scroll_x.value();
    h.ly = gb->video.line.value();
    h.lyc = gb->video.ly_compare.value();
    h.wy = gb->video.window_y.value();
    h.wx = gb->video.window_x.value();
    h.bgp = gb->video.bg_palette.value();
    h.obp0 = gb->video.sprite_palette_0.value();
    h.obp1 = gb->video.sprite_palette_1.value();
    h.vmode = gb->video.get_mode();
    h.vcounter = gb->video.get_cycle_counter();
    h.tdiv = gb->timer.get_divider();
    h.tima = gb->timer.get_timer();
    h.tma = gb->timer.get_timer_modulo();
    h.tac = gb->timer.get_timer_control();
    h.tclocks = gb->timer.get_clocks();
    h.tdivclocks = gb->timer.get_div_clocks();
    gb->mmu.export_serial(&h.sb, &h.sc, &h.sclk);
    gb->cartridge.export_banks(&h.banks);
    h.rtc = gb->cartridge.rtc_state();
    gb->apu.export_state(&h.apu);
    h.cart_ram_size = app->cart_ram_size;

    /* creating/extending a file makes FatFS allocate from the shared
     * heap (via the storage service): refuse under memory pressure
     * instead of risking the firmware's OOM crash */
    if(memmgr_get_free_heap() < 4096) return false;

    /* write to a temp file, then rename: a stall/power-loss mid-write
     * can never destroy the previous state */
    FuriString* tmp_path = furi_string_alloc();
    (furi_string_set)(tmp_path, ss_path);
    furi_string_cat_str(tmp_path, ".tmp");

    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, furi_string_get_cstr(tmp_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = file_write_chunked(f, &h, sizeof(h));
        ok = ok && file_write_chunked(f, gb->video.vram_ptr(), 0x2000);
        ok = ok && file_write_chunked(f, gb->mmu.wram_ptr(), 0x2000);
        ok = ok && file_write_chunked(f, gb->mmu.oam_ptr(), 0xA0);
        ok = ok && file_write_chunked(f, gb->mmu.hram_ptr(), 0x80);
        if(app->cart_ram_size) ok = ok && file_write_chunked(f, app->cart_ram, app->cart_ram_size);
        storage_file_close(f);
    }
    storage_file_free(f);

    if(ok) {
        storage_common_remove(storage, furi_string_get_cstr(ss_path));
        ok = storage_common_rename(
                 storage, furi_string_get_cstr(tmp_path), furi_string_get_cstr(ss_path)) == FSE_OK;
    } else {
        storage_common_remove(storage, furi_string_get_cstr(tmp_path));
    }
    furi_string_free(tmp_path);
    return ok;
}

static bool load_state(AppState* app, Storage* storage, FuriString* ss_path) {
    Gameboy* gb = app->gb;
    SsHead h;
    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, furi_string_get_cstr(ss_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = storage_file_read(f, &h, sizeof(h)) == sizeof(h) && h.magic == SS_MAGIC &&
             h.version == SS_VERSION && h.cart_ram_size == app->cart_ram_size;
        ok = ok && file_read_chunked(f, gb->video.vram_ptr(), 0x2000);
        ok = ok && file_read_chunked(f, gb->mmu.wram_ptr(), 0x2000);
        ok = ok && file_read_chunked(f, gb->mmu.oam_ptr(), 0xA0);
        ok = ok && file_read_chunked(f, gb->mmu.hram_ptr(), 0x80);
        if(ok && app->cart_ram_size) ok = file_read_chunked(f, app->cart_ram, app->cart_ram_size);
        storage_file_close(f);
    }
    storage_file_free(f);
    if(!ok) return false;

    gb->cpu.import_state(&h.cpu);
    gb->cpu.interrupt_flag.set(h.iflag);
    gb->cpu.interrupt_enabled.set(h.ie);
    gb->video.control_byte = h.lcdc;
    gb->video.lcd_control.set(h.lcdc);
    gb->video.lcd_status.set(h.stat);
    gb->video.scroll_y.set(h.scy);
    gb->video.scroll_x.set(h.scx);
    gb->video.ly_compare.set(h.lyc);
    gb->video.window_y.set(h.wy);
    gb->video.window_x.set(h.wx);
    gb->video.bg_palette.set(h.bgp);
    gb->video.sprite_palette_0.set(h.obp0);
    gb->video.sprite_palette_1.set(h.obp1);
    gb->video.set_state(h.ly, h.vmode, h.vcounter);
    gb->timer.set_timer(h.tima);
    gb->timer.set_timer_modulo(h.tma);
    gb->timer.set_timer_control(h.tac);
    gb->timer.set_divider_raw(h.tdiv);
    gb->timer.set_counters(h.tclocks, h.tdivclocks);
    gb->mmu.import_serial(h.sb, h.sc, h.sclk);
    gb->apu.import_state(&h.apu);
    gb->cartridge.rtc_state() = h.rtc;
    gb->cartridge.import_banks(&h.banks); /* remaps ROM pages via provider */
    return true;
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
    if(!app->speaker_available) return;

    ApuVoice best = {false, 0, 0, 0};
    bool have = false;

    if(!force_silent && app->volume_setting && !app->menu_active) {
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
            } else {
                /* acquire lazily: holding the speaker for the whole
                 * session made every system notification stall 30 ms
                 * waiting for it (aggravating service-queue backups) */
                if(!app->speaker_acquired) {
                    app->speaker_acquired = furi_hal_speaker_acquire(4);
                    if(!app->speaker_acquired) have = false;
                }
                if(have && (f != app->tone_freq || vol != app->tone_vol ||
                            app->volume_setting != app->tone_setting)) {
                    /* piezo loudness vs PWM value is very nonlinear:
                     * spread the 4 user levels perceptually */
                    static const float level[5] = {0.0f, 0.10f, 0.25f, 0.50f, 1.0f};
                    furi_hal_speaker_start(
                        (float)f, ((float)vol / 15.0f) * level[app->volume_setting]);
                    app->tone_freq = f;
                    app->tone_vol = vol;
                    app->tone_setting = app->volume_setting;
                }
            }
        }
    }

    if(have) {
        app->silent_frames = 0;
    } else {
        if(app->tone_freq && app->speaker_acquired) {
            furi_hal_speaker_stop();
            app->tone_freq = 0;
            app->tone_vol = 0;
        }
        /* release after ~1 s of silence so system sounds work again */
        if(app->speaker_acquired && ++app->silent_frames > 60) {
            furi_hal_speaker_release();
            app->speaker_acquired = false;
            app->silent_frames = 0;
        }
    }
}

/* ------------------------------------------------------------------- save */

static void sav_write_rtc(AppState* app, File* f);
static void sav_read_rtc(AppState* app, File* f);

static void save_path_for_rom(FuriString* rom_path, FuriString* out) {
    /* parentheses bypass the C11 _Generic macro (not usable from C++) */
    (furi_string_set)(out, rom_path);
    furi_string_cat_str(out, ".sav");
}

static bool save_sram(AppState* app, Storage* storage, FuriString* sav_path) {
    if(!app->cart_ram || !app->cart_ram_size || !app->has_battery) return false;

    /* write to a temp file first: FSOM_CREATE_ALWAYS truncates, so a
     * battery pull / SD glitch mid-write used to destroy the old save */
    FuriString* tmp_path = furi_string_alloc();
    (furi_string_set)(tmp_path, sav_path);
    furi_string_cat_str(tmp_path, ".tmp");

    if(memmgr_get_free_heap() < 4096) {
        furi_string_free(tmp_path);
        return false;
    }

    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, furi_string_get_cstr(tmp_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = file_write_chunked(f, app->cart_ram, app->cart_ram_size);
        if(ok && app->gb && app->gb->cartridge.has_rtc()) sav_write_rtc(app, f);
        storage_file_close(f);
    }
    storage_file_free(f);

    if(ok) {
        storage_common_remove(storage, furi_string_get_cstr(sav_path));
        ok = storage_common_rename(
                 storage, furi_string_get_cstr(tmp_path), furi_string_get_cstr(sav_path)) ==
             FSE_OK;
    }
    furi_string_free(tmp_path);
    return ok;
}

/* RTC footer: 5 current + 5 latched u32 regs + u64 unix timestamp (the
 * de-facto .sav RTC format used by VBA/mGBA) */
static void sav_write_rtc(AppState* app, File* f) {
    Cartridge& c = app->gb->cartridge;
    u32 t = c.rtc_state().halted ? c.rtc_state().halt_value :
                                   (rtc_now_provider() - c.rtc_state().base);
    u32 days = t / 86400;
    u32 regs[10] = {
        t % 60,
        (t / 60) % 60,
        (t / 3600) % 24,
        days & 0xFF,
        ((days >> 8) & 1) | (c.rtc_state().halted ? 0x40u : 0u),
        c.rtc_state().latched[0],
        c.rtc_state().latched[1],
        c.rtc_state().latched[2],
        c.rtc_state().latched[3],
        c.rtc_state().latched[4],
    };
    uint64_t stamp = rtc_now_provider();
    storage_file_write(f, regs, sizeof(regs));
    storage_file_write(f, &stamp, sizeof(stamp));
}

static void sav_read_rtc(AppState* app, File* f) {
    Cartridge& c = app->gb->cartridge;
    u32 regs[10];
    uint64_t stamp;
    if(storage_file_read(f, regs, sizeof(regs)) != sizeof(regs)) return;
    if(storage_file_read(f, &stamp, sizeof(stamp)) != sizeof(stamp)) return;
    u32 days = (regs[3] & 0xFF) | ((regs[4] & 1) << 8);
    u32 total = ((days * 24 + regs[2]) * 60 + regs[1]) * 60 + regs[0];
    bool halted = (regs[4] & 0x40) != 0;
    u32 now = rtc_now_provider();
    if(!halted && now > (u32)stamp) total += now - (u32)stamp; /* time passed while off */
    c.rtc_state().halted = halted;
    c.rtc_state().halt_value = total;
    c.rtc_state().base = now - total;
    for(int i = 0; i < 5; i++)
        c.rtc_state().latched[i] = (u8)regs[5 + i];
}

static void load_sram(AppState* app, Storage* storage, FuriString* sav_path) {
    if(!app->cart_ram || !app->cart_ram_size) return;

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, furi_string_get_cstr(sav_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(f, app->cart_ram, app->cart_ram_size);
        if(app->gb && app->gb->cartridge.has_rtc()) sav_read_rtc(app, f);
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* ------------------------------------------------------------------- menu */

#define MENU_ITEMS   12
#define MENU_VISIBLE 6

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
        "Scale",
        "Dither",
        "OK hold",
        "Save state",
        "Load state",
        "Save SRAM",
        "Exit",
    };

    canvas_set_font(c, FontSecondary);

    /* scrolling window of MENU_VISIBLE rows */
    if(app->menu_cursor < app->menu_top) app->menu_top = app->menu_cursor;
    if(app->menu_cursor >= app->menu_top + MENU_VISIBLE)
        app->menu_top = app->menu_cursor - MENU_VISIBLE + 1;

    for(int row = 0; row < MENU_VISIBLE; row++) {
        int i = app->menu_top + row;
        if(i >= MENU_ITEMS) break;
        int y = 21 + row * 8;
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
            canvas_draw_str(c, 76, y, buf);
        }
        if(i == 4) {
            static const char* vols[5] = {"off", "25%", "50%", "75%", "100%"};
            canvas_draw_str(c, 76, y, !app->speaker_available ? "n/a" : vols[app->volume_setting]);
        }
        if(i == 5) canvas_draw_str(c, 76, y, app->scale_mode ? "1:1 crop" : "fit");
        if(i == 6) {
            static const char* dm[3] = {"std", "temporal", "hi-con"};
            canvas_draw_str(c, 76, y, dm[app->dither_mode]);
        }
        if(i == 7) canvas_draw_str(c, 76, y, app->okhold_start ? "A+Start" : "A only");
    }
    /* scroll indicators */
    if(app->menu_top > 0) canvas_draw_str(c, 122, 20, "^");
    if(app->menu_top + MENU_VISIBLE < MENU_ITEMS) canvas_draw_str(c, 122, 62, "v");

    if(app->status_msg[0]) {
        canvas_draw_str(c, 70, 10, app->status_msg);
    } else {
        /* diagnostics: free heap + real cost of one emulated frame
         * (16.7ms = full speed; above that the game runs slow) */
        char diagbuf[36];
        snprintf(
            diagbuf,
            sizeof(diagbuf),
            "%uk %lums m%lu",
            (unsigned)(memmgr_get_free_heap() / 1024u),
            (unsigned long)(app->emu_us_ema / 1000u),
            (unsigned long)(app->miss_ema >> 4));
        canvas_draw_str_aligned(c, 126, 10, AlignRight, AlignBottom, diagbuf);
    }

    canvas_commit(c);
}

/* returns true while the menu stays open */
static bool menu_tick(AppState* app, Storage* storage, FuriString* sav_path, FuriString* ss_path) {
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
        if(app->menu_cursor == 4 && app->speaker_available) {
            /* volume: off, 25, 50, 75, 100% */
            if(pressed & KBIT_RIGHT)
                app->volume_setting = (uint8_t)((app->volume_setting + 1) % 5);
            else
                app->volume_setting = (uint8_t)((app->volume_setting + 4) % 5);
        }
        if(app->menu_cursor == 5) {
            app->scale_mode ^= 1;
            /* recompact the framebuffer to the new visible rows */
            app->gb->set_row_mask(app->scale_mode ? s_rowmask_crop : s_rowmask_fit);
        }
        if(app->menu_cursor == 6) {
            if(pressed & KBIT_RIGHT)
                app->dither_mode = (uint8_t)((app->dither_mode + 1) % 3);
            else
                app->dither_mode = (uint8_t)((app->dither_mode + 2) % 3);
        }
        if(app->menu_cursor == 7) {
            app->okhold_start ^= 1;
        }
    }

    if(pressed & KBIT_B) return false; /* Back closes the menu */

    if(pressed & KBIT_A) {
        switch(app->menu_cursor) {
        case 0:
            return false;
        case 1:
            /* extend rather than re-press if an injection is still live:
             * two overlapping presses would merge into one edge for the
             * game (Tetris pause state would get out of sync) */
            if(app->inject_start_frames == 0) app->gb->button_pressed(GbButton::Start);
            app->inject_start_frames = 8;
            return false;
        case 2:
            if(app->inject_select_frames == 0) app->gb->button_pressed(GbButton::Select);
            app->inject_select_frames = 8;
            return false;
        case 3:
            break;
        case 4:
            /* OK toggles mute <-> full volume */
            if(app->speaker_available) app->volume_setting = app->volume_setting ? 0 : 4;
            break;
        case 5:
            app->scale_mode ^= 1;
            app->gb->set_row_mask(app->scale_mode ? s_rowmask_crop : s_rowmask_fit);
            break;
        case 6:
            app->dither_mode = (uint8_t)((app->dither_mode + 1) % 3);
            break;
        case 7:
            app->okhold_start ^= 1;
            break;
        case 8: {
            snprintf(app->status_msg, sizeof(app->status_msg), "saving...");
            menu_draw(app); /* show feedback before the slow SD write */
            FURI_LOG_I(
                "FlipGB", "state save: begin (free heap %u)", (unsigned)memmgr_get_free_heap());
            bool ok = save_state(app, storage, ss_path);
            FURI_LOG_I("FlipGB", "state save: %s", ok ? "ok" : "FAILED");
            snprintf(app->status_msg, sizeof(app->status_msg), ok ? "state saved" : "error");
            break;
        }
        case 9: {
            FURI_LOG_I("FlipGB", "state load: begin");
            bool ok = load_state(app, storage, ss_path);
            FURI_LOG_I("FlipGB", "state load: %s", ok ? "ok" : "FAILED");
            snprintf(app->status_msg, sizeof(app->status_msg), ok ? "state loaded" : "no state");
            break;
        }
        case 10:
            if(!app->has_battery) {
                snprintf(app->status_msg, sizeof(app->status_msg), "no battery");
            } else if(!app->cart_ram_size) {
                snprintf(app->status_msg, sizeof(app->status_msg), "no RAM");
            } else {
                bool ok = save_sram(app, storage, sav_path);
                snprintf(app->status_msg, sizeof(app->status_msg), ok ? "saved!" : "error");
            }
            break;
        case 11:
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

    app->cart_type = rc->bank0[0x147];
    MBCType mbc = Cartridge::parse_mbc(rc->bank0[0x147]);
    if(mbc == MBCType::Unsupported) return RomLoadBadMapper;

    memcpy(app->rom_title, &rc->bank0[0x134], 16);
    app->rom_title[16] = 0;
    for(int i = 0; i < 16; i++) {
        char ch = app->rom_title[i];
        if(ch != 0 && (ch < 0x20 || ch > 0x7E)) app->rom_title[i] = 0;
    }

    rc->banks = (u16)Cartridge::rom_bank_count_from_header(rc->bank0[0x148]);
    app->has_battery = Cartridge::has_battery(rc->bank0[0x147]);

    /* MBC1 multicart (MBC1M) detection: 1MB+ MBC1 carts with a second
     * Nintendo logo at bank 0x10 use different bank-register wiring */
    if(mbc == MBCType::MBC1 && rc->banks >= 64) {
        u8 logo2[48];
        storage_file_seek(rc->file, 0x40104, true);
        if(storage_file_read(rc->file, logo2, 48) == 48 &&
           memcmp(logo2, &rc->bank0[0x104], 48) == 0) {
            mbc = MBCType::MBC1M;
        }
    }
    *out_mbc = mbc;

    /* cartridge RAM */
    app->cart_ram_size = Cartridge::ram_size_from_header(rc->bank0[0x149], mbc);
    if(app->cart_ram_size) {
        app->cart_ram = (u8*)safe_malloc(app->cart_ram_size);
        if(!app->cart_ram) return RomLoadNoMem;
        memset(app->cart_ram, 0, app->cart_ram_size);
    }

    /* Reserve the emulator core RIGHT NOW as a real allocation instead of
     * a pessimistic estimate: every byte of over-reservation here used to
     * cost cache slots (Pokemon got 2-3 slots for a 1 MB ROM). The block
     * is placement-constructed later. */
    app->gb_mem = safe_malloc(sizeof(Gameboy));
    if(!app->gb_mem) return RomLoadNoMem;

    /* Allocate as many 8 KB page slots as the heap safely affords. Each
     * slot is its own allocation: no huge contiguous block is ever
     * requested, which makes the loader immune to heap fragmentation.
     * HEAP_RESERVE stays free for the GUI takeover + system services. */
    u32 switchable = (u32)(rc->banks - 1) * 4; /* 4 KB units past bank 0 */
    const size_t reserve = HEAP_RESERVE;

    /* slot bookkeeping arrays (a few bytes per slot) */
    size_t free_heap = memmgr_get_free_heap();
    u32 max_slots =
        (u32)((free_heap > HEAP_RESERVE ? free_heap - HEAP_RESERVE : 0) / UNIT_SIZE) + 1;
    if(max_slots > switchable) max_slots = switchable;
    if(max_slots < 4) max_slots = 4;

    rc->slots = (u8**)safe_malloc(max_slots * sizeof(u8*));
    rc->slot_unit = (u16*)safe_malloc(max_slots * sizeof(u16));
    rc->slot_use = (u32*)safe_malloc(max_slots * sizeof(u32));
    rc->slot_hits = (u16*)safe_malloc(max_slots * sizeof(u16));
    if(!rc->slots || !rc->slot_unit || !rc->slot_use || !rc->slot_hits) return RomLoadNoMem;

    /* Reserve ladder: try to keep the full reserve, but a big cartridge
     * (1 MB + 32 KB SRAM) may not reach the 2-slot minimum with it. All
     * SD writes are heap-guarded and happen paused, so degrading the
     * play-time reserve beats refusing to load. */
    static const size_t reserve_ladder[3] = {HEAP_RESERVE, 8u * 1024u, 6u * 1024u};
    rc->num_slots = 0;
    for(uint li = 0; li < 3; li++) {
        size_t res = reserve_ladder[li];
        if(res > reserve) res = reserve;
        while((u32)rc->num_slots < max_slots) {
            if(memmgr_get_free_heap() < res + UNIT_SIZE + ALLOC_MARGIN) break;
            u8* slot = (u8*)safe_malloc(UNIT_SIZE);
            if(!slot) break;
            rc->slots[rc->num_slots] = slot;
            rc->slot_unit[rc->num_slots] = 0; /* unit 0 never lives here */
            rc->slot_use[rc->num_slots] = 0;
            rc->slot_hits[rc->num_slots] = 0;
            rc->num_slots++;
        }
        if(rc->num_slots >= 4) break;
        if(li < 2)
            FURI_LOG_W(
                "FlipGB",
                "degrading heap reserve to %uK for cache slots",
                (unsigned)(reserve_ladder[li + 1] / 1024u));
    }

    /* fewer than four 4 KB slots: fail gracefully with the "Not enough
     * RAM" dialog instead of crashing inside malloc(). Four is the hard
     * minimum: the cartridge maps up to four lazy quarters and the
     * provider protects the three most recently used slots plus the new
     * fill from eviction. 16 KB is always affordable in practice. */
    if(rc->num_slots < 4) return RomLoadNoMem;

    if((u32)rc->num_slots >= switchable) {
        /* every switchable page fits: preload the whole ROM and close the
         * SD file (O(1) bank switching, zero stutter, frees the handle) */
        storage_file_seek(rc->file, BANK_SIZE, true);
        for(u32 i = 0; i < switchable; i++) {
            size_t got = storage_file_read(rc->file, rc->slots[i], UNIT_SIZE);
            if(got < UNIT_SIZE) memset(rc->slots[i] + got, 0xFF, UNIT_SIZE - got);
            rc->slot_unit[i] = (u16)(i + 4);
        }
        rc->fully_loaded = true;
        storage_file_close(rc->file);
        storage_file_free(rc->file);
        rc->file = NULL;
    } else {
        /* Streamed ROM: pre-fill every slot with units 4,5,6,... in one
         * sequential burst (the file position is already there after the
         * bank0 read). These are the first switchable banks -- what the
         * intro executes -- so each converts an in-game cold miss into
         * cheap sequential load time. */
        storage_file_seek(rc->file, 4u * UNIT_SIZE, true); /* defensive no-op */
        for(u16 i = 0; i < rc->num_slots; i++) {
            size_t got = storage_file_read(rc->file, rc->slots[i], UNIT_SIZE);
            if(got < UNIT_SIZE) memset(rc->slots[i] + got, 0xFF, UNIT_SIZE - got);
            rc->slot_unit[i] = (u16)(i + 4);
            rc->slot_use[i] = ++rc->use_counter; /* ascending eviction order */
            rc->slot_hits[i] = 0;
        }
    }

    return RomLoadOk;
}

static void rom_free(AppState* app) {
    RomCache* rc = &app->rom;
    for(u16 i = 0; i < rc->num_slots; i++)
        if(rc->slots[i]) free(rc->slots[i]);
    if(rc->slots) free(rc->slots);
    if(rc->slot_unit) free(rc->slot_unit);
    if(rc->slot_use) free(rc->slot_use);
    if(rc->slot_hits) free(rc->slot_hits);
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
    app->volume_setting = 4;
    g_app = app;

    init_scale_maps();

    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    DialogsApp* dialogs = (DialogsApp*)furi_record_open(RECORD_DIALOGS);

    FuriString* rom_path = furi_string_alloc_set_str("/ext");
    FuriString* sav_path = furi_string_alloc();
    FuriString* ss_path = furi_string_alloc();

    Gui* gui = NULL;
    Canvas* canvas = NULL;
    bool fb_cb_added = false;

    do {
        /* --- pick a ROM with the stock file browser (normal GUI mode) --- */
        DialogsFileBrowserOptions browser_options;
        /* "*": DMG-compatible ROMs also ship as .gbc; header validation
         * rejects anything that isn't a usable ROM anyway */
        dialog_file_browser_set_basic_options(&browser_options, "*", NULL);
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
        ss_path_for_rom(rom_path, ss_path);

        /* --- construct the emulator --- */
        app->fb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!app->fb_mutex) break;

        /* rom_load pre-allocated this block (so the ROM cache could size
         * itself against real free heap, not an estimate) */
        furi_check(app->gb_mem != NULL);
        app->gb = new(app->gb_mem) Gameboy(
            app->rom.bank0,
            app->rom.banks,
            mbc,
            app->cart_ram,
            app->cart_ram_size,
            rom_bank_provider,
            &app->rom);
        app->gb->set_frame_callback(frame_callback, app);

        /* MBC3 RTC (cart types 0x0F/0x10): wall clock + .sav persistence */
        if(app->cart_type == 0x0F || app->cart_type == 0x10) {
            app->gb->cartridge.set_rtc_present(true);
            app->gb->cartridge.set_rtc_provider(rtc_now_provider);
        }

        config_load(app, storage);
        app->gb->set_row_mask(app->scale_mode ? s_rowmask_crop : s_rowmask_fit);
        load_sram(app, storage, sav_path); /* after gb: RTC footer needs it */

        /* --- sound: probe the speaker once; acquisition is per-burst --- */
        app->speaker_available = furi_hal_speaker_acquire(50);
        if(app->speaker_available) furi_hal_speaker_release();
        if(!app->speaker_available) app->volume_setting = 0;

        /* --- take over the display --- */
        gui = (Gui*)furi_record_open(RECORD_GUI);
        if(!gui) break;
        app->gui = gui;

        gui_add_framebuffer_callback(gui, framebuffer_commit_callback, app);
        fb_cb_added = true;

        canvas = gui_direct_draw_acquire(gui);
        if(!canvas) break;
        app->canvas = canvas;

        /* --- main loop --- */
        uint8_t last_applied_keys = 0;
        uint64_t next_frame_us = 0;
        uint32_t epoch_tick = furi_get_tick();
        app->emu_us_ema = GB_FRAME_US;

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

                /* battery-RAM autosave at pause time: the game is frozen
                 * anyway, so even a slow SD write is invisible */
                if(app->has_battery && app->cart_ram_size && app->gb->cartridge.ram_dirty()) {
                    FURI_LOG_I("FlipGB", "autosave sram: begin");
                    bool ok = save_sram(app, storage, sav_path);
                    FURI_LOG_I("FlipGB", "autosave sram: %s", ok ? "ok" : "FAILED");
                    if(ok) app->gb->cartridge.clear_ram_dirty();
                }

                menu_draw(app);

                while(app->menu_active && !app->exit_requested) {
                    input_poll(app);
                    if(!menu_tick(app, storage, sav_path, ss_path)) {
                        app->menu_active = false;
                    }
                    furi_delay_ms(16); /* 33 ms could miss a quick tap */
                }
                /* the A/B press that operated the menu must not reach the
                 * game; the request latch is cleared so held Up+Down (or
                 * menu-navigation rollover) can't instantly reopen it */
                app->keys_blocked = (uint8_t)(app->keys & (KBIT_A | KBIT_B));
                app->menu_requested = false;
                next_frame_us = 0;
                epoch_tick = furi_get_tick();
                continue;
            }

            /* ------ decide frameskip ------ */
            int skip_n = app->frameskip_setting >= 0 ? app->frameskip_setting : app->auto_skip;
            bool render_this = app->skip_phase == 0;
            app->skip_phase = (app->skip_phase + 1) % (skip_n + 1);

            /* ------ run one emulated frame ------ */
            input_poll(app);
            apply_input(app, &last_applied_keys);
            app->gb->set_skip_render(!render_this);

            /* microsecond emulation timing via the DWT cycle counter:
             * millisecond ticks gave +-6% noise right at the frameskip
             * threshold, making auto-skip flap between 60 and 30 fps */
            uint32_t cyc0 = furi_hal_cortex_timer_get(0).start;
            app->gb->run_to_vblank();
            uint32_t emu_us = (furi_hal_cortex_timer_get(0).start - cyc0) /
                              furi_hal_cortex_instructions_per_microsecond();

            /* refresh the piezo with this frame's dominant APU voice */
            sound_update(app, false);

            /* long-press OK Start injection (request set by input thread) */
            if(app->okhold_request) {
                app->okhold_request = false;
                if(app->inject_start_frames == 0) {
                    app->gb->button_pressed(GbButton::Start);
                    app->inject_start_frames = 8;
                }
            }

            /* SD miss diagnostics (EMA of misses per frame, x16) */
            {
                uint32_t misses = app->rom.miss_count - app->last_miss_count;
                app->last_miss_count = app->rom.miss_count;
                int32_t md = (int32_t)(misses << 4) - (int32_t)app->miss_ema;
                app->miss_ema = (uint32_t)((int32_t)app->miss_ema + md / 8);
            }

            /* NOTE: no mid-gameplay SD writes. The battery-RAM autosave
             * happens when the menu opens (game paused): an SD stall
             * during a write used to freeze the whole app in the middle
             * of gameplay -- while paused, a stall is invisible. */

            /* EMA of the emulation cost (signed math: unsigned underflow
             * used to blow it up and pin the frameskip at maximum) */
            int32_t ema_diff = (int32_t)emu_us - (int32_t)app->emu_us_ema;
            app->emu_us_ema = (uint32_t)((int32_t)app->emu_us_ema + ema_diff / 8);

            /* auto-frameskip with hysteresis + dwell: the old memoryless
             * formula flapped between skip 0 and 1 when a game hovered
             * near the 16.7 ms budget -- the most visible judder there is.
             * A change now needs 20 consecutive frames of sustained
             * disagreement and a 1.5 ms deadband. */
            {
                uint32_t ema = app->emu_us_ema;
                int desired;
                if(ema <= GB_FRAME_US + 1500)
                    desired = 0;
                else
                    desired = (int)((ema - 1500) / GB_FRAME_US);
                if(desired > 8) desired = 8;
                if(desired == app->auto_skip) {
                    app->skip_dwell = 0;
                } else if(++app->skip_dwell >= 20) {
                    app->auto_skip += (desired > app->auto_skip) ? 1 : -1;
                    app->skip_dwell = 0;
                }
            }

            /* ------ pacing: absolute 59.73 Hz grid ------
             * furi_delay_until_tick() pins frame starts to an absolute
             * schedule: the old relative sleep accumulated 1 ms tick
             * quantization into +-1.5 ms frame-to-frame jitter -- exactly
             * the "Tetris feels less smooth than Mario" complaint. */
            next_frame_us += GB_FRAME_US;
            uint32_t target_tick = epoch_tick + (uint32_t)((next_frame_us + 500) / 1000);
            uint32_t now_tick = furi_get_tick();
            int32_t ahead = (int32_t)(target_tick - now_tick);
            bool commit_pending = render_this && !app->menu_active && app->fb_dirty;
            bool slept = false;

            if(ahead > 4 * 17) {
                /* long stall (SD, menu): re-anchor instead of fast-forward */
                epoch_tick = now_tick;
                next_frame_us = 0;
            } else if(ahead > 0) {
                if(commit_pending && app->auto_skip > 0) {
                    /* already skipping: don't add latency */
                    canvas_commit(canvas);
                    app->fb_dirty = false;
                    commit_pending = false;
                }
                furi_delay_until_tick(target_tick);
                slept = true;
            }
            if(commit_pending) {
                /* full-speed path: present on the pacing grid, so scene-
                 * dependent emulation time doesn't jitter the display */
                canvas_commit(canvas);
                app->fb_dirty = false;
            }

            /* MANDATORY yield: the firmware's input service makes an
             * unbounded wait on the priority-2 timer daemon on every key
             * release; a never-sleeping priority-16 loop starves it and
             * input dies system-wide. One tick per frame guarantees the
             * daemon always drains. (~1 ms / 17 ms = ~6% worst case,
             * only paid when emulation is at/over budget.) */
            if(!slept) furi_delay_ms(1);
        }

        /* auto-save battery RAM on exit (skipped if nothing changed since
         * the last autosave) */
        if(app->has_battery && app->gb && app->gb->cartridge.ram_dirty()) {
            FURI_LOG_I("FlipGB", "exit sram save: begin");
            save_sram(app, storage, sav_path);
            FURI_LOG_I("FlipGB", "exit sram save: done");
        }
        if(app->gb) config_save(app, storage);
    } while(false);

    /* --- teardown --- */
    if(app->speaker_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        app->speaker_acquired = false;
    }
    if(fb_cb_added) gui_remove_framebuffer_callback(gui, framebuffer_commit_callback, app);
    wait_inflight_zero(&s_fb_cb_inflight);

    if(gui) {
        if(canvas) gui_direct_draw_release(gui);
        furi_record_close(RECORD_GUI);
    }

    if(app->gb) {
        app->gb->~Gameboy(); /* placement-new counterpart */
        free(app->gb);
        app->gb_mem = NULL;
    } else if(app->gb_mem) {
        free(app->gb_mem); /* rom_load reserved it but construction never ran */
    }
    rom_free(app);
    if(app->cart_ram) free(app->cart_ram);
    if(app->fb_mutex) furi_mutex_free(app->fb_mutex);

    furi_string_free(rom_path);
    furi_string_free(sav_path);
    furi_string_free(ss_path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(app);
    g_app = NULL;

    return 0;
}
