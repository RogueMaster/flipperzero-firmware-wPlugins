// Lanternfall for Flipper Zero: device shell. Input, timing, sound, saves, and the menus and
// dialogue drawn with Flipper fonts over the 1-bit frame from render.c.
#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_cortex.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include "game.h"
#include "render.h"

#define SAVE_PATH APP_DATA_PATH("save.bin")
#define OPT_PATH  APP_DATA_PATH("settings.bin")
#define TAG       "Lanternfall"
#ifndef LF_AUTOSTART
#define LF_AUTOSTART 0
#endif
#ifdef LF_DEBUG
static int lf_count_ents(const Game* g) {
    int n = 0;
    for(int i = 0; i < MAX_ENT; i++)
        n += g->ents[i].type != E_NONE;
    return n;
}
#endif

_Static_assert(
    (int)K_UP == (int)InputKeyUp && (int)K_RIGHT == (int)InputKeyRight &&
        (int)K_LEFT == (int)InputKeyLeft && (int)K_OK == (int)InputKeyOk &&
        (int)K_BACK == (int)InputKeyBack,
    "key ids");
_Static_assert(
    (int)KT_PRESS == (int)InputTypePress && (int)KT_LONG == (int)InputTypeLong &&
        (int)KT_REPEAT == (int)InputTypeRepeat,
    "input types");

typedef struct {
    Game g;
    uint8_t fb[FB_SIZE]; // front buffer, read by the GUI thread under the mutex
    uint8_t back[FB_SIZE]; // rendered by the app thread without holding the mutex
    FuriMutex* mutex;
    ViewPort* vp;
} App;

// ---------------------------------------------------------------- sound

#define SEQ(name, ...) static const NotificationSequence name = {__VA_ARGS__, NULL}
SEQ(seq_swing,
    &message_note_g6,
    &message_delay_10,
    &message_note_c7,
    &message_delay_10,
    &message_sound_off);
SEQ(seq_hit, &message_note_c4, &message_delay_25, &message_sound_off);
SEQ(seq_crit,
    &message_note_c4,
    &message_delay_10,
    &message_note_c5,
    &message_delay_25,
    &message_sound_off);
SEQ(seq_kill,
    &message_note_c5,
    &message_delay_25,
    &message_note_e5,
    &message_delay_25,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off);
SEQ(seq_hurt,
    &message_red_255,
    &message_vibro_on,
    &message_note_a2,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_100,
    &message_red_0);
SEQ(seq_coin,
    &message_note_b5,
    &message_delay_25,
    &message_note_e6,
    &message_delay_50,
    &message_sound_off);
SEQ(seq_level,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_100,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_250,
    &message_sound_off);
SEQ(seq_thunder,
    &message_vibro_on,
    &message_note_c1,
    &message_delay_100,
    &message_note_a1,
    &message_delay_50,
    &message_vibro_off,
    &message_note_d1,
    &message_delay_250,
    &message_sound_off);
SEQ(seq_buy,
    &message_note_e6,
    &message_delay_25,
    &message_note_a6,
    &message_delay_50,
    &message_sound_off);
SEQ(seq_deny,
    &message_note_d3,
    &message_delay_50,
    &message_note_c3,
    &message_delay_100,
    &message_sound_off);
SEQ(seq_quest,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_50,
    &message_note_e6,
    &message_delay_100,
    &message_sound_off);
SEQ(seq_eat,
    &message_note_e5,
    &message_delay_25,
    &message_note_a5,
    &message_delay_25,
    &message_sound_off);
SEQ(seq_menu, &message_note_c6, &message_delay_10, &message_sound_off);
SEQ(seq_die,
    &message_note_g4,
    &message_delay_100,
    &message_note_e4,
    &message_delay_100,
    &message_note_c4,
    &message_delay_250,
    &message_note_c3,
    &message_delay_500,
    &message_sound_off);

// Hurt and thunder mix sound, vibration and the red LED, so each gets a copy per combination of the three
// settings, built once at start: the notification thread reads a sequence after we hand it over, so these
// must never change while the app runs.
#define MIX_LEN 12
static const NotificationMessage* mix[2][8][MIX_LEN];
static int mix_bits(uint16_t opt) {
    return ((opt & OPT_SOUND) ? 1 : 0) | ((opt & OPT_VIBRO) ? 2 : 0) | ((opt & OPT_LED) ? 4 : 0);
}
static void mix_build(void) {
    const NotificationSequence* src[2] = {&seq_hurt, &seq_thunder};
    for(int s = 0; s < 2; s++)
        for(int b = 0; b < 8; b++) {
            int o = 0;
            for(int i = 0; (*src[s])[i] && o < MIX_LEN - 1; i++) {
                NotificationMessageType t = (*src[s])[i]->type;
                bool snd = t == NotificationMessageTypeSoundOn ||
                           t == NotificationMessageTypeSoundOff;
                bool vib = t == NotificationMessageTypeVibro;
                bool led = t == NotificationMessageTypeLedRed ||
                           t == NotificationMessageTypeLedGreen ||
                           t == NotificationMessageTypeLedBlue;
                if((snd && !(b & 1)) || (vib && !(b & 2)) || (led && !(b & 4))) continue;
                mix[s][b][o++] = (*src[s])[i];
            }
            mix[s][b][o] = NULL;
        }
}

static void play(NotificationApp* n, uint16_t sfx, uint16_t opt) {
    // one sound per tick, most important first
    int mb = mix_bits(opt);
    if(sfx & (SFX_HURT | SFX_THUNDER) && !(sfx & (SFX_DIE | SFX_LEVEL))) {
        if(mb)
            notification_message(
                n, (const NotificationSequence*)&mix[(sfx & SFX_HURT) ? 0 : 1][mb]);
        return;
    }
    if(!(opt & OPT_SOUND)) return;
    if(sfx & SFX_DIE)
        notification_message(n, &seq_die);
    else if(sfx & SFX_LEVEL)
        notification_message(n, &seq_level);
    else if(sfx & SFX_QUEST)
        notification_message(n, &seq_quest);
    else if(sfx & SFX_KILL)
        notification_message(n, &seq_kill);
    else if(sfx & SFX_CRIT)
        notification_message(n, &seq_crit);
    else if(sfx & SFX_HIT)
        notification_message(n, &seq_hit);
    else if(sfx & SFX_BUY)
        notification_message(n, &seq_buy);
    else if(sfx & SFX_COIN)
        notification_message(n, &seq_coin);
    else if(sfx & SFX_EAT)
        notification_message(n, &seq_eat);
    else if(sfx & SFX_DENY)
        notification_message(n, &seq_deny);
    else if(sfx & SFX_SWING)
        notification_message(n, &seq_swing);
    else if(sfx & SFX_MENU)
        notification_message(n, &seq_menu);
}

// ---------------------------------------------------------------- saves

static bool save_write(const char* path, const uint8_t* buf, int len) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(f, buf, len) == (size_t)len;
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static int save_read(const char* path, uint8_t* buf, int cap) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    int n = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING))
        n = (int)storage_file_read(f, buf, cap);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return n;
}

// settings.bin: "LFO", a version byte, then the OPT_* bits (little endian)
static uint16_t opt_read(void) {
    uint8_t b[6];
    if(save_read(OPT_PATH, b, sizeof(b)) == 6 && b[0] == 'L' && b[1] == 'F' && b[2] == 'O' &&
       b[3] == 1)
        return (uint16_t)(b[4] | b[5] << 8) & OPT_ALL;
    return OPT_DEFAULT;
}
static void opt_write(uint16_t opt) {
    uint8_t b[6] = {'L', 'F', 'O', 1, (uint8_t)opt, (uint8_t)(opt >> 8)};
    save_write(OPT_PATH, b, sizeof(b));
}

// ---------------------------------------------------------------- overlays

#define WRAP_W     120
#define PAGE_LINES 3

// Word-wraps text to WRAP_W pixels in the current font. Fills starts/lens, returns line count.
static int wrap(Canvas* c, const char* t, uint16_t* starts, uint8_t* lens, int max) {
    int n = 0, i = 0, len = (int)strlen(t);
    char buf[64];
    while(i < len && n < max) {
        while(t[i] == ' ')
            i++;
        int best = 0, j = i;
        while(j <= len) {
            if(j == len || t[j] == ' ') {
                int l = j - i;
                if(l >= (int)sizeof(buf)) break;
                memcpy(buf, t + i, l);
                buf[l] = 0;
                if(canvas_string_width(c, buf) > WRAP_W && best) break;
                best = l;
                if(j == len) break;
            }
            j++;
        }
        if(!best) best = len - i < 24 ? len - i : 24;
        starts[n] = i, lens[n] = best, n++;
        i += best;
    }
    return n;
}

static void draw_talk(Canvas* c, Game* g) {
    canvas_set_font(c, FontSecondary);
    uint16_t st[24];
    uint8_t ln[24];
    int n = wrap(c, g->dlg_text, st, ln, 24);
    g->dlg_pages = (n + PAGE_LINES - 1) / PAGE_LINES;
    if(g->dlg_pages < 1) g->dlg_pages = 1;
    if(g->dlg_page >= g->dlg_pages) g->dlg_page = g->dlg_pages - 1;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 0, 24, 128, 40);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, 0, 24, 128, 40, 3);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 4, 34, g->dlg_name);
    canvas_draw_line(c, 4, 36, 4 + canvas_string_width(c, g->dlg_name), 36);
    canvas_set_font(c, FontSecondary);
    char buf[64];
    for(int k = 0; k < PAGE_LINES; k++) {
        int li = g->dlg_page * PAGE_LINES + k;
        if(li >= n) break;
        int l = ln[li] < sizeof(buf) - 1 ? ln[li] : (int)sizeof(buf) - 1;
        memcpy(buf, g->dlg_text + st[li], l);
        buf[l] = 0;
        canvas_draw_str(c, 4, 45 + k * 9, buf);
    }
    if((g->tick / 6) & 1) { // "more" arrow
        if(g->dlg_page + 1 < g->dlg_pages)
            canvas_draw_triangle(c, 121, 59, 5, 3, CanvasDirectionTopToBottom);
        else
            canvas_draw_disc(c, 122, 58, 1);
    }
}

static void draw_list(Canvas* c, Game* g) {
    bool pause = g->ui == UI_PAUSE, select = g->ui != UI_HERO;
    int n = game_menu(g, -1, NULL, 0);
    int x = pause ? 30 : 0, w = pause ? 68 : 128, rows = pause ? 5 : 5, y0 = pause ? 2 : 0;
    int h = pause ? 60 : 64;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, x, y0, w, h);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, x, y0, w, h, 3);
    canvas_set_font(c, FontPrimary);
    char buf[40];
    if(g->ui == UI_SHOP)
        snprintf(buf, sizeof(buf), "Merchant   %dc", g->coins);
    else
        snprintf(buf, sizeof(buf), "%s", game_menu_title(g));
    canvas_draw_str_aligned(c, x + w / 2, y0 + 3, AlignCenter, AlignTop, buf);
    canvas_draw_line(c, x + 3, y0 + 13, x + w - 4, y0 + 13);
    canvas_set_font(c, FontSecondary);
    int top = g->cursor - rows + 1;
    if(top < 0) top = 0;
    if(top > n - rows) top = n - rows > 0 ? n - rows : 0;
    for(int k = 0; k < rows && top + k < n; k++) {
        int i = top + k, y = y0 + 16 + k * 9;
        game_menu(g, i, buf, sizeof(buf));
        if(select && i == g->cursor) {
            canvas_draw_rbox(c, x + 2, y, w - 4, 9, 2);
            canvas_set_color(c, ColorWhite);
        }
        char* tab = strchr(buf, '\t'); // "label\tvalue": the value sits on the right
        if(tab) {
            *tab = 0;
            canvas_draw_str_aligned(c, x + w - 6, y + 7, AlignRight, AlignBottom, tab + 1);
        }
        canvas_draw_str(c, x + 5, y + 7, buf);
        canvas_set_color(c, ColorBlack);
    }
    if(n > rows) { // scroll bar
        int bh = (h - 18) * rows / n, by = y0 + 16 + (h - 18 - bh) * top / (n - rows);
        canvas_draw_box(c, x + w - 3, by, 1, bh);
    }
}

#ifdef LF_DEBUG
static volatile uint32_t draw_max_us, draw_n;
#endif

// The firmware's canvas_draw_xbm() draws every pixel, white ones too, one u8g2 line call each: ~28 ms a
// frame on the Flipper, more than half the CPU at 20 fps. Drawing only the black runs costs a box call per
// run (a few hundred to ~2600 a frame).
static void draw_fb(Canvas* c, const uint8_t* fb) {
    for(int y = 0; y < SCR_H; y++) {
        const uint8_t* r = fb + y * (SCR_W / 8);
        int x = 0;
        while(x < SCR_W) {
            uint8_t b = r[x >> 3];
            if(!(x & 7) && b == 0) {
                x += 8;
                continue;
            }
            if(!((b >> (x & 7)) & 1)) {
                x++;
                continue;
            }
            int x0 = x;
            while(x < SCR_W) {
                b = r[x >> 3];
                if(!(x & 7) && b == 0xff)
                    x += 8;
                else if((b >> (x & 7)) & 1)
                    x++;
                else
                    break;
            }
            canvas_draw_box(c, x0, y, x - x0, 1);
        }
    }
}

static Game snap; // the GUI thread draws from these copies, so the game's lock is held only to copy
static uint8_t snap_fb[FB_SIZE];

static void draw_cb(Canvas* c, void* ctx) {
    App* a = ctx;
#ifdef LF_DEBUG
    uint32_t d0 = furi_hal_cortex_timer_get(0).start;
#endif
    // The app thread only holds the mutex briefly; never stall the GUI thread on it.
    if(furi_mutex_acquire(a->mutex, 10) != FuriStatusOk) return;
    memcpy(snap_fb, a->fb, FB_SIZE);
    memcpy(&snap, &a->g, sizeof(Game));
    furi_mutex_release(a->mutex);
    Game* g = &snap;
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    draw_fb(c, snap_fb);
    switch(g->ui) {
    case UI_TALK: {
        draw_talk(c, g);
        // hand the page count measured in Flipper fonts back to the game, if it is still this dialogue
        if(furi_mutex_acquire(a->mutex, 10) == FuriStatusOk) {
            if(a->g.ui == UI_TALK && !strcmp(a->g.dlg_text, g->dlg_text)) {
                a->g.dlg_pages = g->dlg_pages;
                if(a->g.dlg_page >= g->dlg_pages) a->g.dlg_page = g->dlg_pages - 1;
            }
            furi_mutex_release(a->mutex);
        }
        break;
    }
    case UI_PAUSE:
    case UI_BAG:
    case UI_HERO:
    case UI_SHOP:
    case UI_BOARD:
    case UI_SETTINGS:
        draw_list(c, g);
        break;
    }
#ifdef LF_DEBUG
    uint32_t dt = (furi_hal_cortex_timer_get(0).start - d0) / 64;
    if(dt > draw_max_us) draw_max_us = dt;
    draw_n++;
#endif
}

static void input_cb(InputEvent* e, void* ctx) {
    furi_message_queue_put(ctx, e, 0); // never stall the input service if we fall behind
}

// ---------------------------------------------------------------- main

int32_t lanternfall_app(void* p) {
    UNUSED(p);
    App* a = malloc(sizeof(App));
    memset(a, 0, sizeof(App));
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->vp = view_port_alloc();

    static uint8_t buf[SAVE_SIZE];
    int n = save_read(SAVE_PATH, buf, sizeof(buf));
    if(!(n > 0 && game_load(&a->g, buf, n))) game_new(&a->g, furi_hal_random_get());
    a->g.opt = opt_read();
    mix_build();
    a->g.rng ^= furi_hal_random_get() | 1;
    a->g.ui = UI_TITLE;

    FuriMessageQueue* q = furi_message_queue_alloc(16, sizeof(InputEvent));
    view_port_draw_callback_set(a->vp, draw_cb, a);
    view_port_input_callback_set(a->vp, input_cb, q);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, a->vp, GuiLayerFullscreen);
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    bool lit = false; // backlight held on; enforce on/auto calls must pair up

    const uint32_t period = furi_kernel_get_tick_frequency() / FPS;
    uint32_t next = furi_get_tick() + period;
    InputEvent e;
    bool quit = false;
#ifdef LF_DEBUG
    uint32_t tick_max = 0;
#endif
    while(!quit) {
        int32_t wait = (int32_t)(next - furi_get_tick());
        if(wait < 1) wait = 1; // always yield, so a slow frame can't starve other threads
        bool ticked = false;
        if(furi_message_queue_get(q, &e, wait) == FuriStatusOk) {
            furi_mutex_acquire(a->mutex, FuriWaitForever);
#ifdef LF_DEBUG
            if(e.type == InputTypePress) FURI_LOG_I(TAG, "key %d ui %d", e.key, a->g.ui);
#endif
            game_key(&a->g, e.key, e.type);
            furi_mutex_release(a->mutex);
        }
        if((int32_t)(furi_get_tick() - next) >= 0) {
            furi_mutex_acquire(a->mutex, FuriWaitForever);
#ifdef LF_DEBUG
            if(LF_AUTOSTART && a->g.ui == UI_TITLE && a->g.tick == 100) {
                game_key(&a->g, K_OK, KT_PRESS);
                a->g.clock = DAY_LEN * 70 / 100; // straight to night for timing
                a->g.rain = 3, a->g.rain_t = 60000;
                a->g.pl.x = a->g.home_x,
                a->g.pl.y = a->g.home_y; // lanterns: the heaviest scene to draw
#if LF_AUTOSTART == 2
                a->g.clock = DAY_LEN * 20 / 100; // idle roam timing: day, a strong hero, roam
                a->g.rain = 0;
                a->g.lvl = 6, a->g.wpn = 3, a->g.arm = 3, a->g.pl.maxhp = a->g.pl.hp = 60,
                a->g.bread = 9;
                a->g.opt |= OPT_ROAM;
                game_set_idle(&a->g, true, NULL);
#endif
            }
#endif
#ifdef LF_DEBUG
            uint32_t k0 = furi_hal_cortex_timer_get(0).start;
#endif
            game_tick(&a->g);
#ifdef LF_DEBUG
            uint32_t kt = (furi_hal_cortex_timer_get(0).start - k0) / 64;
            if(kt > tick_max) tick_max = kt;
#endif
            furi_mutex_release(a->mutex);
            next += period;
            if((int32_t)(furi_get_tick() - next) > (int32_t)period * 4)
                next = furi_get_tick() + period;
            ticked = true;
        }
        if(ticked) {
            // Only this thread changes the game, so it can render without the lock.
#ifdef LF_DEBUG
            uint32_t t0 = furi_hal_cortex_timer_get(0).start;
#endif
            render(&a->g, a->back);
#ifdef LF_DEBUG
            uint32_t t1 = furi_hal_cortex_timer_get(0).start;
            if(a->g.tick % 20 == 0) {
                FURI_LOG_I(
                    TAG,
                    "tick %lu ui %d idle %d goal %d dark %d ents %d render_us %lu tick_max_us %lu draw_max_us %lu draws %lu stack %lu",
                    (unsigned long)a->g.tick,
                    a->g.ui,
                    a->g.idle,
                    a->g.goal,
                    game_darkness(&a->g),
                    lf_count_ents(&a->g),
                    (unsigned long)((t1 - t0) / 64),
                    (unsigned long)tick_max,
                    (unsigned long)draw_max_us,
                    (unsigned long)draw_n,
                    (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
                tick_max = 0, draw_max_us = 0, draw_n = 0;
            }
#endif
        }
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        if(ticked) memcpy(a->fb, a->back, FB_SIZE);
        uint16_t sfx = a->g.sfx, opt = a->g.opt;
        bool opt_save = a->g.opt_dirty;
        a->g.sfx = 0;
        a->g.opt_dirty = false;
        int len = 0;
        if(a->g.want_save && a->g.ui != UI_TITLE &&
           !LF_AUTOSTART) { // timing builds never touch the save
            len = game_save(&a->g, buf);
            a->g.want_save = false;
        }
        quit = a->g.quit;
        furi_mutex_release(a->mutex);
        if(len) {
#ifdef LF_DEBUG
            FURI_LOG_I(TAG, "saving %d bytes", len);
#endif
            bool ok = save_write(SAVE_PATH, buf, len);
#ifdef LF_DEBUG
            FURI_LOG_I(TAG, "saved %d", ok);
#endif
            UNUSED(ok);
        }
#ifdef LF_DEBUG
        if(sfx) FURI_LOG_I(TAG, "sfx %x", sfx);
#endif
        if(opt_save) opt_write(opt);
        if(!!(opt & OPT_LIGHT) != lit) {
            lit = !lit;
            notification_message(
                notif,
                lit ? &sequence_display_backlight_enforce_on :
                      &sequence_display_backlight_enforce_auto);
        }
        if(sfx) play(notif, sfx, opt);
#ifdef LF_DEBUG
        if(sfx) FURI_LOG_I(TAG, "sfx done");
#endif
        if(ticked) view_port_update(a->vp); // key changes show on the next tick, within 50 ms
    }

    if(lit) notification_message(notif, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, a->vp);
    view_port_free(a->vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);
    furi_mutex_free(a->mutex);
    free(a);
    return 0;
}
