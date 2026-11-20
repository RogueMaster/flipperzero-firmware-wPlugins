#include "view_emf.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <gui/elements.h>
#include <stdio.h>
#include <math.h>

#define EMF_POLL_PERIOD_MS 60

/* ADC wiring: Hall-effect sensor (SS49E) on physical pin 16 / PC0.
 * STM32WB55 maps PC0 -> ADC channel 1 (PCn -> ADC channel n+1 for this bank). */
#define EMF_ADC_GPIO    (&gpio_ext_pc0)
#define EMF_ADC_CHANNEL FuriHalAdcChannel1

/* The Hall sensor's actual signal (a magnet approaching) is a DC-ish shift
 * in output voltage, not high-frequency jitter - a fast back-to-back
 * peak-to-peak read (like REM Pod's antenna uses, which genuinely IS noisy
 * sample-to-sample) saw no jitter at all for a real magnet, since nothing
 * is changing THAT fast. The real quantity to track is deviation from a
 * baseline level captured once when this mode starts: average a handful
 * of samples (for noise reduction, not speed) each tick, and compare
 * against that baseline. Still no per-sample delay and no oversampling
 * (same as the fix that stopped the earlier Goertzel version from hanging
 * the device) - just a different, physically correct quantity. */
#define EMF_GPIO_BASELINE_SAMPLE_COUNT 32 /* averaged once, when External mode starts */
#define EMF_GPIO_TICK_SAMPLE_COUNT     8 /* averaged every poll, for noise reduction */

/* Calibration thresholds on the deviation from baseline (mV) - tune these
 * to your actual sensor/environment; real sensitivity depends heavily on
 * the specific SS49E unit and how close it sits to wiring/transformers. */
#define EMF_GPIO_THRESH_1_MV 2.0f
#define EMF_GPIO_THRESH_2_MV 6.0f
#define EMF_GPIO_THRESH_3_MV 15.0f
#define EMF_GPIO_THRESH_4_MV 35.0f

/* "Internal" mode: no wiring needed. We do NOT use the NFC chip - its
 * field-detect only reacts to an active 13.56MHz NFC reader nearby (a
 * phone, another reader), not to generic magnetic/electric fields, so it
 * never moved in practice. Instead we reuse the real CC1101 sub-GHz radio
 * (same chip Spirit Box/Radio Scanner use) and read its RSSI across a
 * handful of fixed frequencies as a genuine ambient-RF-noise gauge: real
 * nearby electronics, switching power supplies, and RF devices DO move
 * this reading; a perfectly quiet RF environment sits near the chip's
 * noise floor (~ -100dBm or lower). */
#define EMF_RF_FREQ_MIN_HZ 300000000UL
#define EMF_RF_FREQ_MAX_HZ 348000000UL
#define EMF_RF_STEPS       8
#define EMF_RF_STEP_HZ     ((EMF_RF_FREQ_MAX_HZ - EMF_RF_FREQ_MIN_HZ) / EMF_RF_STEPS)
#define EMF_RF_SETTLE_MS   5

#define EMF_RF_THRESH_1_DBM -95.0f
#define EMF_RF_THRESH_2_DBM -85.0f
#define EMF_RF_THRESH_3_DBM -75.0f
#define EMF_RF_THRESH_4_DBM -60.0f

#define EMF_WORKER_STACK_SIZE 2048

/* Calibration margin: a user-adjustable offset (UP/DOWN), in the same unit
 * as the raw reading, subtracted from it BEFORE thresholding. This exists
 * because real ambient noise pickup (mains hum riding on a floating sensor
 * node, nearby switching supplies, etc.) can sit well above the fixed
 * thresholds below even at rest, pinning the meter at RED forever. The
 * margin re-zeroes against that real baseline WITHOUT capping how high the
 * meter can still read: if the signal then genuinely increases well past
 * that baseline, there's still full headroom up to RED above it - unlike
 * an earlier version that subtracted a small fixed number of severity
 * steps from the final color, which capped the color at `4 - margin` no
 * matter how strong the real signal got. (An even earlier version of THIS
 * raw-value approach was suspected of hanging the device; that was
 * actually the ADC sampling method below, not this subtraction - confirmed
 * by the hang persisting after reverting to the simple step-based margin,
 * and going away only once the sampling method itself was fixed.) */
#define EMF_MARGIN_MV_STEP 10.0f
#define EMF_MARGIN_MV_MAX  500.0f
#define EMF_MARGIN_DB_STEP 2.0f
#define EMF_MARGIN_DB_MAX  60.0f

typedef enum {
    EmfSourceInternal, /* ambient RF noise via CC1101 RSSI */
    EmfSourceExternal, /* Hall sensor via GPIO ADC */
} EmfSource;

typedef struct {
    EmfSource source;
    float display_value; /* dBm (internal) or millivolts (external) - RAW, unaffected by margin */
    int level; /* 0 (normal/green) .. 4 (critical/red) - computed AFTER margin is subtracted */
    float margin_mv; /* calibration margin for EmfSourceExternal */
    float margin_db; /* calibration margin for EmfSourceInternal */
} EmfViewModel;

struct EmfView {
    View* view;
    FuriThread* thread;
    volatile bool running;
    volatile EmfSource requested_source;
    /* Calibration margin "requests" from the GUI/input thread, mirroring
     * requested_source above: plain volatile fields the worker thread
     * reads directly, instead of going through the locked view model. Only
     * the worker thread writes the view model (see emf_worker) - routing
     * the margin through with_view_model from the INPUT callback as well
     * broke that single-writer convention once; keep it this way. */
    volatile float margin_mv_request;
    volatile float margin_db_request;
};

static int emf_level_from_ripple_mv(float ripple_mv) {
    if(ripple_mv >= EMF_GPIO_THRESH_4_MV) return 4;
    if(ripple_mv >= EMF_GPIO_THRESH_3_MV) return 3;
    if(ripple_mv >= EMF_GPIO_THRESH_2_MV) return 2;
    if(ripple_mv >= EMF_GPIO_THRESH_1_MV) return 1;
    return 0;
}

/* Back-to-back raw reads (no delay, no oversampling), averaged for noise
 * reduction only - not a waveform capture. Used both for the one-off
 * baseline and for each tick's quick sample. */
static float emf_measure_average_mv(FuriHalAdcHandle* adc_handle, int count) {
    uint32_t sum_raw = 0;
    for(int i = 0; i < count; i++) {
        sum_raw += furi_hal_adc_read(adc_handle, EMF_ADC_CHANNEL);
    }
    uint16_t avg_raw = (uint16_t)(sum_raw / (uint32_t)count);
    /* furi_hal_adc_convert_to_voltage already returns mV (per its SDK
     * header), not volts - the old Goertzel-era code here multiplied by
     * 1000 as if it were volts, inflating every value by 1000x. It went
     * unnoticed because the old thresholds just happened to get re-tuned
     * against that already-wrong scale; now that the pipeline is simpler,
     * values in the thousands immediately expose it. */
    return furi_hal_adc_convert_to_voltage(adc_handle, avg_raw);
}

static int emf_level_from_rssi(float rssi_dbm) {
    if(rssi_dbm >= EMF_RF_THRESH_4_DBM) return 4;
    if(rssi_dbm >= EMF_RF_THRESH_3_DBM) return 3;
    if(rssi_dbm >= EMF_RF_THRESH_2_DBM) return 2;
    if(rssi_dbm >= EMF_RF_THRESH_1_DBM) return 1;
    return 0;
}

/* Sets the physical RGB LED to match the active bar level. */
static void emf_update_physical_led(int level) {
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    switch(level) {
    case 0:
    case 1:
        furi_hal_light_set(LightGreen, 255);
        break;
    case 2:
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 180);
        break;
    case 3:
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 60);
        break;
    case 4:
    default:
        furi_hal_light_set(LightRed, 255);
        break;
    }
}

static void emf_acquire(EmfSource source, FuriHalAdcHandle** adc_handle) {
    if(source == EmfSourceInternal) {
        furi_hal_subghz_reset();
        furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    } else {
        furi_hal_gpio_init(EMF_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
        *adc_handle = furi_hal_adc_acquire();
        /* Same ADC config as REM Pod's proven-stable reading (see
         * view_rem.c) - no oversampling, fastest sampling time. The
         * default furi_hal_adc_configure() instead applies 64x hardware
         * oversampling per read, which both (a) is just wrong here: it
         * averages away the very AC ripple the Goertzel filter below needs
         * raw samples of, and (b) does ~64x more ADC conversions than REM
         * Pod per read across this loop's 128 reads - the likely reason
         * this path, unlike REM Pod's, was hanging the whole device. */
        furi_hal_adc_configure_ex(
            *adc_handle,
            FuriHalAdcScale2048,
            FuriHalAdcClockSync64,
            FuriHalAdcOversampleNone,
            FuriHalAdcSamplingtime2_5);
    }
}

static void emf_release(EmfSource source, FuriHalAdcHandle** adc_handle) {
    if(source == EmfSourceInternal) {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    } else {
        furi_hal_gpio_init(EMF_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        if(*adc_handle) {
            furi_hal_adc_release(*adc_handle);
            *adc_handle = NULL;
        }
    }
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

/* Owns the sub-GHz radio AND the ADC exclusively, entirely on this thread -
 * same pattern as Spirit Box / REM Pod, for a consistent, simple lifecycle. */
static int32_t emf_worker(void* context) {
    EmfView* instance = context;
    FuriHalAdcHandle* adc_handle = NULL;

    EmfSource current_source = instance->requested_source;
    emf_acquire(current_source, &adc_handle);
    float emf_baseline_mv = 0.0f; /* set for real on first switch into EmfSourceExternal */

    uint32_t rf_freq = EMF_RF_FREQ_MIN_HZ;

    while(instance->running) {
        EmfSource wanted = instance->requested_source;
        if(wanted != current_source) {
            emf_release(current_source, &adc_handle);
            /* Let the just-released peripheral's clock/power domain fully
             * settle before claiming the next one - speculative, but cheap
             * insurance against a peripheral handoff race between subghz
             * and ADC that could leave furi_hal_adc_read's EOC poll (no
             * timeout per the SDK header) waiting on a conversion that
             * never completes. */
            furi_delay_ms(20);
            current_source = wanted;
            emf_acquire(current_source, &adc_handle);
            if(current_source == EmfSourceExternal) {
                /* Re-zero against whatever ambient baseline this sensor/
                 * environment has right now, so only a CHANGE (e.g. a
                 * magnet approaching) reads as signal. */
                emf_baseline_mv = emf_measure_average_mv(adc_handle, EMF_GPIO_BASELINE_SAMPLE_COUNT);
            }
        }

        if(current_source == EmfSourceInternal) {
            rf_freq += EMF_RF_STEP_HZ;
            if(rf_freq > EMF_RF_FREQ_MAX_HZ) rf_freq = EMF_RF_FREQ_MIN_HZ;

            furi_hal_subghz_idle();
            furi_hal_subghz_set_frequency_and_path(rf_freq);
            furi_hal_subghz_rx();
            furi_delay_ms(EMF_RF_SETTLE_MS);
            float rssi = furi_hal_subghz_get_rssi();

            float margin_db = instance->margin_db_request;
            int level = emf_level_from_rssi(rssi - margin_db);
            emf_update_physical_led(level);
            with_view_model(
                instance->view,
                EmfViewModel * model,
                {
                    model->source = current_source;
                    model->display_value = rssi;
                    model->level = level;
                    model->margin_db = margin_db;
                },
                true);
        } else {
            float sample_mv = emf_measure_average_mv(adc_handle, EMF_GPIO_TICK_SAMPLE_COUNT);
            float deviation_mv = fabsf(sample_mv - emf_baseline_mv);

            float margin_mv = instance->margin_mv_request;
            float calibrated_mv = deviation_mv - margin_mv;
            if(calibrated_mv < 0.0f) calibrated_mv = 0.0f;
            int level = emf_level_from_ripple_mv(calibrated_mv);
            emf_update_physical_led(level);
            with_view_model(
                instance->view,
                EmfViewModel * model,
                {
                    model->source = current_source;
                    model->display_value = deviation_mv;
                    model->level = level;
                    model->margin_mv = margin_mv;
                },
                true);
        }

        furi_delay_ms(EMF_POLL_PERIOD_MS);
    }

    emf_release(current_source, &adc_handle);
    return 0;
}

static void emf_view_draw_callback(Canvas* canvas, void* model) {
    EmfViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_draw_str_aligned(canvas, w / 2, 1, AlignCenter, AlignTop, "EMF METER");
    canvas_draw_str_aligned(
        canvas,
        w / 2,
        11,
        AlignCenter,
        AlignTop,
        m->source == EmfSourceInternal ? "<RF>" : "<ADC>");

    /* Calibration margin indicator: subtracted from the raw reading before
     * it's classified into a level/color (see EMF_MARGIN_* above). Shown
     * even at 0 so the control is discoverable. */
    char cal_buf[16];
    if(m->source == EmfSourceInternal) {
        snprintf(cal_buf, sizeof(cal_buf), "CAL -%.0fdB", (double)m->margin_db);
    } else {
        snprintf(cal_buf, sizeof(cal_buf), "CAL -%.0fmV", (double)m->margin_mv);
    }
    canvas_draw_str_aligned(canvas, w / 2, 20, AlignCenter, AlignTop, cal_buf);

    /* 5 stacked blocks, bottom = green/normal, top = red/critical.
     * Monochrome display: "lit" = filled box, "unlit" = outline only.
     * The physical RGB LED (see emf_update_physical_led) carries the real color. */
    const int block_count = 5;
    const int top = 31;
    const int bottom = (int)h - 22;
    const int gap = 2;
    const int block_h = (bottom - top - gap * (block_count - 1)) / block_count;
    const int block_w = (int)w - 24;
    const int x = ((int)w - block_w) / 2;

    for(int i = 0; i < block_count; i++) {
        int y = top + i * (block_h + gap);
        int severity = block_count - 1 - i; /* top(i=0) => severity 4, bottom(i=4) => severity 0 */
        bool lit = severity <= m->level;
        if(lit) {
            canvas_draw_box(canvas, x, y, block_w, block_h);
        } else {
            canvas_draw_frame(canvas, x, y, block_w, block_h);
        }
    }

    static const char* level_words[5] = {"GREEN", "GREEN", "YELLOW", "ORANGE", "RED"};
    char val_buf[16];
    if(m->source == EmfSourceInternal) {
        snprintf(val_buf, sizeof(val_buf), "%.0fdBm", (double)m->display_value);
    } else {
        snprintf(val_buf, sizeof(val_buf), "%.1fmV", (double)m->display_value);
    }
    canvas_draw_str_aligned(canvas, w / 2, h - 19, AlignCenter, AlignBottom, "UP/DN:CAL");
    canvas_draw_str_aligned(canvas, w / 2, h - 10, AlignCenter, AlignBottom, level_words[m->level]);
    canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, val_buf);
}

static bool emf_view_input_callback(InputEvent* event, void* context) {
    EmfView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        /* instance->requested_source, not the (worker-owned) view model, is
         * the right thing to read here - same single-writer convention. */
        if(instance->requested_source == EmfSourceInternal) {
            float margin = instance->margin_db_request;
            margin += (event->key == InputKeyUp) ? EMF_MARGIN_DB_STEP : -EMF_MARGIN_DB_STEP;
            if(margin < 0.0f) margin = 0.0f;
            if(margin > EMF_MARGIN_DB_MAX) margin = EMF_MARGIN_DB_MAX;
            instance->margin_db_request = margin;
        } else {
            float margin = instance->margin_mv_request;
            margin += (event->key == InputKeyUp) ? EMF_MARGIN_MV_STEP : -EMF_MARGIN_MV_STEP;
            if(margin < 0.0f) margin = 0.0f;
            if(margin > EMF_MARGIN_MV_MAX) margin = EMF_MARGIN_MV_MAX;
            instance->margin_mv_request = margin;
        }
        /* The worker thread re-reads the margin every loop iteration, so
         * the new value takes effect on the next sample. */
        return true;
    }

    if(event->type != InputTypeShort) return false; /* only UP/DOWN repeat; LEFT/RIGHT is a single toggle */

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        instance->requested_source =
            instance->requested_source == EmfSourceInternal ? EmfSourceExternal :
                                                                EmfSourceInternal;
        return true;
    }
    /* Back is intentionally not consumed: it must bubble up to the app's
     * navigation callback so it returns to the main menu. */
    return false;
}

static void emf_view_enter_callback(void* context) {
    EmfView* instance = context;
    instance->requested_source = EmfSourceInternal;
    instance->margin_mv_request = 0.0f;
    instance->margin_db_request = 0.0f;
    with_view_model(
        instance->view,
        EmfViewModel * model,
        {
            model->source = EmfSourceInternal;
            model->display_value = 0.0f;
            model->level = 0;
            model->margin_mv = 0.0f;
            model->margin_db = 0.0f;
        },
        true);
    instance->running = true;
    furi_thread_start(instance->thread);
}

static void emf_view_exit_callback(void* context) {
    EmfView* instance = context;
    instance->running = false;
    furi_thread_join(instance->thread); /* blocks until the worker releases everything and returns */
}

EmfView* emf_view_alloc(void) {
    EmfView* instance = malloc(sizeof(EmfView));
    instance->view = view_alloc();
    instance->running = false;
    instance->requested_source = EmfSourceInternal;
    instance->margin_mv_request = 0.0f;
    instance->margin_db_request = 0.0f;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(EmfViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, emf_view_draw_callback);
    view_set_input_callback(instance->view, emf_view_input_callback);
    view_set_enter_callback(instance->view, emf_view_enter_callback);
    view_set_exit_callback(instance->view, emf_view_exit_callback);

    instance->thread =
        furi_thread_alloc_ex("EmfMeterWorker", EMF_WORKER_STACK_SIZE, emf_worker, instance);

    return instance;
}

void emf_view_free(EmfView* instance) {
    furi_thread_free(instance->thread);
    view_free(instance->view);
    free(instance);
}

View* emf_view_get_view(EmfView* instance) {
    return instance->view;
}
