// Grimoire: a pendulum, a sigil, a talisman and a wand for the Flipper Zero.
//
// Every page shares the planetary hour (hours.c): whose hour it is now and
// until when, computed on the device from the clock and Portland's sunrise.
// The sigil press (sigil.c) and the talisman press (talisman.c) are the
// site's engines, ported step for step.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <math.h>
#include "hours.h"
#include "sigil.h"
#include "talisman.h"
#include "wand.h"
#include <toolbox/level_duration.h>
#include <lib/subghz/devices/cc1101_configs.h>

#define TICK_MS 50
#define W       128
#define H       64

// Where the sun is reckoned from, as the site's js/hours.js has it. The Flipper's clock is local time.
static const HoursPlace place = {
    .latitude = 45.52f,
    .longitude = -122.68f,
    .utc_offset = -8 * 60,
    .us_dst = true,
};

typedef enum {
    PagePendulum,
    PageSigil,
    PageTalisman,
    PageWand,
    PageHours,
    PageCount,
} Page;

static const char* const page_names[PageCount] = {"PENDULUM", "SIGIL", "TALISMAN", "WAND", "HOURS"};

typedef enum {
    SwingIdle,
    SwingSearch, // the question is in: the bob wanders before it settles
    SwingAnswer,
    SwingFade,
} SwingState;

typedef enum {
    AnswerYes, // back and forth
    AnswerNo, // side to side
    AnswerAgain, // round and round
} Answer;

typedef enum {
    LayerIntelligence,
    LayerSpirit,
    LayerName,
    LayerCount,
} Layer;

#define TRAIL   14
#define PEND_CX 98
#define PEND_CY 33
#define PEND_R  20

#define SIGIL_CX 92
#define SIGIL_CY 33
#define SIGIL_S  17.0f // pixels per unit
#define ASH_MAX  120
#define WISH_MAX 48
#define NAME_MAX 24

typedef enum {
    WandIdle, // listening
    WandWaiting, // a blessing is out; an answer may come
    WandAnswered,
    WandNoAnswer,
    WandReceived, // someone else's blessing came in; we answer it
    WandRefused, // the region forbids sending here
} WandState;

static const uint32_t wand_freqs[] = {433920000, 315000000, 868350000, 915000000};
#define WAND_FREQS COUNT_OF(wand_freqs)
#define WAND_RING  256
#define WAND_TIP_X 117
#define WAND_TIP_Y 17

#define TAL_X0   86
#define TAL_Y0   13
#define TAL_SIDE 40

typedef struct {
    float x, y, vx, vy, life;
} Ash;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    bool running;
    Page page;
    uint32_t frame;
    bool sound;
    bool speaker_held;
    bool tone_on;

    // the hour
    bool hour_known;
    PlanetaryHour hour;
    HoursTime now;
    uint32_t hour_checked;
    bool table_night;

    // pendulum
    SwingState swing;
    Answer answer;
    uint16_t swing_left;
    float phase;
    float amp;
    float ax, ay; // the pattern: how much of the swing goes each way
    float turn; // 0 = a line, 1 = a circle
    int8_t trail_x[TRAIL];
    int8_t trail_y[TRAIL];
    uint8_t trail_i;
    float last_c;

    // sigil
    char wish[WISH_MAX];
    Sigil sigil;
    bool has_sigil;
    bool charging;
    float charge; // 0..1
    float burn; // -1 until the sigil ignites, then 0..1
    Ash ash[ASH_MAX];
    uint8_t ash_n;
    const char* said;

    // talisman
    Planet tal_planet;
    bool tal_manual; // chosen by hand rather than following the hour
    Layer tal_layer;
    char tal_name[NAME_MAX];
    uint16_t tal_values[NAME_MAX];
    size_t tal_n;

    // wand
    uint8_t wand_freq;
    WandState wand_state;
    uint16_t wand_left;
    WandMessage wand_last;
    bool wand_have_last;
    bool wand_listening;
    bool wand_answer_due;
    uint32_t wand_rx_tick;
    WandPulse wand_pulses[WAND_PULSES];
    size_t wand_pulse_n;
    volatile size_t wand_pulse_i;
    volatile uint32_t wand_ring[WAND_RING]; // level in the top bit, microseconds below
    volatile uint16_t wand_head;
    uint16_t wand_tail;
    WandDecoder wand_dec;

    // text entry
    bool text_done;
    ViewDispatcher* dispatcher;

    // LED, persistent for the notification thread
    NotificationMessage led_msg[3];
    const NotificationMessage* led_seq[5];
    uint32_t led_last;
    uint8_t flash;
} App;

static uint32_t rnd(uint32_t n) {
    return furi_hal_random_get() % n;
}

static float rndf(void) {
    return rnd(10000) / 10000.0f;
}

// ---------------------------------------------------------------- sound

static bool sound_ready(App* app) {
    if(!app->sound || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return false;
    if(!app->speaker_held) {
        if(!furi_hal_speaker_acquire(10)) return false;
        app->speaker_held = true;
    }
    return true;
}

static void sound_tone(App* app, float freq, float volume) {
    if(!sound_ready(app)) return;
    furi_hal_speaker_start(freq, volume);
    app->tone_on = true;
}

static void sound_quiet(App* app) {
    if(app->tone_on) {
        furi_hal_speaker_stop();
        app->tone_on = false;
    }
}

static void sound_click(App* app, float freq) {
    if(!sound_ready(app)) return;
    furi_hal_speaker_start(freq, 0.35f);
    furi_delay_ms(3);
    furi_hal_speaker_stop();
    app->tone_on = false;
}

static void sound_release(App* app) {
    sound_quiet(app);
    if(app->speaker_held) {
        furi_hal_speaker_release();
        app->speaker_held = false;
    }
}

// ---------------------------------------------------------------- LED

static void led_init(App* app) {
    app->led_msg[0].type = NotificationMessageTypeLedRed;
    app->led_msg[1].type = NotificationMessageTypeLedGreen;
    app->led_msg[2].type = NotificationMessageTypeLedBlue;
    for(size_t i = 0; i < 3; i++)
        app->led_seq[i] = &app->led_msg[i];
    app->led_seq[3] = &message_do_not_reset;
    app->led_seq[4] = NULL;
    app->led_last = 0xFFFFFFFF;
}

static void led_update(App* app) {
    uint8_t r, g, b;
    if(app->flash) {
        r = g = b = 255;
    } else if(app->page == PageSigil && app->has_sigil && app->charge > 0.0f) {
        // the charge, as heat
        uint32_t level = 20 + (uint32_t)(app->charge * 235);
        r = level;
        g = (uint8_t)(level * app->charge * 0.6f);
        b = 0;
    } else {
        // a slow violet breath, a little brighter while something works
        bool busy = app->swing != SwingIdle || app->burn >= 0.0f;
        uint32_t t = app->frame / 4 % 20;
        uint32_t breath = t < 10 ? t : 20 - t;
        uint32_t level = 6 + breath * (busy ? 8 : 3);
        r = 150 * level / 255;
        g = 0;
        b = 255 * level / 255;
    }
    uint32_t packed = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if(packed == app->led_last) return;
    app->led_last = packed;
    app->led_msg[0].data.led.value = r;
    app->led_msg[1].data.led.value = g;
    app->led_msg[2].data.led.value = b;
    notification_message(app->notifications, (const NotificationSequence*)&app->led_seq);
}

// ---------------------------------------------------------------- the hour

static void hour_refresh(App* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    app->now.year = dt.year;
    app->now.month = dt.month;
    app->now.day = dt.day;
    app->now.minute = dt.hour * 60 + dt.minute;
    app->hour_known = hours_now(&place, &app->now, &app->hour);
    app->hour_checked = furi_get_tick();
    if(app->hour_known && !app->tal_manual) app->tal_planet = app->hour.planet;
}

static void clock_str(char* buf, size_t size, int16_t minute) {
    minute %= 1440;
    if(minute < 0) minute += 1440;
    snprintf(buf, size, "%02d:%02d", minute / 60, minute % 60);
}

// ---------------------------------------------------------------- text entry
//
// The firmware's keyboard, run in a dispatcher of its own on this thread
// while our view port sleeps. Returns true when something was entered.

static void text_done_callback(void* context) {
    App* app = context;
    app->text_done = true;
    view_dispatcher_stop(app->dispatcher);
}

static bool text_back_callback(void* context) {
    App* app = context;
    UNUSED(app);
    return false; // let the dispatcher stop
}

static bool ask_text(App* app, const char* header, char* buf, size_t size) {
    app->text_done = false;
    view_port_enabled_set(app->view_port, false);
    TextInput* input = text_input_alloc();
    text_input_set_header_text(input, header);
    text_input_set_result_callback(input, text_done_callback, app, buf, size, true);
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, text_back_callback);
    view_dispatcher_add_view(app->dispatcher, 0, text_input_get_view(input));
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->dispatcher, 0);
    view_dispatcher_run(app->dispatcher);
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_dispatcher_free(app->dispatcher);
    app->dispatcher = NULL;
    text_input_free(input);
    view_port_enabled_set(app->view_port, true);
    return app->text_done && buf[0];
}

// ---------------------------------------------------------------- pendulum

static void pendulum_ask(App* app) {
    if(app->swing == SwingSearch || app->swing == SwingAnswer) return;
    app->swing = SwingSearch;
    app->swing_left = 50 + rnd(30);
    uint32_t r = rnd(100);
    app->answer = r < 45 ? AnswerYes : r < 85 ? AnswerNo : AnswerAgain;
}

static void pendulum_step(App* app) {
    float tx, ty, tturn, tamp;
    switch(app->swing) {
    case SwingSearch:
        if(app->swing_left % 10 == 0) {
            float a = rnd(628) / 100.0f;
            app->ax = cosf(a);
            app->ay = sinf(a);
            app->turn = rnd(100) / 100.0f;
        }
        tx = app->ax;
        ty = app->ay;
        tturn = app->turn;
        tamp = 14.0f;
        if(--app->swing_left == 0) {
            app->swing = SwingAnswer;
            app->swing_left = 140;
            app->flash = 2;
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    case SwingAnswer:
        tx = app->answer == AnswerNo ? 1.0f : 0.0f;
        ty = app->answer == AnswerYes ? 1.0f : 0.0f;
        tturn = app->answer == AnswerAgain ? 1.0f : 0.0f;
        tamp = 19.0f;
        if(--app->swing_left == 0) {
            app->swing = SwingFade;
            app->swing_left = 40;
        }
        break;
    case SwingFade:
        tx = app->ax;
        ty = app->ay;
        tturn = app->turn;
        tamp = 3.0f;
        if(--app->swing_left == 0) app->swing = SwingIdle;
        break;
    default:
        tx = 0.6f;
        ty = 0.8f;
        tturn = 0.3f;
        tamp = 3.0f;
        break;
    }
    app->ax += (tx - app->ax) * 0.12f;
    app->ay += (ty - app->ay) * 0.12f;
    app->turn += (tturn - app->turn) * 0.12f;
    app->amp += (tamp - app->amp) * 0.08f;

    app->phase += 0.26f;
    if(app->phase > 6.2832f) app->phase -= 6.2832f;
    float s = sinf(app->phase), c = cosf(app->phase);
    float x = app->amp * (app->ax * s - app->turn * app->ay * c);
    float y = app->amp * (app->ay * s + app->turn * app->ax * c);
    app->trail_i = (app->trail_i + 1) % TRAIL;
    app->trail_x[app->trail_i] = (int8_t)x;
    app->trail_y[app->trail_i] = (int8_t)y;

    // a tick at each end of the swing while it is working
    if(app->swing != SwingIdle && app->last_c * c <= 0.0f && app->amp > 8.0f) {
        sound_click(app, app->swing == SwingAnswer ? 2600.0f : 1900.0f);
    }
    app->last_c = c;
}

// ---------------------------------------------------------------- sigil

static void sigil_clear(App* app) {
    app->has_sigil = false;
    app->charging = false;
    app->charge = 0.0f;
    app->burn = -1.0f;
    app->ash_n = 0;
    app->wish[0] = 0;
}

static void sigil_write(App* app) {
    char wish[WISH_MAX] = "";
    if(!ask_text(app, "The wish, as a sentence", wish, sizeof(wish))) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    sigil_clear(app);
    if(sigil_make(wish, &app->sigil)) {
        strlcpy(app->wish, wish, sizeof(app->wish));
        app->has_sigil = true;
        app->said = NULL;
    } else {
        app->said = "a wish needs letters";
    }
    furi_mutex_release(app->mutex);
}

static void sigil_ignite(App* app) {
    app->burn = 0.0f;
    app->ash_n = 0;
    for(uint16_t i = 0; i < app->sigil.point_count && app->ash_n < ASH_MAX; i += 2) {
        const SigilPoint* p = &app->sigil.points[i];
        float a = atan2f(p->y, p->x) + (rndf() - 0.5f), v = 20.0f + rndf() * 60.0f;
        Ash* ash = &app->ash[app->ash_n++];
        ash->x = p->x * SIGIL_S;
        ash->y = p->y * SIGIL_S;
        ash->vx = cosf(a) * v;
        ash->vy = sinf(a) * v - 30.0f - rndf() * 40.0f;
        ash->life = 0.6f + rndf() * 0.9f;
    }
    app->flash = 3;
    notification_message(app->notifications, &sequence_double_vibro);
}

static void sigil_step(App* app) {
    const float dt = TICK_MS / 1000.0f;
    if(!app->has_sigil) {
        sound_quiet(app);
        return;
    }
    if(app->burn < 0.0f) {
        if(app->charging) {
            app->charge += dt / 4.0f; // four seconds to full
        } else {
            app->charge -= dt / 2.0f;
        }
        app->charge = CLAMP(app->charge, 1.0f, 0.0f);
        if(app->charge > 0.0f) {
            // a tone rising two octaves with the charge
            float f = 220.0f * powf(2.0f, app->charge * 2.0f);
            sound_tone(app, f, 0.08f + app->charge * 0.3f);
        } else {
            sound_quiet(app);
        }
        if(app->charge >= 1.0f) sigil_ignite(app);
    } else {
        app->burn += dt / 1.6f;
        for(uint8_t i = 0; i < app->ash_n; i++) {
            Ash* a = &app->ash[i];
            if(a->life <= 0.0f) continue;
            a->x += a->vx * dt;
            a->y += a->vy * dt;
            a->vy += 40.0f * dt;
            a->life -= dt;
        }
        if(app->burn < 0.4f) {
            sound_tone(app, 300.0f + rnd(2200), 0.3f); // the snap, as a burst of noise
        } else {
            sound_quiet(app);
        }
        if(app->burn >= 1.0f) {
            sigil_clear(app);
            app->said = "it is done. forget it.";
        }
    }
}

// ---------------------------------------------------------------- talisman

static void talisman_write(App* app) {
    char name[NAME_MAX] = "";
    if(!ask_text(app, "A name to trace", name, sizeof(name))) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strlcpy(app->tal_name, name, sizeof(app->tal_name));
    app->tal_n = talisman_latin(name, app->tal_values, NAME_MAX);
    if(app->tal_n) app->tal_layer = LayerName;
    furi_mutex_release(app->mutex);
}

static bool talisman_layer_ok(App* app, Layer layer) {
    switch(layer) {
    case LayerIntelligence:
        return talisman_intelligence(app->tal_planet) != NULL;
    case LayerName:
        return app->tal_n > 0;
    default:
        return true;
    }
}

static void talisman_next_layer(App* app) {
    for(int i = 1; i <= LayerCount; i++) {
        Layer l = (Layer)((app->tal_layer + i) % LayerCount);
        if(talisman_layer_ok(app, l)) {
            app->tal_layer = l;
            return;
        }
    }
}

static void talisman_choose(App* app, int dir) {
    app->tal_manual = true;
    app->tal_planet = (Planet)((app->tal_planet + PlanetCount + dir) % PlanetCount);
    if(!talisman_layer_ok(app, app->tal_layer)) talisman_next_layer(app);
}

// ---------------------------------------------------------------- wand

// In interrupt context: just keep the pulse
static void wand_rx_isr(bool level, uint32_t duration, void* context) {
    App* app = context;
    uint16_t h = app->wand_head;
    app->wand_ring[h] = (duration & 0x7FFFFFFFu) | (level ? 0x80000000u : 0u);
    app->wand_head = (h + 1) % WAND_RING;
}

// In interrupt context: the next pulse to send
static LevelDuration wand_tx_yield(void* context) {
    App* app = context;
    if(app->wand_pulse_i >= app->wand_pulse_n) return level_duration_reset();
    const WandPulse* p = &app->wand_pulses[app->wand_pulse_i++];
    return level_duration_make(p->level, p->duration);
}

static void wand_listen_start(App* app) {
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(wand_freqs[app->wand_freq]);
    app->wand_head = 0;
    app->wand_tail = 0;
    wand_decoder_reset(&app->wand_dec);
    furi_hal_subghz_start_async_rx(wand_rx_isr, app);
    app->wand_listening = true;
}

static void wand_listen_stop(App* app) {
    if(app->wand_listening) {
        furi_hal_subghz_stop_async_rx();
        app->wand_listening = false;
    }
    furi_hal_subghz_idle();
}

static void wand_start(App* app) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    app->wand_state = WandIdle;
    app->wand_left = 0;
    app->wand_answer_due = false;
    wand_listen_start(app);
}

static void wand_stop(App* app) {
    wand_listen_stop(app);
    furi_hal_subghz_sleep();
    sound_quiet(app);
}

// True when the user's region allows transmitting on the selected frequency.
static const char* wand_region_name(void) {
    const char* name = furi_hal_region_get_name();
    return (name && name[0]) ? name : "??";
}

static bool wand_tx_allowed(const App* app) {
    return furi_hal_region_is_frequency_allowed(wand_freqs[app->wand_freq]);
}

// Sends the hour, blocking for about a third of a second. False, without transmitting,
// where the user's region does not allow the selected frequency.
static bool wand_send(App* app, bool answer) {
    if(!wand_tx_allowed(app)) return false;
    WandMessage m = {
        .answer = answer,
        .planet = app->hour_known ? app->hour.planet : PlanetSun,
        .is_day = app->hour_known ? app->hour.is_day : true,
        .index = app->hour_known ? app->hour.index : 1,
        .minute = app->now.minute,
    };
    app->wand_pulse_n = wand_encode(&m, app->wand_pulses);
    app->wand_pulse_i = 0;
    wand_listen_stop(app);
    furi_hal_subghz_set_frequency_and_path(wand_freqs[app->wand_freq]);
    bool ok = furi_hal_subghz_start_async_tx(wand_tx_yield, app);
    if(ok) {
        // start_async_tx refuses on its own too; this is only reached when allowed
        // a sweep up for a blessing, down for an answer, while the frames go out
        for(int i = 0; i < 80 && !furi_hal_subghz_is_async_tx_complete(); i++) {
            sound_tone(app, answer ? 1500.0f - i * 12.0f : 400.0f + i * 14.0f, 0.3f);
            furi_delay_ms(10);
        }
        furi_hal_subghz_stop_async_tx();
    }
    sound_quiet(app);
    furi_hal_subghz_idle();
    wand_listen_start(app);
    return ok;
}

static void wand_cast(App* app) {
    if(app->wand_state == WandWaiting || app->wand_state == WandReceived) return;
    if(wand_send(app, false)) {
        app->wand_state = WandWaiting;
        app->wand_left = 100; // five seconds for an answer
    } else {
        app->wand_state = WandRefused;
        app->wand_left = 60;
    }
}

static void wand_tune(App* app, int dir) {
    uint8_t f = app->wand_freq;
    for(size_t i = 0; i < WAND_FREQS; i++) {
        f = (f + WAND_FREQS + dir) % WAND_FREQS;
        if(furi_hal_subghz_is_frequency_valid(wand_freqs[f])) break;
    }
    if(f == app->wand_freq) return;
    app->wand_freq = f;
    wand_listen_stop(app);
    wand_listen_start(app);
    app->wand_state = WandIdle;
    app->wand_left = 0;
}

static void wand_step(App* app) {
    uint32_t now = furi_get_tick();
    while(app->wand_tail != app->wand_head) {
        uint32_t v = app->wand_ring[app->wand_tail];
        app->wand_tail = (app->wand_tail + 1) % WAND_RING;
        WandMessage m;
        if(!wand_decoder_feed(&app->wand_dec, v >> 31, v & 0x7FFFFFFFu, &m)) continue;
        if((int32_t)(now - app->wand_rx_tick) < 1000) continue; // the same burst, repeated
        app->wand_rx_tick = now;
        app->wand_last = m;
        app->wand_have_last = true;
        app->flash = 2;
        notification_message(app->notifications, &sequence_single_vibro);
        if(m.answer) {
            if(app->wand_state == WandWaiting) {
                app->wand_state = WandAnswered;
                app->wand_left = 80;
            }
        } else {
            app->wand_state = WandReceived;
            app->wand_left = 30;
            app->wand_answer_due = true;
        }
        break;
    }
    if(app->wand_answer_due && app->wand_state == WandReceived && app->wand_left <= 20) {
        app->wand_answer_due = false;
        if(!wand_send(app, true)) {
            // the region does not allow answering here: say so instead
            app->wand_state = WandRefused;
            app->wand_left = 60;
        }
    }
    if(app->wand_left && --app->wand_left == 0) {
        if(app->wand_state == WandWaiting) {
            app->wand_state = WandNoAnswer;
            app->wand_left = 60;
        } else {
            app->wand_state = WandIdle;
        }
    }
    // a soft tick while an answer is awaited, a chime when one comes
    if(app->wand_state == WandWaiting && app->wand_left % 20 == 0) {
        sound_click(app, 1800.0f);
    } else if(app->wand_state == WandAnswered && app->wand_left > 72) {
        sound_tone(app, 660.0f + (80 - app->wand_left) * 60.0f, 0.3f);
    } else if(app->wand_state == WandReceived && app->wand_left > 24) {
        sound_tone(app, 520.0f + (30 - app->wand_left) * 50.0f, 0.3f);
    } else {
        sound_quiet(app);
    }
}

// ---------------------------------------------------------------- drawing

static void draw_tabs(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_box(canvas, 0, 0, W, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 3, 6, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, W - 3, 6, AlignRight, AlignCenter, ">");
    canvas_draw_str_aligned(canvas, W / 2, 6, AlignCenter, AlignCenter, page_names[app->page]);
    for(int i = 0; i < PageCount; i++) {
        int x = W / 2 + 34 + i * 5;
        if(i == (int)app->page) {
            canvas_draw_box(canvas, x - 1, 4, 3, 3);
        } else {
            canvas_draw_dot(canvas, x, 5);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_hour_footer(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    char buf[40];
    if(app->hour_known) {
        char until[8];
        clock_str(until, sizeof(until), app->hour.end);
        snprintf(
            buf,
            sizeof(buf),
            "%s  %s %u  to %s",
            planet_names[app->hour.planet],
            app->hour.is_day ? "day" : "night",
            app->hour.index,
            until);
    } else {
        strlcpy(buf, "no sunrise here today", sizeof(buf));
    }
    canvas_draw_line(canvas, 0, H - 10, W - 1, H - 10);
    canvas_draw_str_aligned(canvas, W / 2, H - 1, AlignCenter, AlignBottom, buf);
}

static void draw_pendulum(Canvas* canvas, App* app) {
    canvas_draw_circle(canvas, PEND_CX, PEND_CY, PEND_R);
    for(int d = -PEND_R + 3; d <= PEND_R - 3; d += 4) {
        canvas_draw_dot(canvas, PEND_CX + d, PEND_CY);
        canvas_draw_dot(canvas, PEND_CX, PEND_CY + d);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, PEND_CX, PEND_CY - PEND_R + 5, AlignCenter, AlignCenter, "Y");
    canvas_draw_str_aligned(canvas, PEND_CX, PEND_CY + PEND_R - 4, AlignCenter, AlignCenter, "Y");
    canvas_draw_str_aligned(canvas, PEND_CX - PEND_R + 4, PEND_CY, AlignCenter, AlignCenter, "N");
    canvas_draw_str_aligned(canvas, PEND_CX + PEND_R - 4, PEND_CY, AlignCenter, AlignCenter, "N");
    for(int k = TRAIL - 1; k > 0; k--) {
        int i = (app->trail_i + TRAIL - k) % TRAIL;
        if(k < TRAIL / 2 || k % 2 == 0) {
            canvas_draw_dot(canvas, PEND_CX + app->trail_x[i], PEND_CY + app->trail_y[i]);
        }
    }
    int bx = PEND_CX + app->trail_x[app->trail_i], by = PEND_CY + app->trail_y[app->trail_i];
    canvas_draw_line(canvas, PEND_CX, PEND_CY, bx, by);
    canvas_draw_disc(canvas, bx, by, 3);

    switch(app->swing) {
    case SwingSearch:
        canvas_draw_str(canvas, 2, 24, "the bob");
        canvas_draw_str(canvas, 2, 34, "searches");
        break;
    case SwingAnswer: {
        const char* word = app->answer == AnswerYes ? "YES" :
                           app->answer == AnswerNo  ? "NO" :
                                                      "AGAIN";
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_rbox(canvas, 2, 21, 60, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 32, 29, AlignCenter, AlignCenter, word);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            2,
            48,
            app->answer == AnswerYes ? "to and fro" :
            app->answer == AnswerNo  ? "side to side" :
                                       "round: ask again");
        break;
    }
    default:
        canvas_draw_str(canvas, 2, 24, "hold a question");
        canvas_draw_str(canvas, 2, 34, "in mind, then OK");
        break;
    }
}

static void draw_sigil(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    if(!app->has_sigil) {
        canvas_draw_str(canvas, 2, 24, app->said ? app->said : "the press is empty");
        canvas_draw_str(canvas, 2, 40, "OK: write a wish");
        // the empty press: a faint ring
        for(int a = 0; a < 360; a += 12) {
            float r = a * 0.0174533f;
            canvas_draw_dot(
                canvas, SIGIL_CX + (int)(cosf(r) * 20), SIGIL_CY - (int)(sinf(r) * 20));
        }
        return;
    }
    float k = app->charge, b = app->burn;
    if(b < 0.0f || b < 0.4f) {
        // the strokes, shaking a little as the charge builds, breaking up as they burn
        int jitter = (int)(k * k * 3.0f);
        for(uint16_t si = 0; si < app->sigil.stroke_count; si++) {
            const SigilStroke* st = &app->sigil.strokes[si];
            if(b >= 0.0f && rnd(100) < (uint32_t)(b * 250)) continue;
            int px = 0, py = 0;
            for(uint16_t i = 0; i < st->count; i++) {
                const SigilPoint* p = &app->sigil.points[st->first + i];
                int x = SIGIL_CX + (int)(p->x * SIGIL_S), y = SIGIL_CY + (int)(p->y * SIGIL_S);
                if(jitter) {
                    x += (int)rnd(jitter * 2 + 1) - jitter;
                    y += (int)rnd(jitter * 2 + 1) - jitter;
                }
                if(i) canvas_draw_line(canvas, px, py, x, y);
                px = x;
                py = y;
            }
        }
        canvas_draw_disc(canvas, SIGIL_CX, SIGIL_CY, 1 + (int)(k * 1.5f));
    }
    if(b < 0.0f && k > 0.0f) {
        // the charge, as a ring filling clockwise from the top
        for(int a = 0; a < (int)(k * 360.0f); a += 4) {
            float r = (a - 90) * 0.0174533f;
            canvas_draw_dot(
                canvas, SIGIL_CX + (int)(cosf(r) * 21), SIGIL_CY + (int)(sinf(r) * 21));
        }
    }
    if(b >= 0.0f) {
        for(uint8_t i = 0; i < app->ash_n; i++) {
            const Ash* a = &app->ash[i];
            if(a->life <= 0.0f) continue;
            if(a->life < 0.5f && (app->frame + i) % 2) continue; // the embers flicker out
            canvas_draw_dot(canvas, SIGIL_CX + (int)a->x, SIGIL_CY + (int)a->y);
        }
    }

    // the letters, in two rows if need be
    char line[12];
    size_t n = strlen(app->sigil.letters);
    for(size_t row = 0; row < 2 && row * 10 < n; row++) {
        size_t len = MIN((size_t)10, n - row * 10);
        for(size_t i = 0; i < len; i++)
            line[i] = app->sigil.letters[row * 10 + i];
        line[len] = 0;
        char spaced[24];
        size_t k2 = 0;
        for(size_t i = 0; i < len; i++) {
            spaced[k2++] = line[i];
            spaced[k2++] = ' ';
        }
        spaced[k2] = 0;
        canvas_draw_str(canvas, 2, 22 + row * 10, spaced);
    }
    if(b >= 0.0f) {
        canvas_draw_str(canvas, 2, 46, "it burns");
    } else if(k > 0.0f) {
        char buf[16];
        snprintf(buf, sizeof(buf), "charging %u%%", (unsigned)(k * 100));
        canvas_draw_str(canvas, 2, 46, buf);
    } else {
        canvas_draw_str(canvas, 2, 46, "hold OK: charge");
    }
}

// A dotted line, for the spirit's trace
static void draw_dotted(Canvas* canvas, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0), steps = MAX(dx, dy);
    if(steps == 0) {
        canvas_draw_dot(canvas, x0, y0);
        return;
    }
    for(int i = 0; i <= steps; i += 2) {
        canvas_draw_dot(canvas, x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps);
    }
}

static void draw_trace(
    Canvas* canvas,
    Planet planet,
    const uint16_t* values,
    size_t count,
    bool dotted,
    bool thick,
    float cell) {
    int xs[32], ys[32];
    bool loop[32];
    size_t n = 0;
    int8_t lc = -1, lr = -1;
    for(size_t i = 0; i < count && n < 32; i++) {
        uint8_t c, r;
        if(!talisman_cell(planet, values[i], &c, &r)) continue;
        xs[n] = TAL_X0 + (int)((c + 0.5f) * cell);
        ys[n] = TAL_Y0 + (int)((r + 0.5f) * cell);
        loop[n] = (c == lc && r == lr);
        lc = c;
        lr = r;
        n++;
    }
    if(n == 0) return;
    canvas_draw_circle(canvas, xs[0], ys[0], 2); // the start: a ring
    for(size_t i = 1; i < n; i++) {
        if(loop[i]) {
            canvas_draw_circle(
                canvas, xs[i] + 2, ys[i] - 2, 1); // the same cell again: a small loop
            continue;
        }
        if(dotted) {
            draw_dotted(canvas, xs[i - 1], ys[i - 1], xs[i], ys[i]);
        } else {
            canvas_draw_line(canvas, xs[i - 1], ys[i - 1], xs[i], ys[i]);
            if(thick) {
                bool steep = abs(ys[i] - ys[i - 1]) > abs(xs[i] - xs[i - 1]);
                canvas_draw_line(
                    canvas,
                    xs[i - 1] + (steep ? 1 : 0),
                    ys[i - 1] + (steep ? 0 : 1),
                    xs[i] + (steep ? 1 : 0),
                    ys[i] + (steep ? 0 : 1));
            }
        }
    }
    if(n > 1) {
        // the end: a bar across the way
        int dx = xs[n - 1] - xs[n - 2], dy = ys[n - 1] - ys[n - 2];
        float len = sqrtf((float)(dx * dx + dy * dy));
        if(len > 0.5f) {
            int bx = (int)(-dy / len * 3), by = (int)(dx / len * 3);
            canvas_draw_line(
                canvas, xs[n - 1] - bx, ys[n - 1] - by, xs[n - 1] + bx, ys[n - 1] + by);
        }
    }
}

static void draw_talisman(Canvas* canvas, App* app) {
    uint8_t n;
    const uint8_t* sq = talisman_square(app->tal_planet, &n);
    float cell = (float)TAL_SIDE / n;
    for(int i = 0; i <= n; i++) {
        int d = (int)(i * cell + 0.5f);
        canvas_draw_line(canvas, TAL_X0 + d, TAL_Y0, TAL_X0 + d, TAL_Y0 + TAL_SIDE);
        canvas_draw_line(canvas, TAL_X0, TAL_Y0 + d, TAL_X0 + TAL_SIDE, TAL_Y0 + d);
    }
    canvas_set_font(canvas, FontSecondary);
    if(n == 3) {
        char num[4];
        for(int i = 0; i < 9; i++) {
            snprintf(num, sizeof(num), "%u", sq[i]);
            canvas_draw_str_aligned(
                canvas,
                TAL_X0 + (int)((i % 3 + 0.5f) * cell),
                TAL_Y0 + (int)((i / 3 + 0.5f) * cell) + 1,
                AlignCenter,
                AlignCenter,
                num);
        }
    }
    const TalismanName* who = NULL;
    char line4[32] = "";
    switch(app->tal_layer) {
    case LayerIntelligence:
        who = talisman_intelligence(app->tal_planet);
        if(who) {
            draw_trace(canvas, app->tal_planet, who->values, who->count, false, false, cell);
            snprintf(
                line4, sizeof(line4), "%s %u", who->name, talisman_sum(who->values, who->count));
        }
        break;
    case LayerSpirit:
        who = talisman_spirit(app->tal_planet);
        draw_trace(canvas, app->tal_planet, who->values, who->count, true, false, cell);
        snprintf(line4, sizeof(line4), "%s %u", who->name, talisman_sum(who->values, who->count));
        break;
    default:
        draw_trace(canvas, app->tal_planet, app->tal_values, app->tal_n, false, true, cell);
        snprintf(
            line4,
            sizeof(line4),
            "%s %u",
            app->tal_name,
            talisman_sum(app->tal_values, app->tal_n));
        break;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 23, planet_names[app->tal_planet]);
    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf), "%ux%u  rows %u", n, n, talisman_row_sum(app->tal_planet));
    canvas_draw_str(canvas, 2, 33, buf);
    snprintf(
        buf,
        sizeof(buf),
        "all %u  %s",
        talisman_total(app->tal_planet),
        talisman_metal(app->tal_planet));
    canvas_draw_str(canvas, 2, 42, buf);
    canvas_draw_str(canvas, 2, 51, line4);
    // which trace: solid, dotted, thick
    const char* kind = app->tal_layer == LayerIntelligence ? "i" :
                       app->tal_layer == LayerSpirit       ? "s" :
                                                             "n";
    canvas_draw_str_aligned(canvas, TAL_X0 - 4, 51, AlignRight, AlignBottom, kind);
}

static void draw_wand(Canvas* canvas, App* app) {
    // the staff, from the lower left up to the tip, and a star at the tip
    canvas_draw_line(canvas, 86, 52, WAND_TIP_X - 2, WAND_TIP_Y + 2);
    canvas_draw_line(canvas, 87, 52, WAND_TIP_X - 1, WAND_TIP_Y + 2);
    canvas_draw_line(canvas, 86, 51, WAND_TIP_X - 2, WAND_TIP_Y + 1);
    canvas_draw_line(canvas, WAND_TIP_X - 3, WAND_TIP_Y, WAND_TIP_X + 3, WAND_TIP_Y);
    canvas_draw_line(canvas, WAND_TIP_X, WAND_TIP_Y - 3, WAND_TIP_X, WAND_TIP_Y + 3);
    canvas_draw_line(canvas, WAND_TIP_X - 2, WAND_TIP_Y - 2, WAND_TIP_X + 2, WAND_TIP_Y + 2);
    canvas_draw_line(canvas, WAND_TIP_X + 2, WAND_TIP_Y - 2, WAND_TIP_X - 2, WAND_TIP_Y + 2);
    bool busy = app->wand_state == WandWaiting || app->wand_state == WandReceived ||
                app->wand_state == WandAnswered;
    if(busy) {
        // rings going out from the tip
        for(int k = 0; k < 3; k++) {
            int r = (app->frame * 2 + k * 8) % 26 + 4;
            for(int a = 90; a <= 270; a += 6) {
                float rad = a * 0.0174533f;
                int x = WAND_TIP_X + (int)(cosf(rad) * r), y = WAND_TIP_Y - (int)(sinf(rad) * r);
                if(x > 72 && y > 12 && y < 53) canvas_draw_dot(canvas, x, y);
            }
        }
    } else if(app->wand_state == WandIdle && (app->frame / 10) % 2) {
        canvas_draw_dot(canvas, WAND_TIP_X - 5, WAND_TIP_Y - 5);
    }

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    char who[24] = "";
    if(app->wand_have_last) {
        snprintf(
            who,
            sizeof(who),
            "%s %s %u",
            planet_names[app->wand_last.planet],
            app->wand_last.is_day ? "day" : "night",
            app->wand_last.index);
    }
    switch(app->wand_state) {
    case WandWaiting:
        canvas_draw_str(canvas, 2, 24, "blessing cast");
        snprintf(buf, sizeof(buf), "an answer? %us", (app->wand_left + 19) / 20);
        canvas_draw_str(canvas, 2, 36, buf);
        break;
    case WandAnswered:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_rbox(canvas, 2, 20, 70, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 37, 28, AlignCenter, AlignCenter, "ANSWERED");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 48, who);
        break;
    case WandNoAnswer:
        canvas_draw_str(canvas, 2, 24, "no answer came");
        canvas_draw_str(canvas, 2, 36, "the air is quiet");
        break;
    case WandReceived:
        canvas_draw_str(canvas, 2, 24, "a blessing came");
        canvas_draw_str(canvas, 2, 36, who);
        canvas_draw_str(canvas, 2, 48, "answering it");
        break;
    case WandRefused:
        snprintf(
            buf,
            sizeof(buf),
            "%lu.%02lu MHz not allowed",
            wand_freqs[app->wand_freq] / 1000000,
            (wand_freqs[app->wand_freq] / 10000) % 100);
        canvas_draw_str(canvas, 2, 24, buf);
        snprintf(buf, sizeof(buf), "in region %s", wand_region_name());
        canvas_draw_str(canvas, 2, 36, buf);
        canvas_draw_str(canvas, 2, 48, "nothing was sent");
        break;
    default:
        snprintf(
            buf,
            sizeof(buf),
            "listening %lu.%02lu",
            wand_freqs[app->wand_freq] / 1000000,
            (wand_freqs[app->wand_freq] / 10000) % 100);
        canvas_draw_str(canvas, 2, 24, buf);
        if(app->wand_have_last) {
            canvas_draw_str(canvas, 2, 36, "last:");
            canvas_draw_str(canvas, 26, 36, who);
        }
        if(wand_tx_allowed(app)) {
            canvas_draw_str(canvas, 2, 48, "OK: cast a blessing");
        } else {
            snprintf(buf, sizeof(buf), "receive only in %s", wand_region_name());
            canvas_draw_str(canvas, 2, 48, buf);
        }
        break;
    }
}

static void draw_stub(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "in the making");
}

static void draw_table(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    if(!app->hour_known) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "no sunrise here today");
        return;
    }
    static const char* const days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const short_names[PlanetCount] = {
        "SAT", "JUP", "MAR", "SUN", "VEN", "MER", "MOON"};
    char head[40], a[8], b[8];
    clock_str(a, sizeof(a), app->hour.sunrise);
    clock_str(b, sizeof(b), app->hour.sunset);
    snprintf(
        head,
        sizeof(head),
        "%s of %s  %s %s",
        app->table_night ? "Night" : "Day",
        planet_names[app->hour.day_ruler],
        app->table_night ? "set" : "rise",
        app->table_night ? b : a);
    canvas_draw_str(canvas, 2, 7, head);
    canvas_draw_str_aligned(canvas, W - 2, 7, AlignRight, AlignBottom, days[app->hour.weekday]);
    canvas_draw_line(canvas, 0, 9, W - 1, 9);

    int16_t from = app->table_night ? app->hour.sunset : app->hour.sunrise;
    int16_t to = app->table_night ? app->hour.next_sunrise : app->hour.sunset;
    float len = (to - from) / 12.0f;
    for(int n = 0; n < 12; n++) {
        int col = n / 6, row = n % 6;
        int x = 2 + col * 64, y = 18 + row * 9;
        char t[8], line[24];
        clock_str(t, sizeof(t), from + (int16_t)(n * len));
        Planet p = hours_planet_of(app->hour.day_ruler, n + (app->table_night ? 12 : 0));
        snprintf(line, sizeof(line), "%2d %s %s", n + 1, t, short_names[p]);
        bool current = app->hour.is_day != app->table_night && app->hour.index == n + 1;
        if(current) {
            canvas_draw_box(canvas, x - 2, y - 8, 62, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, x, y, line);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(app->page == PageHours) {
        draw_table(canvas, app);
    } else {
        draw_tabs(canvas, app);
        switch(app->page) {
        case PagePendulum:
            draw_pendulum(canvas, app);
            break;
        case PageSigil:
            draw_sigil(canvas, app);
            break;
        case PageTalisman:
            draw_talisman(canvas, app);
            break;
        case PageWand:
            draw_wand(canvas, app);
            break;
        default:
            draw_stub(canvas);
            break;
        }
        draw_hour_footer(canvas, app);
    }
    if(app->flash) canvas_invert_color(canvas);
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* context) {
    furi_message_queue_put(context, event, 0);
}

static void handle_input(App* app, const InputEvent* event) {
    // the sigil is charged by holding OK
    if(app->page == PageSigil && event->key == InputKeyOk) {
        if(event->type == InputTypeLong && app->has_sigil && app->burn < 0.0f) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->charging = true;
            furi_mutex_release(app->mutex);
            return;
        }
        if(event->type == InputTypeRelease) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->charging = false;
            furi_mutex_release(app->mutex);
            return;
        }
    }
    if(event->type == InputTypeLong && event->key == InputKeyOk && app->page == PageTalisman) {
        talisman_write(app);
        return;
    }
    if(event->type != InputTypeShort) return;
    switch(event->key) {
    case InputKeyBack:
        app->running = false;
        return;
    case InputKeyLeft:
    case InputKeyRight:
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->page == PageWand) wand_stop(app);
        app->page =
            (Page)((app->page + PageCount + (event->key == InputKeyRight ? 1 : -1)) % PageCount);
        if(app->page == PageHours) app->table_night = app->hour_known && !app->hour.is_day;
        if(app->page == PageWand) wand_start(app);
        sound_quiet(app);
        furi_mutex_release(app->mutex);
        return;
    default:
        break;
    }
    switch(app->page) {
    case PagePendulum:
        if(event->key == InputKeyOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            pendulum_ask(app);
            furi_mutex_release(app->mutex);
        }
        break;
    case PageSigil:
        if(event->key == InputKeyOk && app->burn < 0.0f) sigil_write(app);
        break;
    case PageTalisman:
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event->key == InputKeyOk) {
            talisman_next_layer(app);
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            talisman_choose(app, event->key == InputKeyUp ? 1 : -1);
        }
        furi_mutex_release(app->mutex);
        break;
    case PageHours:
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->table_night = !app->table_night;
            furi_mutex_release(app->mutex);
        }
        break;
    case PageWand:
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event->key == InputKeyOk) {
            wand_cast(app);
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            wand_tune(app, event->key == InputKeyUp ? 1 : -1);
        }
        furi_mutex_release(app->mutex);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- main

static void tick(App* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if((int32_t)(furi_get_tick() - app->hour_checked) >= 1000) hour_refresh(app);
    switch(app->page) {
    case PagePendulum:
        pendulum_step(app);
        break;
    case PageSigil:
        sigil_step(app);
        break;
    case PageWand:
        wand_step(app);
        break;
    default:
        break;
    }
    app->frame++;
    if(app->flash) app->flash--;
    furi_mutex_release(app->mutex);
    led_update(app);
    view_port_update(app->view_port);
}

int32_t grimoire_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->running = true;
    app->sound = true;
    app->ax = 0.6f;
    app->ay = 0.8f;
    app->amp = 3.0f;
    app->burn = -1.0f;
    app->tal_planet = PlanetSun;
    led_init(app);
    hour_refresh(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->queue);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;
    uint32_t next = furi_get_tick();
    while(app->running) {
        uint32_t now = furi_get_tick();
        uint32_t wait = (int32_t)(now - next) >= 0 ? 0 : next - now;
        if(furi_message_queue_get(app->queue, &event, wait) == FuriStatusOk) {
            handle_input(app, &event);
            continue;
        }
        next += TICK_MS;
        if((int32_t)(furi_get_tick() - next) > 200) next = furi_get_tick();
        tick(app);
    }

    if(app->page == PageWand) wand_stop(app);
    sound_release(app);
    notification_message(app->notifications, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
