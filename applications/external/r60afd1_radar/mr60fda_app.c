/*
 * MR60FDA Radar — Flipper Zero viewer for the Seeed R60AFD1 60 GHz mmWave
 * fall-detection module.
 *
 * Reads UART frames at 115200 8N1 on USART1 (Pin 13 TX, Pin 14 RX),
 * decodes presence / motion / fall, and shows them on the screen.
 *
 * Wiring:
 *   Flipper Pin 1  (+5V)  ──> Radar VCC      (enable: Settings > Power > 5V on GPIO)
 *   Flipper Pin 8  (GND)  ──> Radar GND
 *   Flipper Pin 13 (TX)   ──> Radar RX
 *   Flipper Pin 14 (RX)   ──> Radar TX
 *
 * NOTE: API surface here targets the OFW serial HAL (~0.103+ / 1.0+). If you
 * are on an older or unofficial firmware, the furi_hal_serial_* names may
 * differ — adjust to match your toolchain.
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdio.h>

#include "mr60fda_parser.h"

#define MR60_BAUD       115200
#define MR60_SERIAL_ID  FuriHalSerialIdUsart
#define MR60_RX_BUFSIZE 256

typedef enum {
    EvInput,
    EvData,
} EvType;

typedef struct {
    EvType     type;
    InputEvent input;
} AppEvent;

typedef struct {
    FuriMessageQueue*    queue;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer*    rx;
    FuriMutex*           mtx;
    ViewPort*            vp;
    Gui*                 gui;

    Mr60Parser           parser;
    Mr60RadarState       state;

    bool                 quit;
    bool                 serial_error;
} App;

static void render(Canvas* c, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mtx, FuriWaitForever);
    Mr60RadarState s = app->state;
    furi_mutex_release(app->mtx);

    canvas_clear(c);
    char buf[22];

    if(app->serial_error) {
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, 0, 0, 128, 12);
        canvas_set_color(c, ColorWhite);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 3, 9, "UART Error");
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 4, 28, "UART port busy.");
        canvas_draw_str(c, 4, 38, "Exit UART Bridge");
        canvas_draw_str(c, 4, 48, "and try again.");
        elements_button_right(c, "Back");
        return;
    }

    const bool live = s.last_frame_ms &&
                      (furi_get_tick() - s.last_frame_ms) < 2000;

    /* ══ Header (inverted) ════════════════════════════════ */
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 3, 9, "MR60FDA Radar");
    /* live indicator: filled square=RX OK, outline=no signal */
    if(live) {
        canvas_draw_box(c, 121, 4, 4, 4);
    } else {
        canvas_draw_frame(c, 121, 4, 4, 4);
    }
    canvas_set_color(c, ColorBlack);

    /* ══ Vertical divider ═════════════════════════════════ */
    canvas_draw_line(c, 63, 12, 63, 51);

    /* ══ Left column: measurements ═══════════════════════ */
    canvas_set_font(c, FontSecondary);

    /* HR */
    canvas_draw_str(c, 1, 21, "HR");
    if(s.heart_rate)
        snprintf(buf, sizeof(buf), "%u bpm", s.heart_rate);
    else
        snprintf(buf, sizeof(buf), "-- bpm");
    canvas_draw_str_aligned(c, 61, 21, AlignRight, AlignBottom, buf);

    /* Resp */
    canvas_draw_str(c, 1, 30, "Resp");
    if(s.breath_rate)
        snprintf(buf, sizeof(buf), "%u /min", s.breath_rate);
    else
        snprintf(buf, sizeof(buf), "-- /min");
    canvas_draw_str_aligned(c, 61, 30, AlignRight, AlignBottom, buf);

    /* Dist */
    canvas_draw_str(c, 1, 39, "Dist");
    if(s.distance_cm)
        snprintf(buf, sizeof(buf), "%ucm", s.distance_cm);
    else
        snprintf(buf, sizeof(buf), "--cm");
    canvas_draw_str_aligned(c, 61, 39, AlignRight, AlignBottom, buf);

    /* BSign */
    canvas_draw_str(c, 1, 48, "BSign");
    snprintf(buf, sizeof(buf), "%u", s.body_sign);
    canvas_draw_str_aligned(c, 61, 48, AlignRight, AlignBottom, buf);

    /* ══ Right column: status ══════════════════════════════ */

    /* Presence — inverted box when person is detected */
    if(s.presence) {
        canvas_draw_box(c, 65, 12, 63, 11);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str_aligned(c, 96, 21, AlignCenter, AlignBottom, "PRESENT");
        canvas_set_color(c, ColorBlack);
    } else {
        canvas_draw_frame(c, 65, 12, 63, 11);
        canvas_draw_str_aligned(c, 96, 21, AlignCenter, AlignBottom, "absent");
    }

    /* Motion */
    canvas_draw_str(c, 66, 30, "Mot");
    const char* mot = "none";
    if(s.motion == 1) mot = "static";
    else if(s.motion == 2) mot = "active";
    canvas_draw_str_aligned(c, 126, 30, AlignRight, AlignBottom, mot);

    /* Fall — inverted alarm box when triggered */
    if(s.fall_detected) {
        canvas_draw_box(c, 65, 31, 63, 10);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str_aligned(c, 96, 39, AlignCenter, AlignBottom, "! FALL !");
        canvas_set_color(c, ColorBlack);
    } else {
        canvas_draw_str(c, 66, 39, "Fall");
        canvas_draw_str_aligned(c, 126, 39, AlignRight, AlignBottom, "no");
    }

    /* Residence time */
    canvas_draw_str(c, 66, 48, "Time");
    snprintf(buf, sizeof(buf), "%lus", (unsigned long)s.residence_sec);
    canvas_draw_str_aligned(c, 126, 48, AlignRight, AlignBottom, buf);

    /* ══ Footer ════════════════════════════════════════════════ */
    canvas_draw_line(c, 0, 51, 128, 51);

    // BSign signal-strength bars: 6 bars of increasing height, left-to-right
    // Each bar: 5px wide, heights 2,4,6,8,10,12px, 2px gap → total 6*5+5*2=40px
    // Baseline y=63. Bar N is filled if val > N*(100/6)
    canvas_draw_str(c, 1, 63, "Mv:");
    {
        uint8_t val = s.body_sign > 100 ? 100 : s.body_sign;
        // 6 bars
        static const uint8_t bar_h[6] = {2, 4, 6, 8, 10, 12};
        for(int i = 0; i < 6; i++) {
            int x = 14 + i * 7;          // 5px bar + 2px gap
            uint8_t h2 = bar_h[i];
            int y = 63 - h2 + 1;          // grow upward from baseline y=63
            if(val > (uint8_t)(i * 17)) { // filled
                canvas_draw_box(c, x, y, 5, h2);
            } else {                       // outline only
                canvas_draw_frame(c, x, y, 5, h2);
            }
        }
    }
    snprintf(buf, sizeof(buf), "#%lu", s.frames_total % 1000);
    canvas_draw_str(c, 57, 63, buf);
    elements_button_right(c, "Back");
}

static void on_input(InputEvent* ev, void* ctx) {
    App* app = ctx;
    AppEvent e = {.type = EvInput, .input = *ev};
    furi_message_queue_put(app->queue, &e, FuriWaitForever);
}

static void on_serial_rx(FuriHalSerialHandle* h, FuriHalSerialRxEvent ev, void* ctx) {
    App* app = ctx;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(h);
        furi_stream_buffer_send(app->rx, &b, 1, 0);
        AppEvent e = {.type = EvData};
        furi_message_queue_put(app->queue, &e, 0);
    }
}

int32_t mr60fda_app(void* arg) {
    UNUSED(arg);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    mr60_parser_reset(&app->parser);

    app->mtx   = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(32, sizeof(AppEvent));
    app->rx    = furi_stream_buffer_alloc(MR60_RX_BUFSIZE, 1);

    // GUI must be up before serial so error screen can be shown.
    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, render, app);
    view_port_input_callback_set(app->vp, on_input, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    // Power the radar from Pin 1 (5V OTG).
    furi_hal_power_enable_otg();

    // Acquire and start USART1 at 115200 8N1.
    app->serial = furi_hal_serial_control_acquire(MR60_SERIAL_ID);
    if(!app->serial) {
        app->serial_error = true;
        view_port_update(app->vp);
        AppEvent e;
        while(furi_message_queue_get(app->queue, &e, FuriWaitForever) == FuriStatusOk) {
            if(e.type == EvInput &&
               e.input.type == InputTypeShort &&
               e.input.key == InputKeyBack) break;
        }
        furi_hal_power_disable_otg();
        gui_remove_view_port(app->gui, app->vp);
        furi_record_close(RECORD_GUI);
        view_port_free(app->vp);
        furi_stream_buffer_free(app->rx);
        furi_message_queue_free(app->queue);
        furi_mutex_free(app->mtx);
        free(app);
        return 1;
    }
    furi_hal_serial_init(app->serial, MR60_BAUD);
    furi_hal_serial_async_rx_start(app->serial, on_serial_rx, app, false);

    // Query current presence state (radar only reports on change).
    // 53 59 80 81 00 01 0F BD 54 43
    static const uint8_t presence_query[] = {0x53,0x59,0x80,0x81,0x00,0x01,0x0F,0xBD,0x54,0x43};
    furi_hal_serial_tx(app->serial, presence_query, sizeof(presence_query));

    AppEvent e;
    while(!app->quit) {
        if(furi_message_queue_get(app->queue, &e, 250) != FuriStatusOk) {
            view_port_update(app->vp);
            continue;
        }

        if(e.type == EvInput) {
            if(e.input.type == InputTypeShort && e.input.key == InputKeyBack) {
                app->quit = true;
            }
        } else if(e.type == EvData) {
            uint8_t b;
            while(furi_stream_buffer_receive(app->rx, &b, 1, 0) == 1) {
                if(mr60_parser_feed(&app->parser, b)) {
                    furi_mutex_acquire(app->mtx, FuriWaitForever);
                    mr60_apply(&app->parser, &app->state);
                    app->state.last_frame_ms = furi_get_tick();
                    furi_mutex_release(app->mtx);
                }
            }
            view_port_update(app->vp);
        }
    }

    // Teardown.
    furi_hal_serial_async_rx_stop(app->serial);
    furi_hal_serial_deinit(app->serial);
    furi_hal_serial_control_release(app->serial);
    furi_hal_power_disable_otg();

    gui_remove_view_port(app->gui, app->vp);
    furi_record_close(RECORD_GUI);
    view_port_free(app->vp);

    furi_stream_buffer_free(app->rx);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mtx);
    free(app);
    return 0;
}
