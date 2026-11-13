#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <lib/toolbox/level_duration.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_TEXT  40
#define MAX_ELEMS 700

// RX decoder parameters
#define RX_TEXT_MAX     120
#define RX_MAX_ELEMS    12
#define RX_HOLD_MS      6 // peak-hold, bridges the gaps of an OOK audio tone
#define RX_MIN_PULSE_MS 15 // shorter pulses are treated as noise
#define RX_WARMUP_MS    300
#define RX_LINE_CHARS   20
#define RX_LINES        3

typedef enum {
    ViewMenu,
    ViewTx,
    ViewSettings,
    ViewText,
    ViewRx,
    ViewAbout
} ViewId;

typedef enum {
    MenuTx,
    MenuRx,
    MenuAbout
} MenuIndex;

typedef enum {
    BandPMR,
    BandLPD
} Band;

typedef struct {
    uint32_t dur_us;
    bool on;
} Elem;

typedef struct MorseApp {
    ViewDispatcher* vd;
    Submenu* menu;
    Widget* about;
    View* main_view; // TX screen
    View* rx_view; // RX screen
    VariableItemList* settings;
    TextInput* text_input;
    NotificationApp* notif;

    VariableItem* item_msg;
    VariableItem* item_band;
    VariableItem* item_chan;
    VariableItem* item_wpm;
    VariableItem* item_mode;
    VariableItem* item_tone;

    // settings (shared by TX and RX)
    Band band;
    uint8_t channel; // 1-based
    uint8_t wpm;
    uint8_t mode; // 0 = FM tone, 1 = OOK tone, 2 = CW OOK
    uint8_t tone_idx;
    char text[MAX_TEXT + 1];

    // transmission
    FuriThread* thread;
    volatile bool abort;
    volatile bool sending;
    volatile uint8_t result; // 0 ok, 1 TX not allowed, 2 error
    Elem elems[MAX_ELEMS];
    volatile size_t count;
    volatile size_t idx;
    uint32_t unit_us;
    uint32_t half_us; // half period of the tone (0 = CW)
    uint32_t rem;
    bool started;
    bool phase;

    // reception
    FuriThread* rx_thread;
    volatile bool rx_run;
    volatile bool rx_retune;
    volatile uint8_t rx_error;
    volatile bool rx_sig;
    volatile int16_t rx_rssi_i;
    volatile int16_t rx_noise_i;
    volatile uint8_t rx_margin; // dB above the noise floor
    volatile uint32_t rx_unit_ms; // estimated dot length
    char rx_text[RX_TEXT_MAX + 2];
    volatile size_t rx_len;
    char rx_pat[RX_MAX_ELEMS + 1];

    char chan_text[32];
    char wpm_text[8];
    char tone_text[12];
    char msg_text[20];
} MorseApp;

typedef struct {
    MorseApp* app;
} MainModel;

static const uint16_t tone_hz[] = {500, 700, 1000};
static const char* band_names[] = {"PMR446", "LPD433"};
// 0 = FM: carrier always on, tone as frequency modulation (squelch stays open)
// 1 = OOK: tone made by switching the carrier on/off
// 2 = CW: fixed carrier, keyed in pulses
static const char* mode_names[] = {"FM tone", "OOK tone", "CW (OOK)"};

/* ---------- Frequencies ---------- */

static uint8_t max_channel(Band b) {
    return (b == BandPMR) ? 16 : 69;
}

static uint32_t channel_freq(Band b, uint8_t ch) {
    if(b == BandPMR) return 446006250UL + 12500UL * (ch - 1); // 446.00625 + 12.5 kHz
    return 433075000UL + 25000UL * (ch - 1); // 433.075 + 25 kHz
}

/* ---------- Morse table ---------- */

typedef struct {
    char c;
    const char* code;
} MorseEntry;

static const MorseEntry morse_table[] = {
    {'A', ".-"},     {'B', "-..."},   {'C', "-.-."},   {'D', "-.."},    {'E', "."},
    {'F', "..-."},   {'G', "--."},    {'H', "...."},   {'I', ".."},     {'J', ".---"},
    {'K', "-.-"},    {'L', ".-.."},   {'M', "--"},     {'N', "-."},     {'O', "---"},
    {'P', ".--."},   {'Q', "--.-"},   {'R', ".-."},    {'S', "..."},    {'T', "-"},
    {'U', "..-"},    {'V', "...-"},   {'W', ".--"},    {'X', "-..-"},   {'Y', "-.--"},
    {'Z', "--.."},   {'0', "-----"},  {'1', ".----"},  {'2', "..---"},  {'3', "...--"},
    {'4', "....-"},  {'5', "....."},  {'6', "-...."},  {'7', "--..."},  {'8', "---.."},
    {'9', "----."},  {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'/', "-..-."},
    {'=', "-...-"},  {'-', "-....-"}, {'+', ".-.-."},  {'@', ".--.-."}, {'(', "-.--."},
    {')', "-.--.-"}, {'\'', ".----."}, {':', "---..."}, {'!', "-.-.--"},
};

static const char* morse_lookup(char c) {
    for(size_t i = 0; i < COUNT_OF(morse_table); i++) {
        if(morse_table[i].c == c) return morse_table[i].code;
    }
    return NULL;
}

static char morse_reverse(const char* pat) {
    for(size_t i = 0; i < COUNT_OF(morse_table); i++) {
        if(strcmp(morse_table[i].code, pat) == 0) return morse_table[i].c;
    }
    return 0;
}

/* ---------- On/off sequence builder (TX) ---------- */

static void push_us(MorseApp* a, bool on, uint32_t d) {
    if(a->count && a->elems[a->count - 1].on == on) {
        a->elems[a->count - 1].dur_us += d;
        return;
    }
    if(a->count < MAX_ELEMS) {
        a->elems[a->count].on = on;
        a->elems[a->count].dur_us = d;
        a->count++;
    }
}

static void push(MorseApp* a, bool on, uint32_t units) {
    push_us(a, on, units * a->unit_us);
}

static void push_code(MorseApp* a, const char* code) {
    for(; *code; code++) {
        push(a, true, (*code == '-') ? 3 : 1);
        if(code[1]) push(a, false, 1);
    }
}

static void prepare(MorseApp* a) {
    a->count = 0;
    a->idx = 0;
    a->unit_us = 1200000UL / a->wpm;
    a->half_us = (a->mode < 2) ? (500000UL / tone_hz[a->tone_idx]) : 0;
    // FM mode: carrier starts before the morse so the squelch has time to open
    if(a->mode == 0) push_us(a, false, 400000UL);
}

static void finish(MorseApp* a) {
    // FM mode: carrier stays on for a moment after the last symbol
    if(a->mode == 0) push_us(a, false, 300000UL);
}

static void build_text(MorseApp* a, const char* s) {
    prepare(a);
    for(; *s; s++) {
        char c = toupper((unsigned char)*s);
        if(c == ' ') {
            push(a, false, 4); // 3 (end of letter) + 4 = 7 units
            continue;
        }
        const char* code = morse_lookup(c);
        if(!code) continue;
        push_code(a, code);
        push(a, false, 3);
    }
    finish(a);
}

static void build_sos(MorseApp* a) {
    prepare(a);
    for(int i = 0; i < 3; i++) {
        push_code(a, "...---..."); // SOS prosign, no gaps between letters
        push(a, false, 7);
    }
    finish(a);
}

/* ---------- Async TX callback (called by the driver) ---------- */

static LevelDuration tx_cb(void* ctx) {
    MorseApp* a = ctx;
    if(a->abort || a->idx >= a->count) return level_duration_reset();

    Elem* e = &a->elems[a->idx];
    if(!a->started) {
        a->rem = e->dur_us;
        a->started = true;
        a->phase = true;
    }

    uint32_t chunk;
    LevelDuration ld;
    if(e->on && a->half_us) {
        // "tone" mode: carrier switched on/off at audio frequency
        chunk = (a->half_us < a->rem) ? a->half_us : a->rem;
        ld = level_duration_make(a->phase, chunk);
        a->phase = !a->phase;
    } else {
        chunk = a->rem;
        ld = level_duration_make(e->on, chunk);
    }

    a->rem -= chunk;
    if(a->rem == 0) {
        a->idx++;
        a->started = false;
    }
    return ld;
}

/* ---------- UI helpers ---------- */

static void redraw_view(View* v) {
    with_view_model(v, MainModel * m, { UNUSED(m); }, true);
}

static void redraw(MorseApp* a) {
    redraw_view(a->main_view);
}

static int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

/* ---------- TX thread ---------- */

static int32_t tx_thread(void* ctx) {
    MorseApp* a = ctx;
    uint32_t f = channel_freq(a->band, a->channel);

    if(!furi_hal_subghz_is_tx_allowed(f)) {
        a->result = 1;
    } else {
        // solid blue LED while transmitting
        notification_message(a->notif, &sequence_set_blue_255);

        subghz_devices_init();
        const SubGhzDevice* dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

        if(!dev) {
            a->result = 2;
        } else {
            subghz_devices_begin(dev);
            subghz_devices_reset(dev);
            subghz_devices_idle(dev);
            subghz_devices_load_preset(
                dev,
                (a->mode == 0) ? FuriHalSubGhzPreset2FSKDev238Async :
                                 FuriHalSubGhzPresetOok650Async,
                NULL);
            subghz_devices_set_frequency(dev, f);

            a->started = false;
            if(subghz_devices_start_async_tx(dev, tx_cb, a)) {
                while(!subghz_devices_is_async_complete_tx(dev) && !a->abort) {
                    furi_delay_ms(20);
                    redraw(a);
                }
                subghz_devices_stop_async_tx(dev);
            } else {
                a->result = 2;
            }
            subghz_devices_sleep(dev);
            subghz_devices_end(dev);
        }
        subghz_devices_deinit();

        // blue LED off
        notification_message(a->notif, &sequence_reset_blue);
    }

    a->sending = false;
    redraw(a);
    return 0;
}

static void start_tx(MorseApp* a, bool sos) {
    if(a->sending) return;
    if(a->thread) {
        furi_thread_join(a->thread);
        furi_thread_free(a->thread);
        a->thread = NULL;
    }
    if(sos) {
        build_sos(a);
    } else {
        build_text(a, a->text);
    }
    if(!a->count) return;

    a->abort = false;
    a->result = 0;
    a->sending = true;
    a->thread = furi_thread_alloc_ex("MorseTx", 2048, tx_thread, a);
    furi_thread_start(a->thread);
    redraw(a);
}

/* ---------- RX decoder ---------- */

static void rx_emit(MorseApp* a, char c) {
    size_t n = a->rx_len;
    if(n >= RX_TEXT_MAX) {
        size_t keep = RX_TEXT_MAX / 2;
        memmove(a->rx_text, a->rx_text + (n - keep), keep);
        n = keep;
    }
    a->rx_text[n] = c;
    a->rx_text[n + 1] = '\0';
    a->rx_len = n + 1;
}

// Turns the pulse durations of one character into dots/dashes.
// If the character holds both short and long pulses they are split on their own midpoint,
// otherwise the current dot-length estimate is used.
static void rx_classify(const uint32_t* d, uint8_t n, uint32_t unit, char* pat, uint32_t* est) {
    uint32_t mn = d[0];
    uint32_t mx = d[0];
    for(uint8_t i = 1; i < n; i++) {
        if(d[i] < mn) mn = d[i];
        if(d[i] > mx) mx = d[i];
    }
    uint32_t thr = (n > 1 && mx * 10 >= mn * 22) ? (mn + mx) / 2 : 2 * unit;
    uint32_t acc = 0;
    for(uint8_t i = 0; i < n; i++) {
        bool dash = d[i] >= thr;
        pat[i] = dash ? '-' : '.';
        acc += dash ? d[i] / 3 : d[i];
    }
    pat[n] = '\0';
    if(est) *est = acc / n;
}

static void rx_flush_char(MorseApp* a, const uint32_t* d, uint8_t n) {
    char pat[RX_MAX_ELEMS + 1];
    uint32_t est = 0;
    rx_classify(d, n, a->rx_unit_ms, pat, &est);

    // adapt speed estimate
    uint32_t u = (a->rx_unit_ms + est) / 2;
    if(u < 25) u = 25;
    if(u > 500) u = 500;
    a->rx_unit_ms = u;

    char c = morse_reverse(pat);
    if(c) {
        rx_emit(a, c);
    } else if(strcmp(pat, "...---...") == 0) { // SOS prosign
        rx_emit(a, 'S');
        rx_emit(a, 'O');
        rx_emit(a, 'S');
    } else {
        rx_emit(a, '?');
    }
    a->rx_pat[0] = '\0';
}

static void rx_tune(MorseApp* a, const SubGhzDevice* dev) {
    subghz_devices_idle(dev);
    subghz_devices_set_frequency(dev, channel_freq(a->band, a->channel));
    subghz_devices_flush_rx(dev);
    subghz_devices_set_rx(dev);
}

static int32_t rx_thread_fn(void* ctx) {
    MorseApp* a = ctx;

    subghz_devices_init();
    const SubGhzDevice* dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!dev) {
        a->rx_error = 1;
        subghz_devices_deinit();
        redraw_view(a->rx_view);
        return 0;
    }

    subghz_devices_begin(dev);
    subghz_devices_reset(dev);
    subghz_devices_idle(dev);
    subghz_devices_load_preset(dev, FuriHalSubGhzPresetOok270Async, NULL);
    rx_tune(a, dev);

    float noise = -100.0f;
    float warm_sum = 0.0f;
    uint32_t warm_cnt = 0;
    uint32_t warm_end = furi_get_tick() + RX_WARMUP_MS;
    bool warmed = false;

    bool sig = false;
    bool have_above = false;
    uint32_t last_above = 0;
    uint32_t t_edge = furi_get_tick();
    uint32_t off_start = t_edge;
    uint32_t last_draw = 0;
    bool word_pending = false;

    uint32_t dur[RX_MAX_ELEMS];
    uint8_t n = 0;

    while(a->rx_run) {
        uint32_t now = furi_get_tick();

        if(a->rx_retune) {
            a->rx_retune = false;
            rx_tune(a, dev);
            warmed = false;
            warm_sum = 0.0f;
            warm_cnt = 0;
            warm_end = now + RX_WARMUP_MS;
            sig = false;
            have_above = false;
            n = 0;
            word_pending = false;
            a->rx_pat[0] = '\0';
        }

        float rssi = subghz_devices_get_rssi(dev);
        a->rx_rssi_i = (int16_t)rssi;

        if(!warmed) {
            // measure the noise floor for a moment
            warm_sum += rssi;
            warm_cnt++;
            a->rx_noise_i = (int16_t)(warm_sum / (float)warm_cnt);
            if((int32_t)(now - warm_end) >= 0) {
                noise = warm_sum / (float)warm_cnt;
                warmed = true;
                t_edge = now;
                off_start = now;
            }
        } else {
            float thr = noise + (float)a->rx_margin;
            if(rssi > thr) {
                last_above = now;
                have_above = true;
            } else if(rssi < noise) {
                noise += (rssi - noise) * 0.2f; // follows the floor down quickly
            } else {
                noise += (rssi - noise) * 0.01f; // and up slowly
            }
            a->rx_noise_i = (int16_t)noise;

            bool on = have_above && ((now - last_above) < RX_HOLD_MS);
            uint32_t unit = a->rx_unit_ms;

            if(on && !sig) {
                sig = true;
                t_edge = now;
            } else if(!on && sig) {
                sig = false;
                // back-dated to the last strong sample (removes the hold time)
                uint32_t d = last_above - t_edge;
                if(d >= RX_MIN_PULSE_MS && d <= 10 * unit) {
                    if(n < RX_MAX_ELEMS) {
                        dur[n++] = d;
                    } else {
                        rx_emit(a, '?');
                        n = 0;
                    }
                    off_start = last_above;
                    if(n) rx_classify(dur, n, unit, a->rx_pat, NULL);
                }
            }

            if(!sig) {
                uint32_t gap = now - off_start;
                if(n > 0 && gap >= 2 * unit) { // end of letter
                    rx_flush_char(a, dur, n);
                    n = 0;
                    word_pending = true;
                }
                unit = a->rx_unit_ms;
                if(word_pending && gap >= 5 * unit) { // end of word
                    size_t len = a->rx_len;
                    if(len && a->rx_text[len - 1] != ' ') rx_emit(a, ' ');
                    word_pending = false;
                }
            }
        }

        a->rx_sig = sig;
        if(now - last_draw >= 50) {
            last_draw = now;
            redraw_view(a->rx_view);
        }
        furi_delay_ms(1);
    }

    subghz_devices_idle(dev);
    subghz_devices_sleep(dev);
    subghz_devices_end(dev);
    subghz_devices_deinit();
    return 0;
}

static void rx_start(MorseApp* a) {
    if(a->rx_thread) return;
    a->rx_error = 0;
    a->rx_sig = false;
    a->rx_retune = false;
    a->rx_pat[0] = '\0';
    a->rx_unit_ms = 1200UL / a->wpm;
    a->rx_run = true;
    a->rx_thread = furi_thread_alloc_ex("MorseRx", 2048, rx_thread_fn, a);
    furi_thread_start(a->rx_thread);
}

static void rx_stop(MorseApp* a) {
    if(!a->rx_thread) return;
    a->rx_run = false;
    furi_thread_join(a->rx_thread);
    furi_thread_free(a->rx_thread);
    a->rx_thread = NULL;
}

/* ---------- RX view ---------- */

static void rx_draw(Canvas* c, void* model) {
    MorseApp* a = ((MainModel*)model)->app;
    char buf[48];

    canvas_clear(c);

    // ---- header ----
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    snprintf(buf, sizeof(buf), "RX %s CH %02u", band_names[a->band], a->channel);
    canvas_draw_str(c, 3, 10, buf);
    if(a->rx_sig) {
        canvas_draw_disc(c, 119, 6, 4);
    } else {
        canvas_draw_circle(c, 119, 6, 4);
    }
    canvas_set_color(c, ColorBlack);

    // ---- frequency + RSSI ----
    canvas_set_font(c, FontSecondary);
    uint32_t f = channel_freq(a->band, a->channel);
    snprintf(buf, sizeof(buf), "%lu.%05lu MHz", f / 1000000UL, (f % 1000000UL) / 10UL);
    canvas_draw_str(c, 2, 22, buf);
    snprintf(buf, sizeof(buf), "%d dBm", (int)a->rx_rssi_i);
    canvas_draw_str_aligned(c, 126, 22, AlignRight, AlignBottom, buf);

    // ---- level bar with threshold marker ----
    int rssi = a->rx_rssi_i;
    int thr = (int)a->rx_noise_i + (int)a->rx_margin;
    int w = ((clampi(rssi, -120, -20) + 120) * 120) / 100;
    int tx = 3 + ((clampi(thr, -120, -20) + 120) * 120) / 100;
    canvas_draw_frame(c, 2, 25, 124, 6);
    if(w > 0) canvas_draw_box(c, 3, 26, w, 4);
    canvas_draw_line(c, tx, 24, tx, 31);

    // ---- decoded text ----
    size_t len = a->rx_len;
    if(len > RX_TEXT_MAX) len = RX_TEXT_MAX;
    if(a->rx_error) {
        canvas_draw_str(c, 2, 40, "RX start error");
    } else if(len == 0) {
        canvas_draw_str(c, 2, 38, "Waiting for signal...");
        canvas_draw_str(c, 2, 46, "</> chan   ^/v threshold");
        canvas_draw_str(c, 2, 54, "OK clear   hold OK band");
    } else {
        size_t cap = RX_LINE_CHARS * RX_LINES;
        size_t start = 0;
        if(len > cap) start = ((len - cap + RX_LINE_CHARS - 1) / RX_LINE_CHARS) * RX_LINE_CHARS;
        char line[RX_LINE_CHARS + 1];
        for(size_t l = 0; l < RX_LINES; l++) {
            size_t off = start + l * RX_LINE_CHARS;
            if(off >= len) break;
            size_t cnt = len - off;
            if(cnt > RX_LINE_CHARS) cnt = RX_LINE_CHARS;
            memcpy(line, a->rx_text + off, cnt);
            line[cnt] = '\0';
            canvas_draw_str(c, 2, (int)(38 + l * 8), line);
        }
    }

    // ---- bottom line: current symbols + speed ----
    char pat[RX_MAX_ELEMS + 1];
    memcpy(pat, a->rx_pat, sizeof(pat));
    pat[RX_MAX_ELEMS] = '\0';
    canvas_draw_str(c, 2, 63, pat);
    uint32_t unit = a->rx_unit_ms ? a->rx_unit_ms : 1;
    snprintf(buf, sizeof(buf), "~%luWPM  THR+%u", 1200UL / unit, (unsigned)a->rx_margin);
    canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, buf);
}

static bool rx_input(InputEvent* e, void* ctx) {
    MorseApp* a = ctx;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypeShort) {
            rx_stop(a);
            view_dispatcher_switch_to_view(a->vd, ViewMenu);
        }
        return true;
    }

    uint8_t maxc = max_channel(a->band);

    if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
        switch(e->key) {
        case InputKeyLeft:
            a->channel = (a->channel > 1) ? a->channel - 1 : maxc;
            a->rx_retune = true;
            break;
        case InputKeyRight:
            a->channel = (a->channel < maxc) ? a->channel + 1 : 1;
            a->rx_retune = true;
            break;
        case InputKeyUp:
            if(a->rx_margin < 30) a->rx_margin++;
            break;
        case InputKeyDown:
            if(a->rx_margin > 3) a->rx_margin--;
            break;
        case InputKeyOk:
            if(e->type == InputTypeShort) {
                a->rx_len = 0;
                a->rx_text[0] = '\0';
                a->rx_pat[0] = '\0';
            }
            break;
        default:
            break;
        }
    } else if(e->type == InputTypeLong && e->key == InputKeyOk) {
        a->band = (a->band == BandPMR) ? BandLPD : BandPMR;
        if(a->channel > max_channel(a->band)) a->channel = max_channel(a->band);
        a->rx_retune = true;
    }

    redraw_view(a->rx_view);
    return true;
}

/* ---------- Text input ---------- */

static void text_done(void* ctx) {
    MorseApp* a = ctx;
    view_dispatcher_switch_to_view(a->vd, ViewTx);
}

static void text_setup(MorseApp* a) {
    text_input_set_header_text(a->text_input, "Morse message");
    text_input_set_result_callback(a->text_input, text_done, a, a->text, MAX_TEXT + 1, false);
}

/* ---------- Settings ---------- */

static void update_chan_text(MorseApp* a) {
    uint32_t f = channel_freq(a->band, a->channel);
    snprintf(
        a->chan_text,
        sizeof(a->chan_text),
        "%02u %lu.%05lu",
        a->channel,
        f / 1000000UL,
        (f % 1000000UL) / 10UL);
    variable_item_set_current_value_text(a->item_chan, a->chan_text);
}

static void update_tone_text(MorseApp* a) {
    snprintf(a->tone_text, sizeof(a->tone_text), "%u Hz", tone_hz[a->tone_idx]);
    variable_item_set_current_value_text(a->item_tone, a->tone_text);
}

static void update_msg_text(MorseApp* a) {
    if(a->text[0] == '\0') {
        snprintf(a->msg_text, sizeof(a->msg_text), "(empty)");
    } else {
        snprintf(
            a->msg_text,
            sizeof(a->msg_text),
            "%.14s%s",
            a->text,
            strlen(a->text) > 14 ? ".." : "");
    }
    variable_item_set_current_value_text(a->item_msg, a->msg_text);
}

static void cb_band(VariableItem* it) {
    MorseApp* a = variable_item_get_context(it);
    a->band = variable_item_get_current_value_index(it);
    uint8_t maxc = max_channel(a->band);
    if(a->channel > maxc) a->channel = maxc;
    variable_item_set_current_value_text(it, band_names[a->band]);
    variable_item_set_values_count(a->item_chan, maxc);
    variable_item_set_current_value_index(a->item_chan, a->channel - 1);
    update_chan_text(a);
}

static void cb_chan(VariableItem* it) {
    MorseApp* a = variable_item_get_context(it);
    a->channel = variable_item_get_current_value_index(it) + 1;
    update_chan_text(a);
}

static void cb_wpm(VariableItem* it) {
    MorseApp* a = variable_item_get_context(it);
    a->wpm = variable_item_get_current_value_index(it) + 5;
    snprintf(a->wpm_text, sizeof(a->wpm_text), "%u", a->wpm);
    variable_item_set_current_value_text(it, a->wpm_text);
}

static void cb_mode(VariableItem* it) {
    MorseApp* a = variable_item_get_context(it);
    a->mode = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, mode_names[a->mode]);
}

static void cb_tone(VariableItem* it) {
    MorseApp* a = variable_item_get_context(it);
    a->tone_idx = variable_item_get_current_value_index(it);
    update_tone_text(a);
}

static void refresh_settings(MorseApp* a) {
    update_msg_text(a);

    variable_item_set_current_value_index(a->item_band, a->band);
    variable_item_set_current_value_text(a->item_band, band_names[a->band]);

    variable_item_set_values_count(a->item_chan, max_channel(a->band));
    variable_item_set_current_value_index(a->item_chan, a->channel - 1);
    update_chan_text(a);

    variable_item_set_current_value_index(a->item_wpm, a->wpm - 5);
    snprintf(a->wpm_text, sizeof(a->wpm_text), "%u", a->wpm);
    variable_item_set_current_value_text(a->item_wpm, a->wpm_text);

    variable_item_set_current_value_index(a->item_mode, a->mode);
    variable_item_set_current_value_text(a->item_mode, mode_names[a->mode]);

    variable_item_set_current_value_index(a->item_tone, a->tone_idx);
    update_tone_text(a);
}

static void cb_noop(VariableItem* it) {
    UNUSED(it);
}

static void settings_enter(void* ctx, uint32_t index) {
    MorseApp* a = ctx;
    if(index == 0) { // "Message"
        text_setup(a);
        view_dispatcher_switch_to_view(a->vd, ViewText);
    }
}

/* ---------- TX view ---------- */

static void draw_arrow(Canvas* c, int x, int y, bool left) {
    for(int i = 0; i < 4; i++) {
        int xx = left ? x + i : x - i;
        canvas_draw_line(c, xx, y - i, xx, y + i);
    }
}

static void main_draw(Canvas* c, void* model) {
    MorseApp* a = ((MainModel*)model)->app;
    char buf[48];

    canvas_clear(c);

    // ---- header ----
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 3, 10, "Morse TX");
    uint32_t lit = a->sending ? (1 + (furi_get_tick() / 200) % 3) : 3;
    for(uint32_t k = 0; k < 3; k++) {
        uint8_t h = 3 + k * 2;
        if(k < lit) {
            canvas_draw_box(c, 112 + k * 5, 11 - h, 3, h);
        } else {
            canvas_draw_frame(c, 112 + k * 5, 11 - h, 3, h);
        }
    }
    canvas_set_color(c, ColorBlack);

    // ---- band / channel with arrows ----
    uint32_t f = channel_freq(a->band, a->channel);
    canvas_set_font(c, FontPrimary);
    snprintf(buf, sizeof(buf), "%s  CH %02u", band_names[a->band], a->channel);
    canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignTop, buf);
    draw_arrow(c, 6, 19, true);
    draw_arrow(c, 121, 19, false);

    // ---- frequency ----
    canvas_set_font(c, FontSecondary);
    snprintf(buf, sizeof(buf), "%lu.%05lu MHz", f / 1000000UL, (f % 1000000UL) / 10UL);
    canvas_draw_str_aligned(c, 64, 25, AlignCenter, AlignTop, buf);

    // ---- info line ----
    if(a->sending) {
        snprintf(buf, sizeof(buf), "TRANSMITTING...");
    } else if(a->result == 1) {
        snprintf(buf, sizeof(buf), "TX not allowed (region)");
    } else if(a->result == 2) {
        snprintf(buf, sizeof(buf), "TX start error");
    } else if(a->mode == 0) {
        snprintf(buf, sizeof(buf), "%u WPM  -  FM %u Hz", a->wpm, tone_hz[a->tone_idx]);
    } else if(a->mode == 1) {
        snprintf(buf, sizeof(buf), "%u WPM  -  OOK %u Hz", a->wpm, tone_hz[a->tone_idx]);
    } else {
        snprintf(buf, sizeof(buf), "%u WPM  -  CW", a->wpm);
    }
    canvas_draw_str_aligned(c, 64, 33, AlignCenter, AlignTop, buf);

    // ---- message box / progress bar ----
    canvas_draw_rframe(c, 2, 43, 124, 12, 3);
    if(a->sending) {
        size_t cnt = a->count ? a->count : 1;
        canvas_draw_box(c, 5, 46, (118 * a->idx) / cnt, 6);
    } else if(a->text[0] == '\0') {
        canvas_draw_str(c, 7, 52, "Press OK to write message");
    } else {
        FuriString* t = furi_string_alloc_set(a->text);
        elements_string_fit_width(c, t, 114);
        canvas_draw_str(c, 7, 52, furi_string_get_cstr(t));
        furi_string_free(t);
    }

    // ---- hints ----
    if(a->sending) {
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "Back = stop");
    } else if(a->text[0] == '\0') {
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "OK write   v SOS   ^ set");
    } else {
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "OK send   v SOS   ^ set");
    }
}

static bool main_input(InputEvent* e, void* ctx) {
    MorseApp* a = ctx;

    if(a->sending) {
        if(e->key == InputKeyBack && e->type == InputTypeShort) a->abort = true;
        return true; // consumes everything during TX
    }

    bool consumed = true;
    uint8_t maxc = max_channel(a->band);

    if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
        switch(e->key) {
        case InputKeyLeft:
            a->channel = (a->channel > 1) ? a->channel - 1 : maxc;
            a->result = 0;
            redraw(a);
            break;
        case InputKeyRight:
            a->channel = (a->channel < maxc) ? a->channel + 1 : 1;
            a->result = 0;
            redraw(a);
            break;
        case InputKeyDown:
            if(e->type == InputTypeShort) start_tx(a, true); // quick SOS
            break;
        case InputKeyUp:
            if(e->type == InputTypeShort) {
                refresh_settings(a);
                view_dispatcher_switch_to_view(a->vd, ViewSettings);
            }
            break;
        case InputKeyOk:
            if(e->type == InputTypeShort) {
                if(a->text[0] == '\0') { // no message: open the editor
                    text_setup(a);
                    view_dispatcher_switch_to_view(a->vd, ViewText);
                } else {
                    start_tx(a, false);
                }
            }
            break;
        default:
            consumed = false; // Back -> main menu
            break;
        }
    } else if(e->type == InputTypeLong && e->key == InputKeyOk) {
        text_setup(a);
        view_dispatcher_switch_to_view(a->vd, ViewText);
    } else {
        consumed = (e->key != InputKeyBack);
    }
    return consumed;
}

/* ---------- Navigation ---------- */

static uint32_t exit_app(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE; // leaves the app
}

static uint32_t to_menu(void* ctx) {
    UNUSED(ctx);
    return ViewMenu;
}

static uint32_t to_tx(void* ctx) {
    UNUSED(ctx);
    return ViewTx;
}

static void menu_cb(void* ctx, uint32_t index) {
    MorseApp* a = ctx;
    switch(index) {
    case MenuTx:
        a->result = 0;
        view_dispatcher_switch_to_view(a->vd, ViewTx);
        break;
    case MenuRx:
        rx_start(a);
        view_dispatcher_switch_to_view(a->vd, ViewRx);
        break;
    case MenuAbout:
        view_dispatcher_switch_to_view(a->vd, ViewAbout);
        break;
    default:
        break;
    }
}

/* ---------- Entry point ---------- */

int32_t morse_radio_tx_app(void* p) {
    UNUSED(p);
    MorseApp* a = malloc(sizeof(MorseApp));
    memset(a, 0, sizeof(MorseApp));

    a->band = BandPMR;
    a->channel = 1;
    a->wpm = 15;
    a->mode = 0;
    a->tone_idx = 1;
    a->rx_margin = 10;
    a->rx_unit_ms = 1200UL / a->wpm;

    a->notif = furi_record_open(RECORD_NOTIFICATION);
    Gui* gui = furi_record_open(RECORD_GUI);
    a->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(a->vd, gui, ViewDispatcherTypeFullscreen);

    // main menu
    a->menu = submenu_alloc();
    submenu_set_header(a->menu, "MorseTX");
    submenu_add_item(a->menu, "TX", MenuTx, menu_cb, a);
    submenu_add_item(a->menu, "RX", MenuRx, menu_cb, a);
    submenu_add_item(a->menu, "About", MenuAbout, menu_cb, a);
    view_set_previous_callback(submenu_get_view(a->menu), exit_app);
    view_dispatcher_add_view(a->vd, ViewMenu, submenu_get_view(a->menu));

    // TX view
    a->main_view = view_alloc();
    view_allocate_model(a->main_view, ViewModelTypeLockFree, sizeof(MainModel));
    with_view_model(a->main_view, MainModel * m, { m->app = a; }, false);
    view_set_context(a->main_view, a);
    view_set_draw_callback(a->main_view, main_draw);
    view_set_input_callback(a->main_view, main_input);
    view_set_previous_callback(a->main_view, to_menu);
    view_dispatcher_add_view(a->vd, ViewTx, a->main_view);

    // RX view
    a->rx_view = view_alloc();
    view_allocate_model(a->rx_view, ViewModelTypeLockFree, sizeof(MainModel));
    with_view_model(a->rx_view, MainModel * m, { m->app = a; }, false);
    view_set_context(a->rx_view, a);
    view_set_draw_callback(a->rx_view, rx_draw);
    view_set_input_callback(a->rx_view, rx_input);
    view_dispatcher_add_view(a->vd, ViewRx, a->rx_view);

    // settings
    a->settings = variable_item_list_alloc();
    a->item_msg = variable_item_list_add(a->settings, "Message (OK)", 1, cb_noop, a);
    variable_item_list_set_enter_callback(a->settings, settings_enter, a);
    a->item_band = variable_item_list_add(a->settings, "Band", 2, cb_band, a);
    a->item_chan = variable_item_list_add(a->settings, "Channel", 16, cb_chan, a);
    a->item_wpm = variable_item_list_add(a->settings, "Speed WPM", 26, cb_wpm, a);
    a->item_mode = variable_item_list_add(a->settings, "Mode", 3, cb_mode, a);
    a->item_tone = variable_item_list_add(a->settings, "Tone", 3, cb_tone, a);
    view_set_previous_callback(variable_item_list_get_view(a->settings), to_tx);
    view_dispatcher_add_view(a->vd, ViewSettings, variable_item_list_get_view(a->settings));

    // text input
    a->text_input = text_input_alloc();
    view_set_previous_callback(text_input_get_view(a->text_input), to_tx);
    view_dispatcher_add_view(a->vd, ViewText, text_input_get_view(a->text_input));

    // about
    a->about = widget_alloc();
    widget_add_string_element(
        a->about, 64, 26, AlignCenter, AlignCenter, FontPrimary, "MorseTX ver2.0");
    widget_add_string_element(
        a->about, 64, 40, AlignCenter, AlignCenter, FontSecondary, "created by Marcopiiit");
    view_set_previous_callback(widget_get_view(a->about), to_menu);
    view_dispatcher_add_view(a->vd, ViewAbout, widget_get_view(a->about));

    view_dispatcher_switch_to_view(a->vd, ViewMenu);
    view_dispatcher_run(a->vd);

    // cleanup
    rx_stop(a);
    a->abort = true;
    if(a->thread) {
        furi_thread_join(a->thread);
        furi_thread_free(a->thread);
    }
    view_dispatcher_remove_view(a->vd, ViewMenu);
    view_dispatcher_remove_view(a->vd, ViewTx);
    view_dispatcher_remove_view(a->vd, ViewRx);
    view_dispatcher_remove_view(a->vd, ViewSettings);
    view_dispatcher_remove_view(a->vd, ViewText);
    view_dispatcher_remove_view(a->vd, ViewAbout);
    submenu_free(a->menu);
    view_free(a->main_view);
    view_free(a->rx_view);
    variable_item_list_free(a->settings);
    text_input_free(a->text_input);
    widget_free(a->about);
    view_dispatcher_free(a->vd);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(a);
    return 0;
}
