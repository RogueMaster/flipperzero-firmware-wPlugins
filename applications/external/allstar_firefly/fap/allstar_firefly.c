/**
 * allstar_firefly.c
 * Allstar Firefly 318ALD31K Gate Remote — SubGHz FAP
 *
 * See allstar_firefly.h for full protocol documentation.
 */

#include "allstar_firefly.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ─── Decode symbols → DIP string ────────────────────────────────────────── */

bool af_decode_symbols(const volatile uint8_t* syms, char* dip) {
    for(int i = 0; i < AF_BIT_COUNT; i++) {
        uint8_t a = syms[i * 2];
        uint8_t b = syms[i * 2 + 1];
        if      (a == 1 && b == 1) dip[i] = '+';
        else if (a == 0 && b == 0) dip[i] = '-';
        else if (a == 1 && b == 0) dip[i] = '0';
        else return false; /* invalid (L,H) pair — discard frame */
    }
    dip[AF_BIT_COUNT] = '\0';
    return true;
}

/* ─── Build TX pulse buffer ───────────────────────────────────────────────── */
/*
 * Flat array of durations alternating HIGH/LOW starting with HIGH.
 * TX callback: even index = HIGH (pulse), odd index = LOW (gap).
 *
 * Per bit:
 *   '+'  -> [LONG_PULSE, SHORT_GAP, LONG_PULSE, SHORT_GAP]
 *   '-'  -> [SHORT_PULSE, LONG_GAP, SHORT_PULSE, LONG_GAP]
 *   '0'  -> [LONG_PULSE, SHORT_GAP, SHORT_PULSE, LONG_GAP]
 *
 * Last gap of each frame is stretched to AF_INTERFRAME_US.
 */
void af_build_tx_buf(AllstarApp* app) {
    uint32_t* buf = app->tx_buf;
    uint32_t  pos = 0;

    for(int rep = 0; rep < AF_TX_REPEAT; rep++) {
        for(int bit = 0; bit < AF_BIT_COUNT; bit++) {
            bool     last = (bit == AF_BIT_COUNT - 1);
            char     c    = app->dip[bit];
            uint32_t p0, g0, p1, g1;

            if(c == '+') {
                p0 = AF_LONG_PULSE_US;  g0 = AF_SHORT_GAP_US;
                p1 = AF_LONG_PULSE_US;  g1 = AF_SHORT_GAP_US;
            } else if(c == '-') {
                p0 = AF_SHORT_PULSE_US; g0 = AF_LONG_GAP_US;
                p1 = AF_SHORT_PULSE_US; g1 = AF_LONG_GAP_US;
            } else { /* '0' */
                p0 = AF_LONG_PULSE_US;  g0 = AF_SHORT_GAP_US;
                p1 = AF_SHORT_PULSE_US; g1 = AF_LONG_GAP_US;
            }

            if(last) g1 = AF_INTERFRAME_US;

            buf[pos++] = p0;
            buf[pos++] = g0;
            buf[pos++] = p1;
            buf[pos++] = g1;
        }
    }

    app->tx_size = pos;
}

/* ─── RX callback (IRQ context) ──────────────────────────────────────────── */
/*
 * CC1101 GDO0 = demodulated OOK:  HIGH = carrier,  LOW = silence.
 *
 *   level=false (falling edge): duration = pulse that just ended
 *   level=true  (rising  edge): duration = gap  that just ended
 */
static void rx_callback(bool level, uint32_t duration, void* ctx) {
    AllstarApp* app = (AllstarApp*)ctx;

    if(app->rx_ready) return; /* gate: main thread hasn't consumed last frame */

    if(!level) {
        /* Falling edge — classify and range-check the pulse we just measured.
         *
         * Only two valid pulse ranges exist:
         *   Short (L): AF_SHORT_PULSE_MIN..AF_SHORT_PULSE_MAX  (~530 µs)
         *   Long  (H): AF_LONG_PULSE_MIN..AF_LONG_PULSE_MAX    (~4045 µs)
         *
         * Anything in the dead zone or overlong is noise — reset immediately
         * rather than accumulating a garbage 18-symbol sequence.
         */
        if(app->rx_state == RxState_Receiving) {
            uint8_t sym;
            if(duration >= AF_LONG_PULSE_MIN && duration <= AF_LONG_PULSE_MAX) {
                sym = 1u; /* H */
            } else if(duration >= AF_SHORT_PULSE_MIN && duration <= AF_SHORT_PULSE_MAX) {
                sym = 0u; /* L */
            } else {
                /* Out-of-range pulse — noise, abort frame and wait for re-sync */
                app->rx_state = RxState_WaitGap;
                app->rx_count = 0;
                return;
            }
            if(app->rx_count < AF_SYM_COUNT) {
                app->rx_syms[app->rx_count++] = sym;
            }
        }
    } else {
        /* Rising edge — gap measurement */
        if(duration >= AF_FRAME_THRESH_US) {
            if(app->rx_state == RxState_Receiving &&
               app->rx_count == AF_SYM_COUNT) {
                /* ── Complete 18-symbol frame ── */
                app->rx_ready = true;

            } else if(app->rx_state == RxState_WaitGap) {
                /* ── Large gap while waiting to sync ──
                 * This is the ED-9 inter-frame gap (or sync gap on first
                 * transmission).  Begin collecting the frame that follows.  */
                app->rx_state = RxState_Receiving;
                app->rx_count = 0;

            } else {
                /* ── Receiving but incomplete (noise mid-frame, or we
                 * joined mid-stream) ── wait for the next clean gap.      */
                app->rx_state = RxState_WaitGap;
                app->rx_count = 0;
            }
        }
    }
}

/* ─── TX callback (IRQ context) ──────────────────────────────────────────── */

static LevelDuration tx_callback(void* ctx) {
    AllstarApp* app = (AllstarApp*)ctx;

    if(app->tx_pos >= app->tx_size) {
        app->tx_done = true;
        return level_duration_reset();
    }

    bool     lv  = (app->tx_pos % 2 == 0); /* even = HIGH, odd = LOW */
    uint32_t dur = app->tx_buf[app->tx_pos++];
    return level_duration_make(lv, dur);
}

/* ─── GDO0 GPIO edge callback (replaces subghz async RX timer) ───────────── */
/*
 * subghz_devices_start_async_rx internally requires the furi_hal SubGHz state
 * machine to be in SubGhzStateIdle.  From a FAP this precondition is fragile
 * and results in silent no-ops.
 *
 * Alternative: put the CC1101 in RX mode via subghz_devices_set_rx (which
 * configures GDO0 as async serial data output per the OOK650Async preset),
 * then register a plain GPIO EXTI interrupt on the GDO0 pin to capture each
 * edge and measure its duration with the DWT cycle counter.
 *
 * callback convention (same as async RX):
 *   level = NEW level after the edge
 *   duration = time of the PREVIOUS level (µs)
 *   → level=false (just went LOW):  duration = pulse duration
 *   → level=true  (just went HIGH): duration = gap duration
 */
static void gdo0_edge_cb(void* ctx) {
    AllstarApp* app = (AllstarApp*)ctx;

    uint32_t now     = DWT->CYCCNT;
    uint32_t elapsed = now - app->rx_last_cyc;
    app->rx_last_cyc = now;

    uint32_t duration_us = elapsed / app->cpu_mhz;
    bool     level       = furi_hal_gpio_read(app->gdo0);

    rx_callback(level, duration_us, app);
}

/* ─── Radio helpers ───────────────────────────────────────────────────────── */

void af_radio_start_rx(AllstarApp* app) {
    app->rx_state = RxState_WaitGap;
    app->rx_count = 0;
    app->rx_ready = false;

    /* Always disarm first — add_int_callback furi_check's the slot is empty;
     * double-registration without this guard crashes the device.       */
    furi_hal_gpio_remove_int_callback(app->gdo0);

    /* Ensure HAL state is Idle before configuring */
    subghz_devices_idle(app->radio);
    subghz_devices_reset(app->radio);
    subghz_devices_load_preset(app->radio, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->radio, AF_FREQ);
    subghz_devices_set_rx(app->radio);   /* CC1101 → RX, GDO0 = demod data */

    /* Arm GPIO edge interrupt for raw timing capture */
    furi_hal_gpio_init(app->gdo0, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedVeryHigh);
    app->rx_last_cyc = DWT->CYCCNT;
    furi_hal_gpio_add_int_callback(app->gdo0, gdo0_edge_cb, app);
}

void af_radio_stop_rx(AllstarApp* app) {
    furi_hal_gpio_remove_int_callback(app->gdo0);
    furi_hal_gpio_init(app->gdo0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    subghz_devices_idle(app->radio);
}

void af_radio_start_tx(AllstarApp* app) {
    app->tx_pos  = 0;
    app->tx_done = false;

    /* Disarm any active RX interrupt before switching to TX */
    furi_hal_gpio_remove_int_callback(app->gdo0);
    furi_hal_gpio_init(app->gdo0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    subghz_devices_idle(app->radio);
    subghz_devices_reset(app->radio);
    subghz_devices_load_preset(app->radio, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->radio, AF_FREQ);
    subghz_devices_start_async_tx(app->radio, tx_callback, app);
}

void af_radio_stop_tx(AllstarApp* app) {
    subghz_devices_stop_async_tx(app->radio);
    subghz_devices_idle(app->radio);
}

/* ─── Draw callback ───────────────────────────────────────────────────────── */

static void draw_cb(Canvas* canvas, void* ctx) {
    AllstarApp* app = (AllstarApp*)ctx;
    canvas_clear(canvas);

    /* Title */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Allstar Firefly");

    /* Mode label — top right */
    canvas_set_font(canvas, FontSecondary);
    const char* mode_lbl =
        (app->mode == AppMode_Listen)   ? "LISTEN" :
        (app->mode == AppMode_Edit)     ? "EDIT"   : "TX";
    canvas_draw_str(canvas,
        128 - canvas_string_width(canvas, mode_lbl), 10, mode_lbl);

    canvas_draw_str(canvas, 0, 20, "318MHz OOK  9-bit trinary");

    /* ── DIP switch row ─────────────────────────────────────────────────── */
    const int DIP_X0  = 27; /* pixel X of first DIP char                   */
    const int DIP_Y   = 35; /* baseline Y                                   */
    const int DIP_SPC = 11; /* pixels between DIP positions                 */

    canvas_draw_str(canvas, 0, DIP_Y, "DIP:");

    for(int i = 0; i < AF_BIT_COUNT; i++) {
        int  cx   = DIP_X0 + i * DIP_SPC;
        char ch[2] = { 0, 0 };
        ch[0] = (app->has_decode || app->mode == AppMode_Edit)
                    ? app->dip[i] : '?';

        bool cur = (app->mode == AppMode_Edit && i == app->cursor);
        if(cur) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, cx - 1, DIP_Y - 8, 9, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, cx, DIP_Y, ch);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, cx, DIP_Y, ch);
        }
    }

    /* Index numbers 1-9 under each DIP position */
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < AF_BIT_COUNT; i++) {
        char num[3];
        snprintf(num, sizeof(num), "%d", i + 1);
        int w = canvas_string_width(canvas, num);
        canvas_draw_str(canvas, DIP_X0 + i * DIP_SPC + (4 - w / 2), 44, num);
    }

    /* ── Status / help line ──────────────────────────────────────────── */
    switch(app->mode) {
    case AppMode_Listen: {
        /* ── RSSI bar ─────────────────────────────────────────────────
         * Maps -100 .. -20 dBm → 0 .. 84 px.  Gives instant feedback
         * on whether the radio is actually in RX mode and seeing RF.  */
        const int bx = 0, by = 47, bw = 85, bh = 8;
        canvas_draw_frame(canvas, bx, by, bw, bh);
        float r = app->rssi_dbm;
        if(r < -100.0f) r = -100.0f;
        if(r > -20.0f)  r = -20.0f;
        int fill = (int)((r + 100.0f) * (bw - 2) / 80.0f);
        if(fill > 0) canvas_draw_box(canvas, bx + 1, by + 1, fill, bh - 2);
        char rs[12];
        snprintf(rs, sizeof(rs), "%d dBm", (int)app->rssi_dbm);
        canvas_draw_str(canvas, bw + 2, by + 7, rs);

        /* ── Status hint ─────────────────────────────────────── */
        char bot[32];
        if(app->has_decode)
            snprintf(bot, sizeof(bot), "Frames: %lu  OK=edit",
                     (unsigned long)app->frame_count);
        else
            snprintf(bot, sizeof(bot), "OK=edit  Back=exit");
        canvas_draw_str(canvas, 0, 63, bot);
        break;
    }
    case AppMode_Edit:
        canvas_draw_str(canvas, 0, 55, "U/D=cycle OK=TX Back=listen");
        break;
    case AppMode_Transmit:
        canvas_draw_str(canvas, 0, 55, "Transmitting... Back=abort");
        break;
    }
}

/* ─── Input callback ──────────────────────────────────────────────────────── */

static void input_cb(InputEvent* event, void* ctx) {
    AllstarApp* app = (AllstarApp*)ctx;
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

/* ─── Entry point ─────────────────────────────────────────────────────────── */

int32_t allstar_firefly_app(void* p) {
    UNUSED(p);

    AllstarApp* app = malloc(sizeof(AllstarApp));
    if(!app) return -1;
    memset(app, 0, sizeof(AllstarApp));

    for(int i = 0; i < AF_BIT_COUNT; i++) app->dip[i] = '-';
    app->dip[AF_BIT_COUNT] = '\0';
    app->running = true;
    app->mode    = AppMode_Listen;

    /* Enable DWT cycle counter for µs-precision GPIO edge timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /* SubGHz */
    subghz_devices_init();
    app->radio   = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    furi_check(app->radio);
    subghz_devices_begin(app->radio);
    app->gdo0    = subghz_devices_get_data_gpio(app->radio);
    app->cpu_mhz = furi_hal_cortex_instructions_per_microsecond();

    /* GUI */
    app->gui       = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port,  draw_cb,  app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->event_queue   = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    af_radio_start_rx(app);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    /* ── Main loop ──────────────────────────────────────────────────────── */
    InputEvent ev;
    while(app->running) {

        /* Consume a newly decoded frame */
        if(app->rx_ready && app->mode == AppMode_Listen) {
            char decoded[AF_BIT_COUNT + 1];
            if(af_decode_symbols(app->rx_syms, decoded)) {
                memcpy(app->dip, decoded, sizeof(decoded));
                app->has_decode = true;
                app->frame_count++;
                notification_message(app->notifications, &sequence_blink_green_10);
                view_port_update(app->view_port);
            }
            /* Unlock IRQ — go back to WaitGap so we sync cleanly on
             * the next inter-frame boundary rather than picking up
             * the stream mid-frame.                                         */
            app->rx_ready = false;
            app->rx_state = RxState_WaitGap;
            app->rx_count = 0;
        }

        /* Check for TX completion */
        if(app->mode == AppMode_Transmit && app->tx_done) {
            af_radio_stop_tx(app);
            notification_message(app->notifications, &sequence_blink_blue_10);
            app->mode = AppMode_Edit;
            view_port_update(app->view_port);
        }

        /* Input — 50 ms timeout gives ~20 fps draw rate.
         * On timeout: refresh RSSI and redraw so the bar and IRQ counter
         * stay live even without button presses.                          */
        if(furi_message_queue_get(app->event_queue, &ev, 50) != FuriStatusOk) {
            if(app->mode == AppMode_Listen) {
                app->rssi_dbm = furi_hal_subghz_get_rssi();
                view_port_update(app->view_port);
            }
            continue;
        }
        if(ev.type != InputTypePress && ev.type != InputTypeRepeat)
            continue;

        switch(app->mode) {

        case AppMode_Listen:
            if(ev.key == InputKeyBack) {
                notification_message(app->notifications, &sequence_blink_stop);
                app->running = false;
            } else if(ev.key == InputKeyOk) {
                notification_message(app->notifications, &sequence_blink_stop);
                af_radio_stop_rx(app);
                app->cursor = 0;
                app->mode   = AppMode_Edit;
                view_port_update(app->view_port);
            }
            break;

        case AppMode_Edit:
            switch(ev.key) {
            case InputKeyBack:
                app->mode = AppMode_Listen;
                af_radio_start_rx(app);
                notification_message(app->notifications, &sequence_blink_start_cyan);
                view_port_update(app->view_port);
                break;
            case InputKeyLeft:
                if(app->cursor > 0) { app->cursor--; view_port_update(app->view_port); }
                break;
            case InputKeyRight:
                if(app->cursor < AF_BIT_COUNT - 1) { app->cursor++; view_port_update(app->view_port); }
                break;
            case InputKeyUp: {
                /* Cycle forward: + -> - -> 0 -> + */
                char c = app->dip[app->cursor];
                app->dip[app->cursor] = (c == '+') ? '-' : (c == '-') ? '0' : '+';
                view_port_update(app->view_port);
                break;
            }
            case InputKeyDown: {
                /* Cycle backward: + -> 0 -> - -> + */
                char c = app->dip[app->cursor];
                app->dip[app->cursor] = (c == '+') ? '0' : (c == '0') ? '-' : '+';
                view_port_update(app->view_port);
                break;
            }
            case InputKeyOk:
                af_build_tx_buf(app);
                app->mode = AppMode_Transmit;
                af_radio_start_tx(app);
                view_port_update(app->view_port);
                break;
            default: break;
            }
            break;

        case AppMode_Transmit:
            if(ev.key == InputKeyBack) {
                af_radio_stop_tx(app);
                app->mode = AppMode_Edit;
                view_port_update(app->view_port);
            }
            break;
        }
    }

    /* ── Cleanup ──────────────────────────────────────────────────────────── */
    notification_message(app->notifications, &sequence_blink_stop);
    af_radio_stop_rx(app);
    subghz_devices_end(app->radio);
    subghz_devices_deinit();

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
    return 0;
}