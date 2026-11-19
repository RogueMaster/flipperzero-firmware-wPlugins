#include <stdio.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include <nrf24.h>
#include "nrf24channelscanner_icons.h"

#define CHANNEL_COUNT    128
#define SAMPLES_PER_PASS 3

// x maps 1:1 to the channel index, so the plot is exactly 128px wide
#define CHART_TOP        12
#define CHART_BOTTOM     53
#define CHART_HEIGHT     (CHART_BOTTOM - CHART_TOP)
#define AXIS_TICK_TOP    54
#define AXIS_TICK_BOTTOM 56
#define AXIS_LABEL_Y     63

// A sweep touches 128 channels, and a repaint per channel is ~64 full framebuffer
// transfers a second. That saturates the GUI thread, which is also what dispatches
// input - hence the dead buttons. Cap the repaint rate instead.
#define REDRAW_PERIOD_MS 80

#define DWELL_MIN 1
#define DWELL_MAX 50

// Halve both counters rather than let them wrap; the ratio survives
#define SAMPLE_RESCALE_AT 0xF000

typedef struct {
    uint8_t channel;
    const char* label;
} WifiMarker;

// 2.4GHz Wi-Fi centres: ch1 2412, ch6 2437, ch11 2462, ch13 2472 MHz
static const WifiMarker wifi_markers[] = {
    {12, "1"},
    {37, "6"},
    {62, "11"},
    {72, "13"},
};

// Hits over samples, rather than a free-running count, so the number means something:
// the share of dwell time this channel had a carrier above the RPD threshold
static uint16_t ch_hits[CHANNEL_COUNT];
static uint16_t ch_samples[CHANNEL_COUNT];

static bool nrf_missing;
static bool show_help;
static bool scanning;
static bool stop_requested;
static bool show_dwell;
static uint8_t scan_ch;
static uint8_t cursor_ch;
static int dwell_ms;

static FuriThread* worker;

static uint8_t channel_busy_pct(uint8_t channel) {
    const uint16_t samples = ch_samples[channel];
    if(samples == 0) return 0;
    const uint32_t pct = ((uint32_t)ch_hits[channel] * 100u) / samples;
    return (pct > 100u) ? 100u : (uint8_t)pct;
}

static uint8_t channel_bar_height(uint8_t channel) {
    const uint8_t pct = channel_busy_pct(channel);
    if(pct == 0) return 0;
    const uint8_t h = (uint8_t)(((uint32_t)pct * CHART_HEIGHT) / 100u);
    // The RPD only trips above about -64dBm, so hits are sparse - give anything
    // that was seen at all one visible pixel instead of rounding it away
    return h ? h : 1;
}

static void clear_samples(void) {
    memset(ch_hits, 0, sizeof(ch_hits));
    memset(ch_samples, 0, sizeof(ch_samples));
}

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, 1);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_icon(canvas, 1, 0, &I_Ok_btn_9x9);
    canvas_draw_str(canvas, 12, 8, scanning ? "Stop" : "Scan");
    canvas_draw_icon(canvas, 118, 0, &I_Pin_back_arrow_10x8);

    char readout[24];
    if(show_dwell) {
        snprintf(readout, sizeof(readout), "dwell %dms", dwell_ms);
    } else {
        snprintf(
            readout, sizeof(readout), "%uMHz %u%%", 2400u + cursor_ch, channel_busy_pct(cursor_ch));
    }
    canvas_draw_str_aligned(canvas, 76, 4, AlignCenter, AlignCenter, readout);

    // Where the sweep is right now, kept above the rule so it cannot be read as the cursor
    if(scanning) canvas_draw_line(canvas, scan_ch, 9, scan_ch, 10);

    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(nrf_missing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "NRF24 not found!");
        return;
    }

    if(show_help) {
        canvas_draw_str(canvas, 2, 21, "OK: start/stop scan");
        canvas_draw_str(canvas, 2, 31, "< >: channel  ^v: dwell");
        canvas_draw_str(canvas, 2, 41, "Back: clear, hold: exit");
        canvas_draw_str(canvas, 2, 51, "ticks: Wi-Fi 1/6/11/13");
    } else {
        for(uint8_t i = 0; i < CHANNEL_COUNT; i++) {
            const uint8_t h = channel_bar_height(i);
            if(h) canvas_draw_line(canvas, i, CHART_BOTTOM - h, i, CHART_BOTTOM);
        }

        // Dotted above the bar, cut back out of the bar itself, so the cursor stays
        // visible whether or not the channel it sits on has anything on it
        const uint8_t cursor_h = channel_bar_height(cursor_ch);
        const int bar_top = CHART_BOTTOM - cursor_h;
        for(int y = CHART_TOP; y < bar_top; y += 2) {
            canvas_draw_dot(canvas, cursor_ch, y);
        }
        if(cursor_h) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_line(canvas, cursor_ch, bar_top, cursor_ch, CHART_BOTTOM);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    for(size_t i = 0; i < COUNT_OF(wifi_markers); i++) {
        const uint8_t x = wifi_markers[i].channel;
        canvas_draw_line(canvas, x, AXIS_TICK_TOP, x, AXIS_TICK_BOTTOM);
        canvas_draw_str_aligned(
            canvas, x, AXIS_LABEL_Y, AlignCenter, AlignBottom, wifi_markers[i].label);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static int32_t scan_worker(void* context) {
    ViewPort* view_port = context;

    nrf24_write_reg(nrf24_HANDLE, REG_EN_AA, 0x00);
    nrf24_write_reg(nrf24_HANDLE, REG_RF_SETUP, 0x0f);
    nrf24_set_rx_mode(nrf24_HANDLE, true);
    furi_delay_ms(10); // Tpd2stby, 4.5ms worst case

    const uint32_t redraw_ticks = furi_ms_to_ticks(REDRAW_PERIOD_MS);
    uint32_t last_redraw = furi_get_tick();

    while(!stop_requested) {
        for(uint8_t i = 0; i < CHANNEL_COUNT; i++) {
            if(stop_requested) break;
            scan_ch = i;

            // RF_CH has to be written from standby. Writing it with CE still high is
            // not guaranteed to take, which left the sweep re-reading one channel.
            furi_hal_gpio_write(nrf24_CE_PIN, false);
            nrf24_write_reg(nrf24_HANDLE, REG_RF_CH, i);
            furi_hal_gpio_write(nrf24_CE_PIN, true);
            furi_delay_us(150); // Tstby2a = 130us

            for(uint8_t s = 0; s < SAMPLES_PER_PASS; s++) {
                furi_delay_ms(dwell_ms);
                if(nrf24_get_rdp(nrf24_HANDLE) & 0x01) ch_hits[i]++;
                ch_samples[i]++;
            }

            if(ch_samples[i] >= SAMPLE_RESCALE_AT) {
                ch_samples[i] >>= 1;
                ch_hits[i] >>= 1;
            }

            const uint32_t now = furi_get_tick();
            if(now - last_redraw >= redraw_ticks) {
                last_redraw = now;
                view_port_update(view_port);
            }
        }
    }

    nrf24_set_idle(nrf24_HANDLE);
    scanning = false;
    view_port_update(view_port);
    return 0;
}

static bool scan_start(ViewPort* view_port) {
    if(!nrf24_check_connected(nrf24_HANDLE)) {
        nrf_missing = true;
        return false;
    }

    nrf_missing = false;
    stop_requested = false;
    scanning = true; // set here, so this frame already shows "Stop"

    worker = furi_thread_alloc();
    furi_thread_set_name(worker, "NrfChScan");
    furi_thread_set_stack_size(worker, 2048);
    furi_thread_set_callback(worker, scan_worker);
    furi_thread_set_context(worker, view_port);
    furi_thread_start(worker);
    return true;
}

static void scan_stop(void) {
    stop_requested = true;
    if(worker) {
        furi_thread_join(worker);
        furi_thread_free(worker);
        worker = NULL;
    }
    scanning = false;
}

static void change_dwell(int delta) {
    dwell_ms += delta;
    if(dwell_ms < DWELL_MIN) dwell_ms = DWELL_MIN;
    if(dwell_ms > DWELL_MAX) dwell_ms = DWELL_MAX;
    show_dwell = true;
}

static void move_cursor(int delta) {
    int channel = (int)cursor_ch + delta;
    while(channel < 0) {
        channel += CHANNEL_COUNT;
    }
    while(channel >= CHANNEL_COUNT) {
        channel -= CHANNEL_COUNT;
    }
    cursor_ch = (uint8_t)channel;
    show_dwell = false;
}

// Main entry of the application
int32_t nrf24channelscanner_main(void* p) {
    UNUSED(p);

    clear_samples();
    nrf_missing = false;
    show_help = true;
    scanning = false;
    stop_requested = false;
    show_dwell = false;
    scan_ch = 0;
    cursor_ch = 0;
    dwell_ms = 5;
    worker = NULL;

    InputEvent event;
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    uint8_t attempts = 0;
    const bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }

    nrf24_init();

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    bool running = true;
    while(running) {
        furi_check(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk);

        show_help = false;

        switch(event.key) {
        case InputKeyOk:
            if(event.type == InputTypeShort) {
                if(scanning) {
                    notification_message(notification, &sequence_blink_yellow_100);
                    scan_stop();
                } else if(scan_start(view_port)) {
                    notification_message(notification, &sequence_blink_green_100);
                } else {
                    notification_message(notification, &sequence_error);
                }
            }
            break;

        case InputKeyBack:
            if(event.type == InputTypeLong) {
                running = false;
            } else if(event.type == InputTypeShort) {
                clear_samples();
                nrf_missing = false;
            }
            break;

        case InputKeyUp:
            if(event.type == InputTypeShort) {
                change_dwell(1);
            } else if(event.type == InputTypeRepeat) {
                change_dwell(5);
            }
            break;

        case InputKeyDown:
            if(event.type == InputTypeShort) {
                change_dwell(-1);
            } else if(event.type == InputTypeRepeat) {
                change_dwell(-5);
            }
            break;

        case InputKeyLeft:
            if(event.type == InputTypeShort) {
                move_cursor(-1);
            } else if(event.type == InputTypeRepeat) {
                move_cursor(-5);
            }
            break;

        case InputKeyRight:
            if(event.type == InputTypeShort) {
                move_cursor(1);
            } else if(event.type == InputTypeRepeat) {
                move_cursor(5);
            }
            break;

        default:
            break;
        }

        view_port_update(view_port);
    }

    scan_stop();
    nrf24_deinit();

    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }

    // View port first: it is what feeds the queue, so the queue cannot be freed
    // while the GUI thread may still be posting into it
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    return 0;
}
