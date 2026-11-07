

#include <furi.h>
#include <furi_hal.h>
#include <infrared_transmit.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_H        9
#define VISIBLE_ROWS 4

static const uint32_t FREQS[] = {30000, 33000, 36000, 38000, 40000, 42000, 56000};
#define FREQ_COUNT (sizeof(FREQS) / sizeof(FREQS[0]))

typedef enum {
    ModeBasic,
    ModeEnhanced,
    ModeSweep,
    ModeRandom,
    ModeEmpty,
    ModeCount
} JamMode;
static const char* MODE_NAMES[] = {"Basic", "Enh.Basic", "Sweep", "Random", "Empty"};

typedef enum {
    OutAuto,
    OutInternal,
    OutExternal,
    OutCount
} OutSel;
static const char* OUT_NAMES[] = {"Auto", "Int", "Ext"};

typedef enum {
    ItemMode,
    ItemFreq,
    ItemOutput,
    ItemMark,
    ItemSpace,
    ItemMin,
    ItemMax,
    ItemSpeed,
    ItemPower
} Item;

static const GpioPin ir_int_pin = {.port = GPIOB, .pin = LL_GPIO_PIN_9};

typedef struct {
    FuriMutex* mutex;

    JamMode mode;
    uint8_t freq_idx;
    OutSel out_sel;
    int32_t mark; 
    int32_t space; 
    int32_t min_t; 
    int32_t max_t; 
    int8_t sweep_dir;
    uint8_t speed;
    uint8_t power; 

    // runtime
    bool active;
    uint32_t last_jam;
    uint32_t last_update;
    uint32_t jam_count;
    uint32_t start_tick;
    uint32_t run_secs;

    // output hardware
    FuriHalInfraredTxPin tx_pin;
    bool otg_by_us;

    // ui
    uint8_t sel;
    uint8_t scroll;
} JamState;

static int wrap(int v, int n) {
    return ((v % n) + n) % n;
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint8_t get_items(JamMode m, Item* out) {
    uint8_t n = 0;
    out[n++] = ItemMode;
    out[n++] = ItemFreq;
    out[n++] = ItemOutput;
    switch(m) {
    case ModeBasic:
        out[n++] = ItemMark;
        break;
    case ModeEnhanced:
        out[n++] = ItemMark;
        out[n++] = ItemSpace;
        break;
    case ModeSweep:
        out[n++] = ItemMin;
        out[n++] = ItemMax;
        out[n++] = ItemSpeed;
        break;
    default:
        break;
    }
    out[n++] = ItemPower;
    return n;
}

static FuriHalInfraredTxPin detect_tx_pin(void) {
    FuriHalInfraredTxPin pin = furi_hal_infrared_detect_tx_output();
    if(pin >= FuriHalInfraredTxPinMax) pin = FuriHalInfraredTxPinInternal;
    return pin;
}

static void update_output(JamState* s) {
    FuriHalInfraredTxPin pin;
    if(s->out_sel == OutInternal) {
        pin = FuriHalInfraredTxPinInternal;
    } else if(s->out_sel == OutExternal) {
        pin = FuriHalInfraredTxPinExtPA7;
    } else if(s->active && s->tx_pin < FuriHalInfraredTxPinMax) {
        pin = s->tx_pin;
    } else {
        pin = detect_tx_pin();
    }

    if(pin != s->tx_pin) {
        furi_hal_infrared_set_tx_output(pin);
        s->tx_pin = pin;
    }

    bool want_otg = (pin == FuriHalInfraredTxPinExtPA7) && s->active;
    if(want_otg && !s->otg_by_us && !furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        s->otg_by_us = true;
    } else if(!want_otg && s->otg_by_us) {
        furi_hal_power_disable_otg();
        s->otg_by_us = false;
    }
}

static const GpioPin* active_pin(const JamState* s) {
    return (s->tx_pin == FuriHalInfraredTxPinExtPA7) ? &gpio_ext_pa7 : &ir_int_pin;
}

static void bitbang(const GpioPin* pin, uint32_t cycles, uint32_t mark_us, uint32_t space_us) {
    furi_hal_gpio_init(pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    for(uint32_t i = 0; i < cycles; i++) {
        furi_hal_gpio_write(pin, true);
        furi_delay_us(mark_us);
        furi_hal_gpio_write(pin, false);
        furi_delay_us(space_us);
    }
}

static void sw_mark(const GpioPin* pin, uint32_t usec, uint32_t freq) {
    uint32_t period = 1000000UL / freq;
    uint32_t on = period / 2;
    uint32_t off = period - on;
    uint32_t t = 0;
    while(t < usec) {
        uint32_t d = (usec - t < on) ? (usec - t) : on;
        furi_hal_gpio_write(pin, true);
        furi_delay_us(d);
        furi_hal_gpio_write(pin, false);
        t += on;
        if(t >= usec) break;
        d = (usec - t < off) ? (usec - t) : off;
        furi_delay_us(d);
        t += off;
    }
}

static void sw_send_raw(const GpioPin* pin, const uint16_t* pat, size_t n, uint32_t freq) {
    furi_hal_gpio_init(pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    for(size_t i = 0; i < n; i++) {
        if((i & 1) == 0) {
            sw_mark(pin, pat[i], freq);
        } else {
            furi_hal_gpio_write(pin, false);
            furi_delay_us(pat[i]);
        }
    }
    furi_hal_gpio_write(pin, false);
}

static void send_basic_pattern(const GpioPin* pin, const JamState* s) {
    bitbang(pin, 10, s->mark, s->space);
}

static void perform_jamming(JamState* s) {
    uint32_t now = furi_get_tick();

    if(now - s->last_jam < (uint32_t)(10 / s->power)) return;
    s->last_jam = now;

    const GpioPin* pin = active_pin(s);
    bool sent = false;

    switch(s->mode) {
    case ModeBasic:
        if(now - s->last_update > 20) {
            bitbang(pin, 50 * s->power, s->mark, s->mark);
            send_basic_pattern(pin, s);
            s->last_update = now;
            sent = true;
        }
        break;

    case ModeEnhanced:
        if(now - s->last_update > 20) {
            bitbang(pin, 25 * s->power, s->mark, s->space);
            send_basic_pattern(pin, s);
            s->last_update = now;
            sent = true;
        }
        break;

    case ModeSweep:
        if(now - s->last_update > 30) {
            s->mark += s->sweep_dir * s->speed;
            if(s->mark > s->max_t || s->mark < s->min_t) {
                s->sweep_dir = -s->sweep_dir;
                s->mark = clampi(s->mark, s->min_t, s->max_t);
            }
            s->space = s->mark;
            bitbang(pin, 20 * s->power, s->mark, s->mark);
            send_basic_pattern(pin, s);
            s->last_update = now;
            sent = true;
        }
        break;

    case ModeRandom:
        if(now - s->last_update > 100) {
            uint16_t pat[30];
            for(int i = 0; i < 30; i++) pat[i] = 5 + (furi_hal_random_get() % 995);
            for(int i = 0; i < s->power / 2 + 1; i++) {
                if((furi_hal_random_get() % 10) < 3) s->freq_idx = furi_hal_random_get() % FREQ_COUNT;
                sw_send_raw(pin, pat, 30, FREQS[s->freq_idx]);
            }
            s->last_update = now;
            sent = true;
        }
        break;

    case ModeEmpty:
        if(now - s->last_update > 50) {
            static const uint16_t empty_pat[4] = {1, 1, 1, 1};
            for(int i = 0; i < s->power; i++) {
                sw_send_raw(pin, empty_pat, 4, FREQS[s->freq_idx]);
            }
            if((furi_hal_random_get() % 5) < 2) s->freq_idx = (s->freq_idx + 1) % FREQ_COUNT;
            s->last_update = now;
            sent = true;
        }
        break;

    default:
        break;
    }

    if(sent) s->jam_count++;
}

static void adjust(JamState* s, Item it, int dir) {
    switch(it) {
    case ItemMode:
        s->mode = (JamMode)wrap((int)s->mode + dir, ModeCount);
        s->mark = 12;
        s->space = 12;
        s->min_t = 8;
        s->max_t = 70;
        s->speed = 1;
        s->sweep_dir = 1;
        break;
    case ItemFreq:
        s->freq_idx = wrap((int)s->freq_idx + dir, (int)FREQ_COUNT);
        break;
    case ItemOutput:
        s->out_sel = (OutSel)wrap((int)s->out_sel + dir, OutCount);
        update_output(s);
        break;
    case ItemMark:
        s->mark = clampi(s->mark + dir, 5, 100);
        if(s->mode == ModeBasic) s->space = s->mark; // keep in sync in Basic
        break;
    case ItemSpace:
        s->space = clampi(s->space + dir, 1, 100);
        break;
    case ItemMin:
        s->min_t = clampi(s->min_t + dir, 1, s->max_t - 5);
        break;
    case ItemMax:
        s->max_t = clampi(s->max_t + dir, s->min_t + 5, 150);
        break;
    case ItemSpeed:
        s->speed = clampi(s->speed + dir, 1, 10);
        break;
    case ItemPower:
        s->power = clampi(s->power + dir, 1, 20);
        break;
    }
}

/* -------------------------------------------- drawing ------------------------------------------- */

static void item_text(const JamState* s, Item it, const char** label, char* val, size_t sz) {
    switch(it) {
    case ItemMode:
        *label = "Mode";
        snprintf(val, sz, "%s", MODE_NAMES[s->mode]);
        break;
    case ItemFreq:
        *label = "Freq";
        snprintf(val, sz, "%lu kHz", FREQS[s->freq_idx] / 1000);
        break;
    case ItemOutput:
        *label = "Output";
        snprintf(val, sz, "%s", OUT_NAMES[s->out_sel]);
        break;
    case ItemMark:
        *label = (s->mode == ModeBasic) ? "Timing" : "Mark";
        snprintf(val, sz, "%ld us", s->mark);
        break;
    case ItemSpace:
        *label = "Space";
        snprintf(val, sz, "%ld us", s->space);
        break;
    case ItemMin:
        *label = "Min";
        snprintf(val, sz, "%ld us", s->min_t);
        break;
    case ItemMax:
        *label = "Max";
        snprintf(val, sz, "%ld us", s->max_t);
        break;
    case ItemSpeed:
        *label = "Speed";
        snprintf(val, sz, "%u", s->speed);
        break;
    case ItemPower:
        *label = "Density";
        snprintf(val, sz, "%u", s->power);
        break;
    }
}

static void draw_cb(Canvas* c, void* ctx) {
    JamState* s = ctx;
    furi_mutex_acquire(s->mutex, FuriWaitForever);
    char buf[32];

    canvas_clear(c);

    // Header bar
    canvas_draw_box(c, 0, 0, 128, 11);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 3, 9, "IR Jammer");
    canvas_set_font(c, FontSecondary);
    bool ext = (s->tx_pin == FuriHalInfraredTxPinExtPA7);
    const char* st = s->active ? "JAMMING" : "IDLE";
    snprintf(buf, sizeof(buf), "%s %s", ext ? "EXT" : "INT", st);
    canvas_draw_str_aligned(c, 125, 6, AlignRight, AlignCenter, buf);
    canvas_set_color(c, ColorBlack);

    // Settings list
    Item items[8];
    uint8_t n = get_items(s->mode, items);
    for(uint8_t i = 0; i < VISIBLE_ROWS && (s->scroll + i) < n; i++) {
        uint8_t idx = s->scroll + i;
        int y = 13 + i * ROW_H;
        const char* label = "";
        char val[20];
        item_text(s, items[idx], &label, val, sizeof(val));

        bool selected = (idx == s->sel);
        if(selected) {
            canvas_draw_box(c, 0, y, 84, ROW_H);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str_aligned(c, 3, y + 5, AlignLeft, AlignCenter, label);
        canvas_draw_str_aligned(c, 81, y + 5, AlignRight, AlignCenter, val);
        if(selected) canvas_set_color(c, ColorBlack);
    }
    if(n > VISIBLE_ROWS) {
        elements_scrollbar_pos(c, 86, 13, ROW_H * VISIBLE_ROWS, s->sel, n);
    }

    // Stats panel
    canvas_draw_rframe(c, 91, 12, 36, 38, 2);
    float jps = s->run_secs > 0 ? (float)s->jam_count / (float)s->run_secs : 0.0f;
    canvas_draw_str_aligned(c, 109, 21, AlignCenter, AlignBottom, "Jams");
    snprintf(buf, sizeof(buf), "%lu", s->jam_count);
    canvas_draw_str_aligned(c, 109, 30, AlignCenter, AlignBottom, buf);
    snprintf(buf, sizeof(buf), "%02lu:%02lu", s->run_secs / 60, s->run_secs % 60);
    canvas_draw_str_aligned(c, 109, 39, AlignCenter, AlignBottom, buf);
    snprintf(buf, sizeof(buf), "%.1f/s", (double)jps);
    canvas_draw_str_aligned(c, 109, 48, AlignCenter, AlignBottom, buf);

    // Footer buttons
    elements_button_left(c, "-");
    elements_button_center(c, s->active ? "Stop" : "Start");
    elements_button_right(c, "+");

    furi_mutex_release(s->mutex);
}

/* -------------------------------------------- input ---------------------------------------- */

static void input_cb(InputEvent* e, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, e, 0);
}

static bool handle_input(JamState* s, InputEvent* e) {
    bool keep = true;
    furi_mutex_acquire(s->mutex, FuriWaitForever);

    Item items[8];
    uint8_t n = get_items(s->mode, items);

    if(e->type == InputTypeLong && e->key == InputKeyBack) {
        keep = false;
    } else if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
        switch(e->key) {
        case InputKeyUp:
            s->sel = wrap((int)s->sel - 1, n);
            break;
        case InputKeyDown:
            s->sel = wrap((int)s->sel + 1, n);
            break;
        case InputKeyLeft:
            adjust(s, items[s->sel], -1);
            break;
        case InputKeyRight:
            adjust(s, items[s->sel], +1);
            break;
        case InputKeyOk:
            if(e->type == InputTypeShort) {
                if(!s->active) {
                    update_output(s); 
                    s->active = true;
                    s->jam_count = 0;
                    s->run_secs = 0;
                    s->start_tick = furi_get_tick();
                    s->last_jam = 0;
                    s->last_update = 0;
                } else {
                    s->active = false;
                }
                update_output(s);
            }
            break;
        case InputKeyBack:
            if(e->type == InputTypeShort) {
                if(s->active) {
                    s->active = false;
                    update_output(s);
                } else {
                    keep = false;
                }
            }
            break;
        default:
            break;
        }
    }

    n = get_items(s->mode, items);
    if(s->sel >= n) s->sel = n - 1;
    if(s->sel < s->scroll) s->scroll = s->sel;
    if(s->sel >= s->scroll + VISIBLE_ROWS) s->scroll = s->sel - VISIBLE_ROWS + 1;
    if(n <= VISIBLE_ROWS) s->scroll = 0;

    furi_mutex_release(s->mutex);
    return keep;
}

/* ------------------------------------ main ---------------------------------- */

int32_t ir_jammer_app(void* p) {
    UNUSED(p);

    JamState* s = malloc(sizeof(JamState));
    memset(s, 0, sizeof(JamState));
    s->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    s->mode = ModeBasic;
    s->freq_idx = 3; // 38 Khz
    s->out_sel = OutAuto;
    s->mark = 12;
    s->space = 12;
    s->min_t = 8;
    s->max_t = 70;
    s->sweep_dir = 1;
    s->speed = 1;
    s->power = 5;
    s->tx_pin = FuriHalInfraredTxPinMax;

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, s);
    view_port_input_callback_set(vp, input_cb, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    update_output(s);
    furi_mutex_release(s->mutex);

    uint32_t last_ui = 0;
    uint32_t last_detect = 0;
    bool run = true;

    while(run) {
        InputEvent ev;
        
        uint32_t timeout = s->active ? 5 : 100;
        if(furi_message_queue_get(queue, &ev, timeout) == FuriStatusOk) {
            run = handle_input(s, &ev);
            view_port_update(vp);
        }

        if(s->active && run) perform_jamming(s);

        uint32_t now = furi_get_tick();

        // While idle check ext
        if(!s->active && now - last_detect >= 2000) {
            last_detect = now;
            furi_mutex_acquire(s->mutex, FuriWaitForever);
            if(s->out_sel == OutAuto) update_output(s);
            furi_mutex_release(s->mutex);
        }

        if(now - last_ui >= 150) {
            last_ui = now;
            furi_mutex_acquire(s->mutex, FuriWaitForever);
            if(s->active) s->run_secs = (now - s->start_tick) / 1000;
            furi_mutex_release(s->mutex);
            view_port_update(vp);
        }
    }

    furi_mutex_acquire(s->mutex, FuriWaitForever);
    s->active = false;
    update_output(s);
    furi_hal_gpio_write(&gpio_ext_pa7, false);
    furi_hal_gpio_write(&ir_int_pin, false);
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&ir_int_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    furi_mutex_release(s->mutex);

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_mutex_free(s->mutex);
    free(s);
    return 0;
}
