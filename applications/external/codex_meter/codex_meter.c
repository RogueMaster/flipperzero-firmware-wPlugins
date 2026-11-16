/* Codex Meter. BLE lifecycle adapted from cobanov/kota (MIT).
 * See LICENSE-Kota. Protocol specification: ../PROTOCOL.md. */
#include <furi.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <cli/cli.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#define TAG         "Meter"
#define RX_BUF_SIZE 128
#define HELLO       "CODEXMETER 1\n"
#define REFRESH     "R\n"

/* Keep the internal backlight layer at 24%, beneath the normal timed layer.
 * Hardware key input still wakes the normal brightness and restarts the user's
 * configured timeout. When that timer expires, the light returns to this floor.
 * This override lasts only until the app releases its backlight lock on exit. */
static const NotificationMessage message_idle_brightness = {
    .type = NotificationMessageTypeForceDisplayBrightnessSetting,
    .data.forced_settings.display_brightness = 0.24f,
};
static const NotificationSequence sequence_idle_floor = {
    &message_idle_brightness,
    &message_display_backlight_enforce_on,
    NULL,
};

typedef struct {
    bool has_data;
    int16_t session_pct;
    int16_t week_pct;
    uint32_t session_reset_s;
    uint32_t week_reset_s;
    uint32_t received_tick;
    uint32_t session_mins;
    uint32_t week_mins;
} Usage;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    bool ble_ok;
    uint32_t started_tick;
    CliRegistry* cli;
    volatile bool usb_mode;
    volatile bool connected;
    volatile bool got_data; /* host sent a line since this connect */
    volatile bool failed;
    volatile bool remaining;
    volatile bool refreshing; /* OK pressed, waiting for the answer */
    uint32_t refresh_tick;
    Usage usage;
    char rx[RX_BUF_SIZE];
    size_t rx_len;
    bool rx_overflow;
} Meter;

/* ── protocol ─────────────────────────────────────────────── */

static uint32_t clamp_u32(long v, long max) {
    if(v < 0) return 0;
    if(v > max) return (uint32_t)max;
    return (uint32_t)v;
}

static void meter_parse_line(Meter* app, const char* line) {
    if(strcmp(line, "E1") == 0) {
        app->failed = true;
        app->refreshing = false;
        return;
    }
    if(strncmp(line, "Q1 ", 3) != 0) return;
    const char* p = line + 3;
    long v[6];
    for(int i = 0; i < 6; i++) {
        char* end;
        v[i] = strtol(p, &end, 10);
        if(end == p) return;
        p = end;
    }
    while(*p == ' ' || *p == '\r')
        p++;
    if(*p) return;
    if(v[0] < -1 || v[0] > 100 || v[3] < -1 || v[3] > 100) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->usage.session_pct = v[0];
    app->usage.session_reset_s = clamp_u32(v[1], 2592000);
    app->usage.session_mins = clamp_u32(v[2], 43200);
    app->usage.week_pct = v[3];
    app->usage.week_reset_s = clamp_u32(v[4], 2592000);
    app->usage.week_mins = clamp_u32(v[5], 43200);
    app->usage.received_tick = furi_get_tick();
    app->usage.has_data = true;
    furi_mutex_release(app->mutex);
    app->got_data = true;
    app->failed = false;
    app->refreshing = false;
}

static void meter_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    Meter* app = context;
    const char* line = furi_string_get_cstr(args);
    if(strcmp(line, "poll") == 0) {
        printf("\r\nMETER %s\r\n", app->refreshing ? "R" : "READY");
    } else if(strncmp(line, "Q1 ", 3) == 0 || strcmp(line, "E1") == 0) {
        app->usb_mode = true;
        meter_parse_line(app, line);
        printf("\r\nMETER OK\r\n");
    } else {
        printf("\r\nMETER BAD\r\n");
    }
}

/* Runs on the BLE thread. Never call ble_profile_serial_tx() from here: the
 * stack holds the same mutex and it deadlocks. Replies go through the main loop. */
static uint16_t meter_ble_rx(SerialServiceEvent event, void* context) {
    Meter* app = context;
    if(event.event != SerialServiceEventTypeDataReceived) return 0;

    for(uint16_t i = 0; i < event.data.size; i++) {
        char c = (char)event.data.buffer[i];
        if(c == '\n') {
            app->rx[app->rx_len] = '\0';
            if(app->rx_len && !app->rx_overflow) {
                app->usb_mode = false;
                meter_parse_line(app, app->rx);
            }
            app->rx_overflow = false;
            app->rx_len = 0;
        } else if(app->rx_len < RX_BUF_SIZE - 1) {
            app->rx[app->rx_len++] = c;
        } else {
            app->rx_overflow = true;
        }
    }
    return RX_BUF_SIZE;
}

/* The BT service opens an RPC session on every connect and takes over the
 * serial callback, so take it back here. */
static void meter_bt_status(BtStatus status, void* context) {
    Meter* app = context;
    bool connected = (status == BtStatusConnected);
    if(connected && app->profile) {
        ble_profile_serial_set_event_callback(app->profile, RX_BUF_SIZE, meter_ble_rx, app);
        ble_profile_serial_set_rpc_active(app->profile, false);
        app->rx_len = 0;
        app->rx_overflow = false;
    }
    app->got_data = false;
    app->refreshing = false;
    app->connected = connected;
    view_port_update(app->view_port);
}

static void meter_ble_start(Meter* app) {
    app->bt = furi_record_open(RECORD_BT);
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    app->profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(!app->profile) {
        FURI_LOG_E(TAG, "serial profile failed to start");
        return;
    }
    ble_profile_serial_set_event_callback(app->profile, RX_BUF_SIZE, meter_ble_rx, app);
    ble_profile_serial_set_rpc_active(app->profile, false);
    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(app->bt, meter_bt_status, app);
    app->ble_ok = true;
}

static void meter_ble_stop(Meter* app) {
    if(app->profile) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_profile_restore_default(app->bt);
        app->profile = NULL;
    }
    furi_record_close(RECORD_BT);
}

/* ── drawing ──────────────────────────────────────────────── */

static void format_duration(char* out, size_t size, uint32_t s) {
    uint32_t m = s / 60;
    if(m >= 24 * 60) {
        snprintf(
            out, size, "%lud %luh", (unsigned long)(m / 1440), (unsigned long)((m % 1440) / 60));
    } else if(m >= 60) {
        snprintf(out, size, "%luh %02lum", (unsigned long)(m / 60), (unsigned long)(m % 60));
    } else if(m > 0) {
        snprintf(out, size, "%lum", (unsigned long)m);
    } else {
        snprintf(out, size, "now");
    }
}

static void window_label(char* out, size_t size, uint32_t minutes, const char* fallback) {
    if(minutes == 0)
        snprintf(out, size, "%s", fallback);
    else if(minutes % 1440 == 0)
        snprintf(out, size, "%lud", (unsigned long)(minutes / 1440));
    else if(minutes % 60 == 0)
        snprintf(out, size, "%luh", (unsigned long)(minutes / 60));
    else
        snprintf(out, size, "%lum", (unsigned long)minutes);
}

static void draw_meter(
    Canvas* c,
    int y,
    const char* label,
    int pct,
    uint32_t reset,
    bool remaining,
    bool expired) {
    char text[24];
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 3, y, label);
    if(pct < 0) {
        canvas_draw_str_aligned(c, 124, y, AlignRight, AlignBottom, "not available");
        canvas_draw_rframe(c, 3, y + 3, 121, 7, 2);
        return;
    }
    format_duration(text, sizeof(text), reset);
    if(expired) snprintf(text, sizeof(text), "sync needed");
    canvas_draw_str_aligned(c, 89, y, AlignRight, AlignBottom, text);
    int value = remaining ? 100 - pct : pct;
    snprintf(text, sizeof(text), "%d%%", value);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 125, y + 1, AlignRight, AlignBottom, text);
    canvas_draw_rframe(c, 3, y + 3, 121, 7, 2);
    int fill = (117 * value) / 100;
    if(fill) canvas_draw_box(c, 5, y + 5, fill, 3);
    canvas_set_color(c, ColorWhite);
    for(int i = 1; i < 4; i++)
        canvas_draw_line(c, 3 + 121 * i / 4, y + 5, 3 + 121 * i / 4, y + 7);
    canvas_set_color(c, ColorBlack);
}

static void meter_draw(Canvas* c, void* context) {
    Meter* app = context;
    canvas_clear(c);
    if(furi_get_tick() - app->started_tick < 2500 * furi_kernel_get_tick_frequency() / 1000) {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 20, AlignCenter, AlignBottom, "DR. MOSFET");
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 39, AlignCenter, AlignBottom, "CODEX MONITOR");
        canvas_draw_line(c, 40, 46, 88, 46);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 59, AlignCenter, AlignBottom, "Bluetooth + USB");
        return;
    }
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    Usage u = app->usage;
    furi_mutex_release(app->mutex);
    uint32_t age =
        u.has_data ? (furi_get_tick() - u.received_tick) / furi_kernel_get_tick_frequency() : 0;
    canvas_draw_rbox(c, 0, 0, 128, 13, 2);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 4, 10, "CODEX");
    canvas_set_font(c, FontSecondary);
    const char* status = app->failed     ? "API ERROR" :
                         app->refreshing ? "SYNC..." :
                         app->usb_mode   ? (age > 150 ? "STALE" : "USB") :
                         !app->ble_ok    ? "BT ERROR" :
                         !app->connected ? "OFFLINE" :
                         !u.has_data     ? "LINKED" :
                         age > 150       ? "STALE" :
                                           "LIVE";
    canvas_draw_str_aligned(c, 123, 10, AlignRight, AlignBottom, status);
    canvas_set_color(c, ColorBlack);
    if(!u.has_data) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(
            c,
            64,
            30,
            AlignCenter,
            AlignCenter,
            app->failed ? "Check Codex login" : "Waiting for PC");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 43, AlignCenter, AlignCenter, "Run START-WINDOWS.cmd");
    } else {
        char label[12];
        uint32_t sr = u.session_reset_s > age ? u.session_reset_s - age : 0;
        uint32_t wr = u.week_reset_s > age ? u.week_reset_s - age : 0;
        window_label(label, sizeof(label), u.session_mins, "Limit 1");
        draw_meter(c, 25, label, u.session_pct, sr, app->remaining, sr == 0);
        window_label(label, sizeof(label), u.week_mins, "Limit 2");
        draw_meter(c, 46, label, u.week_pct, wr, app->remaining, wr == 0);
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 3, 63, "OK sync");
    canvas_draw_str_aligned(
        c, 125, 63, AlignRight, AlignBottom, app->remaining ? "<> LEFT" : "<> USED");
}

static void meter_input(InputEvent* event, void* context) {
    Meter* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

/* ── entry ────────────────────────────────────────────────── */

int32_t codex_meter_app(void* p) {
    UNUSED(p);
    Meter* app = malloc(sizeof(Meter));
    memset(app, 0, sizeof(Meter));
    app->started_tick = furi_get_tick();
    app->remaining = true;
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(app->notifications, &sequence_idle_floor);
    notification_message(app->notifications, &sequence_display_backlight_on);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, meter_draw, app);
    view_port_input_callback_set(app->view_port, meter_input, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->cli = furi_record_open(RECORD_CLI);
    cli_registry_add_command(app->cli, "codex_meter", CliCommandFlagParallelSafe, meter_cli, app);

    meter_ble_start(app);

    InputEvent event;
    while(true) {
        if(furi_message_queue_get(app->input_queue, &event, 1000) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort) break;
            if((event.key == InputKeyLeft || event.key == InputKeyRight) &&
               event.type == InputTypeShort)
                app->remaining = !app->remaining;
            if(event.key == InputKeyOk && event.type == InputTypeShort &&
               (app->usb_mode || (app->connected && app->profile))) {
                app->refreshing = true;
                app->refresh_tick = furi_get_tick();
                if(!app->usb_mode)
                    ble_profile_serial_tx(app->profile, (uint8_t*)REFRESH, strlen(REFRESH));
            }
        }
        /* Repeat the hello until the host answers: it may subscribe a moment after connecting. */
        if(app->connected && !app->got_data && app->profile) {
            ble_profile_serial_tx(app->profile, (uint8_t*)HELLO, strlen(HELLO));
        }
        /* Give up on a refresh the host never answered (older host, fetch error). */
        if(app->refreshing &&
           furi_get_tick() - app->refresh_tick > 10 * furi_kernel_get_tick_frequency()) {
            app->refreshing = false;
        }
        view_port_update(app->view_port); /* tick the countdowns */
    }

    cli_registry_delete_command(app->cli, "codex_meter");
    furi_record_close(RECORD_CLI);
    meter_ble_stop(app);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    notification_message_block(app->notifications, &sequence_display_backlight_enforce_auto);
    notification_message_block(app->notifications, &sequence_display_backlight_on);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
