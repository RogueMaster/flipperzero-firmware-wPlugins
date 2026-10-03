// Stack Attack for Flipper Zero: the Siemens C45 classic, pixel for pixel.
#include "game.h"
#include "render.h"

#include <furi.h>
#include <furi_hal_light.h>
#include <furi_hal_random.h>
#include <furi_hal_vibro.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dolphin/dolphin.h>

#define HISCORE_PATH APP_DATA_PATH("hiscore.bin")

typedef enum {
    ModeOriginal,
    ModeFast,
    ModeCount,
} Mode;

// Ticks per second and starting cranes for each mode. Original matches the phone (15 fps).
static const uint32_t mode_hz[ModeCount] = {15, 20};
static const uint8_t mode_cranes[ModeCount] = {1, 2};

// Sounds of the real Siemens C45 (square-wave beeper), measured from the clips that
// eaun01re/stktk cut out of the C45 gameplay video. Hz / ms.
// The Flipper's piezo sounds shriller than the phone did at the same pitch, so every
// tone is played PITCH_DIV times lower (4 = two octaves; 1 = as measured).
#define PITCH_DIV 4
#define TONE(hz, ms)                                                           \
    &(const NotificationMessage){                                              \
        .type = NotificationMessageTypeSoundOn,                                \
        .data.sound = {.frequency = (float)(hz) / PITCH_DIV, .volume = 1.0f}}, \
        &(const NotificationMessage) {                                         \
        .type = NotificationMessageTypeDelay, .data.delay.length = ms          \
    }
#define REST(ms)                                                      \
    &message_sound_off, &(const NotificationMessage) {                \
        .type = NotificationMessageTypeDelay, .data.delay.length = ms \
    }

static const NotificationSequence snd_jump = {TONE(4220, 140), &message_sound_off, NULL};
static const NotificationSequence snd_land = {TONE(1940, 150), &message_sound_off, NULL};
static const NotificationSequence snd_push = {
    TONE(1770, 70),
    TONE(1640, 80),
    REST(280),
    TONE(1770, 80),
    TONE(1850, 75),
    &message_sound_off,
    NULL};
static const NotificationSequence snd_blow =
    {TONE(1640, 130), TONE(1940, 135), &message_sound_off, NULL};
static const NotificationSequence snd_score = {
    TONE(1850, 10),
    TONE(4180, 55),
    REST(145),
    TONE(1770, 145),
    TONE(1980, 70),
    TONE(4690, 35),
    TONE(3920, 15),
    TONE(4690, 35),
    TONE(3920, 10),
    TONE(4690, 45),
    &message_sound_off,
    NULL};
static const NotificationSequence snd_over = {
    TONE(1810, 155),
    TONE(1680, 70),
    TONE(1980, 70),
    TONE(1850, 95),
    REST(50),
    TONE(1770, 310),
    &message_sound_off,
    NULL};

typedef enum {
    ScreenTitle,
    ScreenPlay,
    ScreenPause,
    ScreenOver,
    ScreenOptions,
} Screen;

typedef enum {
    OptSound,
    OptLed,
    OptVibro,
    OptCount,
} Option;

// Haptics built from Momentum's "vibro on keypress" pulses: level 1 = 13 ms, 2 = 16 ms,
// 3 = 19 ms of motor on.
#define VIBRO_LAND_MS  13
#define VIBRO_BLAST_MS 19
#define VIBRO_HIT_MS   60 // a box on the helmet: one firm thud
#define VIBRO_BREAK_MS 16 // the helmet smashes a box: a small knock

// LED effects, most important first; a weaker one never cuts a stronger one short.
typedef enum {
    FxNone,
    FxBreak, // one orange flash when the helmet smashes a box
    FxBlast, // a random colour per exploding box
    FxDeath, // flickering red fading out over the game-over jingle
} Fx;

typedef enum {
    EventTick,
    EventInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    FuriTimer* timer;
    NotificationApp* notify;

    Screen screen;
    Mode mode;
    uint8_t title_sel; // 0 Original, 1 Fast, 2 Options
    bool opt[OptCount]; // sound, LED, vibration
    uint8_t opt_sel;
    Screen opt_return; // where Back leaves the options screen to
    uint8_t pause_sel;
    Game game;
    uint8_t held;
    uint8_t pressed;
    uint32_t hiscore[ModeCount];
    bool new_record;
    uint16_t over_ticks;
    uint32_t sound_until; // game tick when the current sound ends
    FuriTimer* led_timer; // steps the current LED effect
    FuriTimer* vibro_timer; // one-shot, ends a vibro pulse
    volatile uint8_t led_steps;
    uint8_t led_total;
    volatile uint8_t fx;
    uint8_t led_color;
    uint8_t fb[FB_SIZE];
} App;

// Save file: high scores, then option flags (older files stop after the scores or the
// sound flag; missing flags mean "on").
typedef struct {
    uint32_t hiscore[ModeCount];
    uint8_t sound_off;
    uint8_t led_off;
    uint8_t vibro_off;
} SaveData;

static void save_load(App* app) {
    for(int i = 0; i < OptCount; i++)
        app->opt[i] = true;
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, HISCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveData d = {0};
        size_t n = storage_file_read(f, &d, sizeof(d));
        if(n >= sizeof(d.hiscore)) memcpy(app->hiscore, d.hiscore, sizeof(d.hiscore));
        // older versions wrote the unused bytes after sound_off as zero, so they read as "on"
        if(n >= offsetof(SaveData, sound_off) + 1) app->opt[OptSound] = !d.sound_off;
        if(n >= offsetof(SaveData, led_off) + 1) app->opt[OptLed] = !d.led_off;
        if(n >= offsetof(SaveData, vibro_off) + 1) app->opt[OptVibro] = !d.vibro_off;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void save_store(App* app) {
    SaveData d = {
        .sound_off = !app->opt[OptSound],
        .led_off = !app->opt[OptLed],
        .vibro_off = !app->opt[OptVibro],
    };
    memcpy(d.hiscore, app->hiscore, sizeof(d.hiscore));
    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(st, STORAGE_APP_DATA_PATH_PREFIX);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, HISCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, &d, sizeof(d));
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// canvas_draw_xbm pushes every pixel through u8g2 one by one (25 ms for a full screen,
// measured). A 1 px tall box is one call, so draw each run of black pixels (~9 ms).
static void draw_fb(Canvas* canvas, const uint8_t* fb) {
    for(int y = 0; y < FB_H; y++) {
        const uint8_t* row = fb + y * FB_STRIDE;
        int start = -1;
        for(int x = 0; x <= FB_W; x++) {
            bool black = x < FB_W && (row[x >> 3] & (1 << (x & 7)));
            if(black && start < 0) {
                start = x;
            } else if(!black && start >= 0) {
                canvas_draw_box(canvas, start, y, x - start, 1);
                start = -1;
            }
        }
    }
}

static void draw_button(Canvas* canvas, int x, int y, int w, const char* label, bool selected) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, w, 12);
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, x, y, w, 12, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, 12, 2);
    }
    canvas_draw_str_aligned(canvas, x + w / 2, y + 6, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

// White box with a double frame over the field, like the phone's popups.
static void draw_popup(Canvas* canvas, int x, int y, int w, int h) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x - 1, y - 1, w + 2, h + 2);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);
    canvas_draw_frame(canvas, x + 1, y + 1, w - 2, h - 2);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(app->screen == ScreenTitle) {
        render_title(app->fb);
        draw_fb(canvas, app->fb);
        canvas_set_font(canvas, FontSecondary);
        draw_button(canvas, 1, 51, 44, "Original", app->title_sel == 0);
        draw_button(canvas, 47, 51, 32, "Fast", app->title_sel == 1);
        draw_button(canvas, 81, 51, 46, "Options", app->title_sel == 2);
    } else if(app->screen == ScreenOptions) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "Options");
        canvas_draw_line(canvas, 0, 14, 127, 14);
        canvas_set_font(canvas, FontSecondary);
        static const char* const names[OptCount] = {"Sound", "LED", "Vibration"};
        for(int i = 0; i < OptCount; i++) {
            int y = 18 + i * 13;
            if(app->opt_sel == i) {
                canvas_draw_rbox(canvas, 4, y, 120, 12, 2);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str_aligned(canvas, 10, y + 6, AlignLeft, AlignCenter, names[i]);
            canvas_draw_str_aligned(
                canvas, 118, y + 6, AlignRight, AlignCenter, app->opt[i] ? "On" : "Off");
            canvas_set_color(canvas, ColorBlack);
        }
    } else {
        render_game(app->fb, &app->game, app->hiscore[app->mode]);
        draw_fb(canvas, app->fb);

        if(app->screen == ScreenPause) {
            draw_popup(canvas, 16, 9, 70, 46);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 51, 17, AlignCenter, AlignCenter, "Pause");
            canvas_set_font(canvas, FontSecondary);
            const char* items[3] = {"Continue", "Options", "Menu"};
            for(int i = 0; i < 3; i++) {
                int y = 25 + i * 10;
                if(app->pause_sel == i) {
                    canvas_draw_box(canvas, 22, y - 1, 58, 10);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_str_aligned(canvas, 51, y + 4, AlignCenter, AlignCenter, items[i]);
                canvas_set_color(canvas, ColorBlack);
            }
        } else if(app->screen == ScreenOver) {
            char buf[16];
            draw_popup(canvas, 12, 18, 78, 24);
            canvas_set_font(canvas, FontPrimary);
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->game.score);
            canvas_draw_str_aligned(canvas, 51, 27, AlignCenter, AlignCenter, buf);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas,
                51,
                36,
                AlignCenter,
                AlignCenter,
                app->new_record ? "New record!" : "Game over");
        }
    }

    furi_mutex_release(app->mutex);
}

static void led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    furi_hal_light_set(LightRed, r);
    furi_hal_light_set(LightGreen, g);
    furi_hal_light_set(LightBlue, b);
}

static void vibro_timer_callback(void* ctx) {
    UNUSED(ctx);
    furi_hal_vibro_on(false);
}

// A short motor pulse, like the firmware's vibro-on-keypress. Direct HAL calls, so the
// pulse never waits behind a sound in the notification queue.
static void vibro_pulse(App* app, uint32_t ms) {
    if(!app->opt[OptVibro]) return;
    furi_hal_vibro_on(true);
    furi_timer_start(app->vibro_timer, furi_ms_to_ticks(ms));
}

static void led_timer_callback(void* ctx) {
    App* app = ctx;
    if(!app->led_steps) return;
    if(--app->led_steps == 0) {
        led_rgb(0, 0, 0);
        app->fx = FxNone;
        return;
    }
    uint8_t step = app->led_total - app->led_steps; // 1, 2, ...
    switch(app->fx) {
    case FxBlast: {
        vibro_pulse(app, VIBRO_BLAST_MS);
        if(!app->opt[OptLed]) break;
        uint8_t c;
        do {
            c = 1 + furi_hal_random_get() % 7; // any mix of red, green, blue except black
        } while(c == app->led_color);
        app->led_color = c;
        led_rgb((c & 4) ? 255 : 0, (c & 2) ? 255 : 0, (c & 1) ? 255 : 0);
        break;
    }
    case FxDeath: {
        // red only, flickering bright/dim and fading out
        uint32_t level = 255 * app->led_steps / app->led_total;
        led_rgb((step & 1) ? level : level / 4, 0, 0);
        break;
    }
    case FxBreak:
        // backlight orange; the green die is much brighter than red, so keep it low
        led_rgb(255, 40, 0);
        break;
    default:
        break;
    }
}

// Runs `steps` effect steps `interval_ms` apart, then switches the LED off.
static void fx_start(App* app, Fx fx, uint8_t steps, uint32_t interval_ms) {
    if(app->led_steps && app->fx > fx) return;
    app->fx = fx;
    app->led_total = steps + 1;
    app->led_steps = steps + 1;
    furi_timer_start(app->led_timer, furi_ms_to_ticks(interval_ms));
    led_timer_callback(app);
}

// Row explosion: 12 steps, one per box, each a LED colour and a light vibro pulse.
static void led_explosion(App* app) {
    if(!app->opt[OptLed] && !app->opt[OptVibro]) return;
    uint32_t total_ms = G_CLEAR_TICKS * 1000 / mode_hz[app->mode];
    fx_start(app, FxBlast, G_COLS, total_ms / G_COLS);
}

static void led_stop(App* app) {
    app->led_steps = 0;
    app->fx = FxNone;
    furi_timer_stop(app->led_timer);
    led_rgb(0, 0, 0);
    furi_timer_stop(app->vibro_timer);
    furi_hal_vibro_on(false);
}

static void open_options(App* app, Screen back) {
    app->opt_return = back;
    app->opt_sel = 0;
    app->screen = ScreenOptions;
}

static void input_callback(InputEvent* input, void* ctx) {
    App* app = ctx;
    AppEvent ev = {.type = EventInput, .input = *input};
    furi_message_queue_put(app->queue, &ev, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    App* app = ctx;
    AppEvent ev = {.type = EventTick};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void start_game(App* app) {
    game_init(&app->game, furi_hal_random_get(), mode_cranes[app->mode]);
    app->held = 0;
    app->pressed = 0;
    app->new_record = false;
    app->sound_until = 0;
    app->screen = ScreenPlay;
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / mode_hz[app->mode]);
    dolphin_deed(DolphinDeedPluginGameStart);
}

static uint8_t key_bit(InputKey key) {
    switch(key) {
    case InputKeyLeft:
        return G_KEY_LEFT;
    case InputKeyRight:
        return G_KEY_RIGHT;
    case InputKeyUp:
    case InputKeyOk:
        return G_KEY_JUMP;
    default:
        return 0;
    }
}

// Returns false when the app should exit.
static bool handle_input(App* app, const InputEvent* in) {
    if(in->key == InputKeyBack && in->type == InputTypeLong) return false;

    switch(app->screen) {
    case ScreenTitle:
        if(in->type != InputTypeShort && in->type != InputTypeRepeat) break;
        if(in->key == InputKeyLeft && app->title_sel > 0) app->title_sel--;
        if(in->key == InputKeyRight && app->title_sel < 2) app->title_sel++;
        if(app->title_sel < ModeCount) app->mode = app->title_sel;
        if(in->key == InputKeyOk) {
            if(app->title_sel == 2)
                open_options(app, ScreenTitle);
            else
                start_game(app);
        }
        if(in->key == InputKeyBack) return false;
        break;

    case ScreenOptions:
        if(in->type != InputTypeShort && in->type != InputTypeRepeat) break;
        if(in->key == InputKeyUp && app->opt_sel > 0) app->opt_sel--;
        if(in->key == InputKeyDown && app->opt_sel < OptCount - 1) app->opt_sel++;
        if(in->type == InputTypeShort &&
           (in->key == InputKeyOk || in->key == InputKeyLeft || in->key == InputKeyRight)) {
            app->opt[app->opt_sel] = !app->opt[app->opt_sel];
            save_store(app);
            if(app->opt_sel == OptVibro && app->opt[OptVibro]) vibro_pulse(app, VIBRO_BLAST_MS);
        }
        if(in->key == InputKeyBack) app->screen = app->opt_return;
        break;

    case ScreenPlay: {
        uint8_t bit = key_bit(in->key);
        if(in->type == InputTypePress) {
            app->held |= bit;
            app->pressed |= bit;
        } else if(in->type == InputTypeRelease) {
            app->held &= ~bit;
        } else if(in->type == InputTypeShort && in->key == InputKeyBack) {
            led_stop(app);
            app->screen = ScreenPause;
            app->pause_sel = 0;
            furi_timer_stop(app->timer);
        }
        break;
    }

    case ScreenPause:
        if(in->type != InputTypeShort) break;
        if(in->key == InputKeyUp && app->pause_sel > 0) app->pause_sel--;
        if(in->key == InputKeyDown && app->pause_sel < 2) app->pause_sel++;
        if(in->key == InputKeyBack || (in->key == InputKeyOk && app->pause_sel == 0)) {
            app->held = 0;
            app->screen = ScreenPlay;
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / mode_hz[app->mode]);
        } else if(in->key == InputKeyOk && app->pause_sel == 1) {
            open_options(app, ScreenPause);
        } else if(in->key == InputKeyOk) {
            app->screen = ScreenTitle;
        }
        break;

    case ScreenOver:
        if(in->type != InputTypeShort || app->over_ticks < 10) break;
        if(in->key == InputKeyOk) start_game(app);
        if(in->key == InputKeyBack) app->screen = ScreenTitle;
        break;
    }
    return true;
}

static void handle_tick(App* app) {
    if(app->screen == ScreenOver) {
        if(app->over_ticks < 0xffff) app->over_ticks++;
        return;
    }
    if(app->screen != ScreenPlay) return;

    game_tick(&app->game, app->held, app->pressed);
    app->pressed = 0;

    uint8_t ev = app->game.events;
    // One sound per tick, most important first. Minor sounds are skipped while another
    // is still playing so the notification queue never falls behind the game.
    const NotificationSequence* snd = NULL;
    uint32_t ms = 0;
    bool always = false;
    if(ev & G_EV_DEATH)
        snd = &snd_over, ms = 750, always = true;
    else if(ev & G_EV_CLEAR)
        snd = &snd_score, ms = 580, always = true;
    else if(ev & G_EV_BREAK)
        snd = &snd_blow, ms = 265;
    else if(ev & G_EV_PUSH)
        snd = &snd_push, ms = 585;
    else if(ev & G_EV_JUMP)
        snd = &snd_jump, ms = 140;
    else if(ev & G_EV_WLAND)
        snd = &snd_land, ms = 150;
    uint32_t now = app->game.tick;
    if(snd && app->opt[OptSound] && (always || now >= app->sound_until)) {
        notification_message(app->notify, snd);
        app->sound_until = now + (ms * mode_hz[app->mode] + 999) / 1000;
    }

    if(ev & G_EV_CLEAR)
        led_explosion(app);
    else if(ev & G_EV_LAND)
        vibro_pulse(app, VIBRO_LAND_MS);
    if(ev & G_EV_BREAK) {
        vibro_pulse(app, VIBRO_BREAK_MS);
        if(app->opt[OptLed]) fx_start(app, FxBreak, 1, 100);
    }
    if(ev & G_EV_DEATH) {
        // the box touches the helmet on this very tick: thud now, not after the jingle
        vibro_pulse(app, VIBRO_HIT_MS);
        if(app->opt[OptLed]) fx_start(app, FxDeath, 14, 55); // ~0.75 s, the jingle
        if(app->game.score > app->hiscore[app->mode]) {
            app->hiscore[app->mode] = app->game.score;
            app->new_record = true;
            save_store(app);
        }
        app->over_ticks = 0;
        app->screen = ScreenOver;
    }
}

int32_t stack_attack_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    app->led_timer = furi_timer_alloc(led_timer_callback, FuriTimerTypePeriodic, app);
    app->vibro_timer = furi_timer_alloc(vibro_timer_callback, FuriTimerTypeOnce, app);
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    app->screen = ScreenTitle;
    save_load(app);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, app);
    view_port_input_callback_set(vp, input_callback, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    notification_message(app->notify, &sequence_display_backlight_enforce_on);

    // Title and game-over screens still need ticks for the popup delay.
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / mode_hz[ModeOriginal]);

    bool running = true;
    AppEvent ev;
    while(running) {
        if(furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(ev.type == EventInput)
            running = handle_input(app, &ev.input);
        else
            handle_tick(app);
        furi_mutex_release(app->mutex);
        view_port_update(vp);
    }

    notification_message(app->notify, &sequence_display_backlight_enforce_auto);
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    led_stop(app);
    furi_timer_free(app->led_timer);
    furi_timer_free(app->vibro_timer);
    notification_message(app->notify, &sequence_reset_rgb);
    view_port_enabled_set(vp, false);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
