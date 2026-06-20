#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

/*
 * KT0803L FM Transmitter FAP for Flipper Zero (Momentum firmware)
 *
 * KT0803L I2C write address: 0x7C (7-bit: 0x3E)
 *
 * Register map (from KT0803L datasheet v1.3):
 *   REG 0x00: CHSEL[8:1]
 *   REG 0x01: RFGAIN[1:0] | PGA[2:0] | CHSEL[11:9]
 *   REG 0x02: CHSEL[0] | RFGAIN[3] | -- | -- | MUTE | PLTADJ | -- | PHTCNST
 *   REG 0x0B: Standby | -- | PDPA | -- | -- | AUTO_PADN | -- | --
 *   REG 0x0E: -- | -- | -- | -- | -- | -- | PA_BIAS | --
 *   REG 0x13: RFGAIN[2] | -- | -- | -- | -- | -- | -- | --
 *
 * CHSEL formula (datasheet §4.1):
 *   CHSEL[11:0] = round(freq_MHz * 20)
 *   Layout: CHSEL[11:9] = REG0x01[2:0]
 *           CHSEL[8:1]  = REG0x00[7:0]
 *           CHSEL[0]    = REG0x02[7]
 */

// i2c stuff
#define KT0803_I2C_BUS     (&furi_hal_i2c_handle_external)
#define KT0803_I2C_ADDR    (0x3E) // 7-bit address (0x7C >> 1)
#define KT0803_I2C_TIMEOUT (50) // ms

// registers
#define KT0803_REG_CHSEL_HIGH 0x00
#define KT0803_REG_RFGAIN_PGA 0x01
#define KT0803_REG_CHSEL_LOW  0x02
#define KT0803_REG_MISC       0x0B
#define KT0803_REG_PA_BIAS    0x0E
#define KT0803_REG_RFGAIN2    0x13

// REG0x02 bits
#define KT0803_MUTE_BIT    (1 << 3)
#define KT0803_PHTCNST_BIT (1 << 0) // 0=75us (USA/Japan), 1=50us (Europe)
#define KT0803_RFGAIN3_BIT (1 << 6)
#define KT0803_PLTADJ_BIT  (1 << 2)

// REG0x0B bits
#define KT0803_STANDBY_BIT (1 << 7)
#define KT0803_PDPA_BIT    (1 << 5)

// max power config - RFGAIN=0b1111 (108 dBuV), PA_BIAS=1 pushes it to 112.5 dBuV
// RFGAIN[3:0] is split across 3 registers which is annoying but whatever
#define KT0803_RFGAIN_10_BITS (0b11 << 6) // bits 7:6 of REG0x01
#define KT0803_PA_BIAS_BIT    (1 << 1) // bit 1 of REG0x0E

typedef struct {
    const char* name;
    float min_freq;
    float max_freq;
    float step;
    bool phtcnst; // true = 50us (Europe/Australia), false = 75us (USA/Japan)
} RegionConfig;

static const RegionConfig regions[] = {
    {"Europe", 87.5f, 108.0f, 0.1f, true},
    {"USA", 87.9f, 107.9f, 0.2f, false},
    {"Japan", 76.0f, 95.0f, 0.1f, false},
};
static const size_t REGION_COUNT = COUNT_OF(regions);

typedef enum {
    SCREEN_MAIN,
    SCREEN_REGION,
    SCREEN_FREQUENCY,
    SCREEN_INIT,
} AppScreen;

typedef struct {
    AppScreen current_screen;
    size_t region_index;
    float frequency;
    bool init_in_progress;
    char init_status[64];
} FMTXState;

typedef struct {
    FMTXState state;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* state_mutex;
    NotificationApp* notifications;
} FMTXApp;

// write one byte to a register, bus must NOT be acquired before calling
static bool kt0803_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    furi_hal_i2c_acquire(KT0803_I2C_BUS);
    bool ok = furi_hal_i2c_tx(KT0803_I2C_BUS, KT0803_I2C_ADDR << 1, buf, 2, KT0803_I2C_TIMEOUT);
    furi_hal_i2c_release(KT0803_I2C_BUS);
    return ok;
}

static bool kt0803_is_present(void) {
    furi_hal_i2c_acquire(KT0803_I2C_BUS);
    bool present =
        furi_hal_i2c_is_device_ready(KT0803_I2C_BUS, KT0803_I2C_ADDR << 1, KT0803_I2C_TIMEOUT);
    furi_hal_i2c_release(KT0803_I2C_BUS);
    return present;
}

/*
 * set frequency on the kt0803l
 *
 * encoding (datasheet §4.1):
 *   CHSEL[11:0] = round(freq_MHz * 20)
 *   REG0x00 = CHSEL[8:1]
 *   REG0x01 = RFGAIN[1:0] | PGA[2:0]=000 | CHSEL[11:9]
 *   REG0x02 = CHSEL[0] | RFGAIN[3]=1 | MUTE=0 | PLTADJ=0 | PHTCNST
 *
 * write order matters - chip latches on the last write (REG0x02)
 */
static bool kt0803_set_frequency(float freq_mhz, bool europe_preemphasis) {
    uint16_t chsel = (uint16_t)((freq_mhz * 20.0f) + 0.5f);

    uint8_t reg00 = (uint8_t)((chsel >> 1) & 0xFF);
    uint8_t reg01 = KT0803_RFGAIN_10_BITS | (uint8_t)((chsel >> 9) & 0x07);
    uint8_t reg02 = (uint8_t)(((chsel & 0x01) << 7)) | KT0803_RFGAIN3_BIT |
                    (europe_preemphasis ? KT0803_PHTCNST_BIT : 0);

    if(!kt0803_write_reg(KT0803_REG_CHSEL_HIGH, reg00)) return false;
    if(!kt0803_write_reg(KT0803_REG_RFGAIN_PGA, reg01)) return false;
    if(!kt0803_write_reg(KT0803_REG_CHSEL_LOW, reg02)) return false;

    return true;
}

// wake from standby and turn PA on, call this before set_frequency
static bool kt0803_power_on(void) {
    // REG0x13: RFGAIN[2]=1 (bit 7), chip default is already 0x80 but set it anyway
    if(!kt0803_write_reg(KT0803_REG_RFGAIN2, 0x80)) return false;
    // REG0x0E: PA_BIAS=1 bumps max output from 108 to 112.5 dBuV
    if(!kt0803_write_reg(KT0803_REG_PA_BIAS, KT0803_PA_BIAS_BIT)) return false;
    return kt0803_write_reg(KT0803_REG_MISC, 0x00);
}

static void fmtx_draw_callback(Canvas* canvas, void* ctx) {
    FMTXApp* app = (FMTXApp*)ctx;

    if(furi_mutex_acquire(app->state_mutex, 25) != FuriStatusOk) {
        return;
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 2, 2, 124, 60, 3);

    switch(app->state.current_screen) {
    case SCREEN_MAIN:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "KT0803 FM TX");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Set frequency & transmit");
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, "audio via I2C");

        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "[ Configure ]");
        break;

    case SCREEN_REGION:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Select Region");

        canvas_set_font(canvas, FontSecondary);
        for(size_t i = 0; i < REGION_COUNT; i++) {
            if(i == app->state.region_index) {
                canvas_draw_str(canvas, 10, 28 + i * 14, ">");
            }
            canvas_draw_str_aligned(canvas, 20, 20 + i * 14, AlignLeft, AlignTop, regions[i].name);
        }
        // hint line removed, it overlapped japan text
        break;

    case SCREEN_FREQUENCY: {
        const RegionConfig* region = &regions[app->state.region_index];
        char freq_text[32];

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Set Frequency");

        canvas_set_font(canvas, FontSecondary);
        snprintf(freq_text, sizeof(freq_text), "Freq: %.1f MHz", (double)app->state.frequency);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, freq_text);

        snprintf(
            freq_text,
            sizeof(freq_text),
            "(%s) %.1f-%.1f",
            region->name,
            (double)region->min_freq,
            (double)region->max_freq);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, freq_text);

        canvas_draw_str(canvas, 5, 57, "L:Back");
        canvas_draw_str(canvas, 45, 57, "U/D:Set");
        canvas_draw_str(canvas, 90, 57, "OK:Next");
        break;
    }

    case SCREEN_INIT:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Init Status");

        canvas_set_font(canvas, FontSecondary);
        if(app->state.init_in_progress) {
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Initializing...");
        } else {
            // split status into 2 lines of 31 chars each
            char line1[32], line2[32];
            size_t len = strlen(app->state.init_status);
            strncpy(line1, app->state.init_status, 31);
            line1[31] = '\0';
            canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, line1);

            if(len > 31) {
                strncpy(line2, app->state.init_status + 31, 31);
                line2[31] = '\0';
                canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignTop, line2);
            }
        }
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "[ Start Init ]");
        break;
    }

    furi_mutex_release(app->state_mutex);
}

static void fmtx_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FMTXApp* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

int32_t fmtx_i2c_init_app(void* p) {
    UNUSED(p);

    FMTXApp* app = malloc(sizeof(FMTXApp));
    furi_assert(app);

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->state.current_screen = SCREEN_MAIN;
    app->state.region_index = 0; // europe default
    app->state.frequency = 100.0f;
    app->state.init_in_progress = false;
    strncpy(app->state.init_status, "Ready", sizeof(app->state.init_status) - 1);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, fmtx_draw_callback, app);
    view_port_input_callback_set(app->view_port, fmtx_input_callback, app);

    // use RECORD_* macros (momentum sdk)
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) continue;

        // back always exits regardless of screen
        if(event.key == InputKeyBack && event.type == InputTypeShort) {
            if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                if(app->state.current_screen == SCREEN_MAIN) {
                    furi_mutex_release(app->state_mutex);
                    running = false;
                } else {
                    app->state.current_screen = SCREEN_MAIN;
                    furi_mutex_release(app->state_mutex);
                }
            }
            view_port_update(app->view_port);
            continue;
        }

        if(event.type != InputTypeShort && event.type != InputTypeRepeat) {
            continue;
        }

        if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        switch(app->state.current_screen) {
        case SCREEN_MAIN:
            if(event.key == InputKeyOk && event.type == InputTypeShort) {
                app->state.current_screen = SCREEN_REGION;
            }
            break;

        case SCREEN_REGION:
            if(event.key == InputKeyUp) {
                if(app->state.region_index > 0) app->state.region_index--;
            } else if(event.key == InputKeyDown) {
                if(app->state.region_index < REGION_COUNT - 1) app->state.region_index++;
            } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                // clamp freq to new region bounds
                const RegionConfig* r = &regions[app->state.region_index];
                if(app->state.frequency < r->min_freq) app->state.frequency = r->min_freq;
                if(app->state.frequency > r->max_freq) app->state.frequency = r->max_freq;
                app->state.current_screen = SCREEN_FREQUENCY;
            }
            break;

        case SCREEN_FREQUENCY: {
            const RegionConfig* region = &regions[app->state.region_index];
            if(event.key == InputKeyUp) {
                app->state.frequency += region->step;
                if(app->state.frequency > region->max_freq)
                    app->state.frequency = region->max_freq;
                // round to avoid float drift
                app->state.frequency =
                    ((float)(int)((app->state.frequency / region->step) + 0.5f)) * region->step;
            } else if(event.key == InputKeyDown) {
                app->state.frequency -= region->step;
                if(app->state.frequency < region->min_freq)
                    app->state.frequency = region->min_freq;
                app->state.frequency =
                    ((float)(int)((app->state.frequency / region->step) + 0.5f)) * region->step;
            } else if(event.key == InputKeyLeft && event.type == InputTypeShort) {
                app->state.current_screen = SCREEN_REGION;
            } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                app->state.current_screen = SCREEN_INIT;
                strncpy(
                    app->state.init_status,
                    "Press OK to start",
                    sizeof(app->state.init_status) - 1);
            }
            break;
        }

        case SCREEN_INIT:
            if(event.key == InputKeyOk && event.type == InputTypeShort) {
                float freq = app->state.frequency;
                size_t reg_idx = app->state.region_index;
                app->state.init_in_progress = true;
                strncpy(
                    app->state.init_status, "Scanning I2C...", sizeof(app->state.init_status) - 1);

                furi_mutex_release(app->state_mutex);
                view_port_update(app->view_port);

                bool present = kt0803_is_present();

                if(!present) {
                    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                        strncpy(
                            app->state.init_status,
                            "No KT0803 found!Connect module (SCL/SDA)",
                            sizeof(app->state.init_status) - 1);
                        app->state.init_in_progress = false;
                        furi_mutex_release(app->state_mutex);
                    }
                    notification_message(app->notifications, &sequence_error);
                } else {
                    bool ok = kt0803_power_on();
                    if(ok) {
                        furi_delay_ms(5); // let it settle after wakeup
                        ok = kt0803_set_frequency(freq, regions[reg_idx].phtcnst);
                    }

                    if(furi_mutex_acquire(app->state_mutex, FuriWaitForever) == FuriStatusOk) {
                        if(ok) {
                            snprintf(
                                app->state.init_status,
                                sizeof(app->state.init_status),
                                "OK! %.1f MHz (%s)",
                                (double)freq,
                                regions[reg_idx].name);
                            notification_message(app->notifications, &sequence_blink_green_100);
                        } else {
                            strncpy(
                                app->state.init_status,
                                "I2C write failed!Check wiring",
                                sizeof(app->state.init_status) - 1);
                            notification_message(app->notifications, &sequence_error);
                        }
                        app->state.init_in_progress = false;
                        furi_mutex_release(app->state_mutex);
                    }
                }

                view_port_update(app->view_port);
                continue;
            }
            break;
        }

        furi_mutex_release(app->state_mutex);
        view_port_update(app->view_port);
    }

    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->state_mutex);
    free(app);

    return 0;
}
