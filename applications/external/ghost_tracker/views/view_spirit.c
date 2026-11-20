#include "view_spirit.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <furi_hal_i2c.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <stdio.h>

/* REAL frequency sweep using the Flipper's actual CC1101 sub-GHz radio -
 * the same chip Radio Scanner uses, and (per Radio Scanner's own source)
 * the SAME technique: furi_hal_subghz_set_async_mirror_pin() mirrors the
 * CC1101's raw demodulated RX bitstream directly onto the speaker's GPIO
 * pin, in hardware, in real time. That's actual RF static playing through
 * the piezo buzzer - not a synthesized tone. We don't touch furi_hal_speaker
 * at all here; the audio path is a totally different, lower-level one.
 *
 * 300.00-348.00MHz is one of the CC1101's hardware-valid receive ranges
 * (furi_hal_subghz_is_frequency_valid). We only RX (never
 * furi_hal_subghz_tx), so there's no transmit-legality concern. */
#define SUBGHZ_FREQ_MIN_HZ  300000000UL
#define SUBGHZ_FREQ_MAX_HZ  348000000UL
#define SUBGHZ_FREQ_STEP_HZ 200000UL /* 200kHz per hop across the ~48MHz band */
#define SUBGHZ_SETTLE_MS    10 /* let the CC1101's AGC settle before trusting RSSI */
#define SUBGHZ_HIT_RSSI_DBM -90.0f /* above this = real RF energy present, not just noise floor */

/* REAL FM broadcast sweep using a TEA5767 module wired to the Flipper's
 * official external I2C bus (furi_hal_i2c_handle_external = PC0/SCL,
 * PC1/SDA, pins 16/15 - same pins the EMF Hall sensor / REM Pod antenna use
 * in their external-wiring modes; the user's board has a physical switch
 * to pick which hardware is connected to those two pins at any given time).
 * This is the "minimal" tier from the TEA5767 plan: the Flipper only sends
 * I2C tuning/mute commands. Audio comes out of the module's own L/R output
 * jack into a separate speaker/headphones wired directly to it - the
 * Flipper's own speaker is never touched in this mode, and there's no
 * software volume control, only real hardware mute (register bit). */
#define TEA5767_I2C_ADDR \
    (0x60 << 1) /* fixed 7-bit device address 0x60, shifted for furi_hal_i2c */
#define TEA5767_I2C_TIMEOUT_MS 10

#define FM_FREQ_MIN_HZ  87500000UL /* 87.5MHz: standard US/Europe FM band low edge */
#define FM_FREQ_MAX_HZ  108000000UL
#define FM_FREQ_STEP_HZ 100000UL /* 0.1MHz per hop */
#define FM_SETTLE_MS \
    50 /* TEA5767's PLL needs real time to lock before ADC level reads are valid */

#define SPIRIT_STEP_MIN_MS \
    20 /* FM mode floors out near FM_SETTLE_MS - can't tune faster than the PLL locks */
#define SPIRIT_STEP_MAX_MS 500
#define SPIRIT_STEP_INC_MS 10

#define SPIRIT_WORKER_STACK_SIZE 2048

typedef enum {
    SpiritSourceStatic, /* CC1101 sub-GHz static sweep (original) */
    SpiritSourceFm, /* TEA5767 real FM broadcast sweep */
} SpiritSource;

typedef struct {
    SpiritSource source;
    uint32_t freq_hz; /* real, currently tuned frequency (sub-GHz or FM band depending on source) */
    float rssi_dbm; /* SpiritSourceStatic only */
    bool hit; /* SpiritSourceStatic only: RSSI above threshold */
    uint8_t fm_level; /* SpiritSourceFm only: TEA5767 ADC signal level, 0-15 */
    bool fm_stereo; /* SpiritSourceFm only */
    bool fm_connected; /* SpiritSourceFm only: did the module ACK on the bus last check? */
    uint32_t step_ms;
    bool muted;
} SpiritViewModel;

struct SpiritView {
    View* view;
    FuriThread* thread;
    volatile bool running;
    volatile SpiritSource requested_source;
};

/* Required by furi_hal_subghz_start_async_rx but unused: the actual audio
 * comes from the hardware mirror pin, not from this callback. */
static void spirit_rx_capture_callback(bool level, uint32_t duration, void* context) {
    UNUSED(level);
    UNUSED(duration);
    UNUSED(context);
}

/* Writes a real TEA5767 tuning command over I2C: PLL word for the given
 * frequency (high-side injection, internal 32.768kHz reference - the
 * reference clock virtually every cheap TEA5767 breakout board ships with),
 * plus the hardware MUTE bit. This is the exact register sequence used by
 * the well-known open-source TEA5767 Arduino libraries this app's TEA5767
 * plan was based on. */
static bool tea5767_write_frequency(uint32_t freq_hz, bool mute) {
    uint32_t pll = (uint32_t)((4ULL * (freq_hz + 225000ULL) + 16384ULL) / 32768ULL);

    uint8_t data[5];
    data[0] = (uint8_t)((pll >> 8) & 0x3F); /* top 6 bits of the 14-bit PLL word */
    if(mute) data[0] |= 0x80; /* MUTE */
    data[1] = (uint8_t)(pll & 0xFF); /* low 8 bits of the PLL word */
    data[2] = 0x10; /* HLSI=1 (high-side injection); search-mode bits unused (SM=0) */
    data[3] = 0x10; /* XTAL=1 (internal 32.768kHz ref); BL=0 (US/Europe 87.5-108MHz band) */
    data[4] = 0x00;

    return furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external,
        TEA5767_I2C_ADDR,
        data,
        sizeof(data),
        TEA5767_I2C_TIMEOUT_MS);
}

/* Reads real signal-strength/stereo status back from the TEA5767. Level is
 * the chip's own 4-bit ADC level (0-15) - a genuine hardware reading of how
 * strongly the tuned frequency is being received, same quantity a real
 * SB7-style spirit box's "signal found" behavior is based on. */
static bool tea5767_read_status(uint8_t* level_out, bool* stereo_out) {
    uint8_t data[5] = {0};
    bool ok = furi_hal_i2c_rx(
        &furi_hal_i2c_handle_external,
        TEA5767_I2C_ADDR,
        data,
        sizeof(data),
        TEA5767_I2C_TIMEOUT_MS);
    if(ok) {
        *stereo_out = (data[2] & 0x80) != 0;
        *level_out = (data[3] >> 4) & 0x0F;
    }
    return ok;
}

/* Claims whichever hardware the given source needs. Mirrors the
 * acquire/release pattern used by EMF/REM Pod for consistent lifecycle
 * handling when the user flips sources mid-session. */
static void spirit_acquire(SpiritSource source) {
    if(source == SpiritSourceStatic) {
        furi_hal_subghz_reset();
        /* 2FSK/238 preset: same one Radio Scanner uses for audible RX. */
        furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
        furi_hal_subghz_set_async_mirror_pin(&gpio_speaker);
    } else {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    }
}

static void spirit_release(SpiritSource source, bool rx_active, uint32_t fm_freq_at_release) {
    if(source == SpiritSourceStatic) {
        if(rx_active) {
            furi_hal_subghz_stop_async_rx();
        }
        furi_hal_light_set(LightRed, 0);
        furi_hal_subghz_idle();
        furi_hal_subghz_set_async_mirror_pin(NULL);
        furi_hal_subghz_sleep();
    } else {
        /* The TEA5767's audio comes out its OWN L/R jack, straight to a
         * separate speaker - entirely outside the Flipper's control once
         * tuned. Releasing the I2C bus here does NOT silence it; without
         * an explicit mute write first, it just keeps playing whatever it
         * was last tuned to after the user backs out of this view. */
        tea5767_write_frequency(fm_freq_at_release, true);
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    }
}

/* Owns whichever radio hardware the active source needs, entirely on this
 * thread. Switching sources mid-run cleanly releases the old hardware
 * before claiming the new one. */
static int32_t spirit_worker(void* context) {
    SpiritView* instance = context;

    SpiritSource current_source = instance->requested_source;
    spirit_acquire(current_source);

    uint32_t static_freq = SUBGHZ_FREQ_MIN_HZ;
    bool rx_active = false;
    /* The mirror pin is the ONLY audio control available with the static
     * sweep's real-hardware technique: it's a raw digital bitstream, not a
     * PWM amplitude signal, so there's no continuous volume - just pin
     * connected (audible) or disconnected (silent). */
    bool mirror_connected = (current_source == SpiritSourceStatic);

    uint32_t fm_freq = FM_FREQ_MIN_HZ;

    while(instance->running) {
        SpiritSource wanted = instance->requested_source;
        if(wanted != current_source) {
            spirit_release(current_source, rx_active, fm_freq);
            rx_active = false;
            current_source = wanted;
            spirit_acquire(current_source);
            mirror_connected = (current_source == SpiritSourceStatic);
        }

        uint32_t step_ms;
        bool muted;
        with_view_model(
            instance->view,
            SpiritViewModel * model,
            {
                step_ms = model->step_ms;
                muted = model->muted;
            },
            false);

        if(current_source == SpiritSourceStatic) {
            static_freq += SUBGHZ_FREQ_STEP_HZ;
            if(static_freq > SUBGHZ_FREQ_MAX_HZ) static_freq = SUBGHZ_FREQ_MIN_HZ;

            if(rx_active) {
                furi_hal_subghz_stop_async_rx();
            }
            furi_hal_subghz_idle();
            uint32_t real_freq = furi_hal_subghz_set_frequency_and_path(static_freq);
            furi_hal_subghz_start_async_rx(spirit_rx_capture_callback, NULL);
            rx_active = true;

            furi_delay_ms(SUBGHZ_SETTLE_MS);
            float rssi = furi_hal_subghz_get_rssi();
            bool hit = rssi > SUBGHZ_HIT_RSSI_DBM;

            bool want_connected = !muted;
            if(want_connected != mirror_connected) {
                /* Reconnecting/disconnecting the mirror is independent of
                 * the ongoing async RX capture - toggling it doesn't
                 * disturb the sweep or the RSSI readings at all. */
                furi_hal_subghz_set_async_mirror_pin(want_connected ? &gpio_speaker : NULL);
                mirror_connected = want_connected;
            }

            furi_hal_light_set(LightRed, hit ? 255 : 0);

            with_view_model(
                instance->view,
                SpiritViewModel * model,
                {
                    model->source = current_source;
                    model->freq_hz = real_freq;
                    model->rssi_dbm = rssi;
                    model->hit = hit;
                },
                true);

            /* The static keeps playing live via the hardware mirror for
             * the rest of this dwell step - no CPU work needed meanwhile. */
            uint32_t remaining_ms = step_ms > SUBGHZ_SETTLE_MS ? step_ms - SUBGHZ_SETTLE_MS : 0;
            furi_delay_ms(remaining_ms);
        } else {
            /* Check the module actually ACKs before doing anything else -
             * a short, bounded presence check. If it's not wired/powered
             * (switch in the wrong position, board unplugged), this just
             * reports "not found" instead of the worker spinning on tune/
             * read calls against a device that isn't there. This does NOT
             * protect against a genuinely stuck I2C bus (SDA held low by
             * a half-connected module) - that's a real hardware lockup no
             * software timeout can fully guarantee against; if the device
             * freezes here, check the switch/wiring, don't just retry. */
            bool connected = furi_hal_i2c_is_device_ready(
                &furi_hal_i2c_handle_external, TEA5767_I2C_ADDR, TEA5767_I2C_TIMEOUT_MS);

            uint8_t level = 0;
            bool stereo = false;
            if(connected) {
                fm_freq += FM_FREQ_STEP_HZ;
                if(fm_freq > FM_FREQ_MAX_HZ) fm_freq = FM_FREQ_MIN_HZ;

                tea5767_write_frequency(fm_freq, muted);
                furi_delay_ms(FM_SETTLE_MS); /* let the PLL lock before trusting the level read */
                tea5767_read_status(&level, &stereo);
            }

            with_view_model(
                instance->view,
                SpiritViewModel * model,
                {
                    model->source = current_source;
                    model->fm_connected = connected;
                    if(connected) {
                        model->freq_hz = fm_freq;
                        model->fm_level = level;
                        model->fm_stereo = stereo;
                    }
                },
                true);

            uint32_t remaining_ms = step_ms > FM_SETTLE_MS ? step_ms - FM_SETTLE_MS : 0;
            furi_delay_ms(connected ? remaining_ms : 300);
        }
    }

    spirit_release(current_source, rx_active, fm_freq);
    return 0;
}

static void spirit_view_draw_callback(Canvas* canvas, void* model) {
    SpiritViewModel* m = model;
    canvas_clear(canvas);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, w / 2, 1, AlignCenter, AlignTop, "SPIRIT BOX");
    canvas_draw_str_aligned(
        canvas,
        w / 2,
        11,
        AlignCenter,
        AlignTop,
        m->source == SpiritSourceFm ? "FM SWEEP" : "RF SWEEP");

    if(m->source == SpiritSourceFm && !m->fm_connected) {
        /* No ACK from the TEA5767 on the bus - module not wired/powered or
         * the switch is in the wrong position. Deliberately skip the
         * frequency/dial display (it would show a stale value). */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, w / 2, h / 2 - 5, AlignCenter, AlignCenter, "NO");
        canvas_draw_str_aligned(canvas, w / 2, h / 2 + 9, AlignCenter, AlignCenter, "MODULE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, "OK:MUTE");
        return;
    }

    char freq_buf[16];
    snprintf(
        freq_buf, sizeof(freq_buf), "%lu.%02lu", m->freq_hz / 1000000, (m->freq_hz / 10000) % 100);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, w / 2, h / 2 - 14, AlignCenter, AlignCenter, freq_buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, w / 2, h / 2, AlignCenter, AlignCenter, "MHz");

    /* Dial: a vertical-safe horizontal bar spanning the swept band, with a
     * tick mark at the current frequency's position. */
    uint32_t band_min = m->source == SpiritSourceFm ? FM_FREQ_MIN_HZ : SUBGHZ_FREQ_MIN_HZ;
    uint32_t band_max = m->source == SpiritSourceFm ? FM_FREQ_MAX_HZ : SUBGHZ_FREQ_MAX_HZ;
    int dial_y = h / 2 + 16;
    int dial_w = (int)w - 12;
    int dial_x = 6;
    canvas_draw_line(canvas, dial_x, dial_y, dial_x + dial_w, dial_y);
    float pos = (float)(m->freq_hz - band_min) / (float)(band_max - band_min);
    int tick_x = dial_x + (int)(pos * (float)dial_w);
    canvas_draw_box(canvas, tick_x - 1, dial_y - 4, 3, 8);

    int status_y = dial_y + 10;
    if(m->source == SpiritSourceFm) {
        char lvl_buf[16];
        snprintf(lvl_buf, sizeof(lvl_buf), "LVL%u %s", m->fm_level, m->fm_stereo ? "ST" : "MO");
        if(m->fm_level >= 8) {
            /* Inverted pill behind the text instead of extra words: stays
             * compact on the 64px-wide vertical canvas. */
            canvas_draw_box(canvas, dial_x, status_y - 1, dial_w, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, w / 2, status_y, AlignCenter, AlignTop, lvl_buf);
        canvas_set_color(canvas, ColorBlack);
    } else {
        char rssi_buf[12];
        snprintf(rssi_buf, sizeof(rssi_buf), "%.0fdBm", (double)m->rssi_dbm);
        if(m->hit) {
            canvas_draw_box(canvas, dial_x, status_y - 1, dial_w, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, w / 2, status_y, AlignCenter, AlignTop, rssi_buf);
        canvas_set_color(canvas, ColorBlack);
    }

    char step_buf[16];
    snprintf(step_buf, sizeof(step_buf), "%lums %s", m->step_ms, m->muted ? "MUTE" : "");
    canvas_draw_str_aligned(canvas, w / 2, h - 17, AlignCenter, AlignBottom, "+- STEP");
    canvas_draw_str_aligned(canvas, w / 2, h - 9, AlignCenter, AlignBottom, step_buf);
    canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, "OK: MUTE");
}

static bool spirit_view_input_callback(InputEvent* event, void* context) {
    SpiritView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            instance->view,
            SpiritViewModel * model,
            {
                int32_t step = (int32_t)model->step_ms;
                step += (event->key == InputKeyUp) ? SPIRIT_STEP_INC_MS : -SPIRIT_STEP_INC_MS;
                if(step < SPIRIT_STEP_MIN_MS) step = SPIRIT_STEP_MIN_MS;
                if(step > SPIRIT_STEP_MAX_MS) step = SPIRIT_STEP_MAX_MS;
                model->step_ms = (uint32_t)step;
            },
            true);
        /* The worker thread re-reads step_ms every loop iteration, so the
         * new value takes effect on the next hop. */
        return true;
    }

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(event->type != InputTypeShort) return true; /* swallow repeat, avoid flip-flopping */
        instance->requested_source =
            instance->requested_source == SpiritSourceStatic ? SpiritSourceFm : SpiritSourceStatic;
        return true;
    }

    if(event->key == InputKeyOk) {
        if(event->type != InputTypeShort) return true; /* swallow repeat, avoid flip-flopping */
        with_view_model(
            instance->view, SpiritViewModel * model, { model->muted = !model->muted; }, true);
        return true;
    }

    return false;
}

static void spirit_view_enter_callback(void* context) {
    SpiritView* instance = context;

    uint32_t step_ms = 170; /* matches the SB7 Pro's own recommended default sweep rate */
    instance->requested_source = SpiritSourceStatic;
    with_view_model(
        instance->view,
        SpiritViewModel * model,
        {
            model->source = SpiritSourceStatic;
            model->freq_hz = SUBGHZ_FREQ_MIN_HZ;
            model->rssi_dbm = -120.0f;
            model->hit = false;
            model->fm_level = 0;
            model->fm_stereo = false;
            model->fm_connected = false;
            model->step_ms = step_ms;
            model->muted = false;
        },
        true);

    instance->running = true;
    furi_thread_start(instance->thread);
}

static void spirit_view_exit_callback(void* context) {
    SpiritView* instance = context;
    instance->running = false;
    furi_thread_join(
        instance->thread); /* blocks until the worker releases everything and returns */
}

SpiritView* spirit_view_alloc(void) {
    SpiritView* instance = malloc(sizeof(SpiritView));
    instance->view = view_alloc();
    instance->running = false;
    instance->requested_source = SpiritSourceStatic;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SpiritViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, spirit_view_draw_callback);
    view_set_input_callback(instance->view, spirit_view_input_callback);
    view_set_enter_callback(instance->view, spirit_view_enter_callback);
    view_set_exit_callback(instance->view, spirit_view_exit_callback);

    instance->thread =
        furi_thread_alloc_ex("SpiritBoxWorker", SPIRIT_WORKER_STACK_SIZE, spirit_worker, instance);

    return instance;
}

void spirit_view_free(SpiritView* instance) {
    furi_thread_free(instance->thread);
    view_free(instance->view);
    free(instance);
}

View* spirit_view_get_view(SpiritView* instance) {
    return instance->view;
}
