#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"
#include <stdio.h>
#include <string.h>

// Runs a loaded .cantest profile. Default state is DRY-RUN (shows what it would
// send, transmits nothing). The user presses ARM to send; transmission is hard-
// gated by fsd_profile_tx_allowed() — Active/Service mode + a fresh DI_speed
// frame showing the car stationary — re-checked before every frame, so a moving
// car aborts the burst. A result log is written to SD for the bug report.

#define SEND_RESULTS_DIR "/ext/apps_data/tesla_mod/tests/results"
#define SendRunEventArm  0xA1u

static void send_run_button_cb(GuiButtonType result, InputType type, void* context) {
    TeslaFSDApp* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SendRunEventArm);
    }
}

static const char* send_run_block_reason(const FSDState* state, uint32_t now) {
    if(state->op_mode == OpMode_ListenOnly) return "set Mode=Active";
    if(!state->speed_seen) return "no 0x257 speed";
    if((now - state->last_speed_tick_ms) > FSD_PROFILE_SPEED_FRESH_MS) return "speed stale";
    if(state->vehicle_speed_kph > 0.5f) return "not stationary";
    return "blocked";
}

// Drain one RX frame; if it is DI_speed, refresh the interlock freshness.
static void send_run_pump_speed(MCP2515* mcp, FSDState* state) {
    CANFRAME rx;
    if(check_receive(mcp) == ERROR_OK && read_can_message(mcp, &rx) == ERROR_OK) {
        if(rx.canId == CAN_ID_DI_SPEED) {
            fsd_handle_di_speed(state, &rx);
            state->last_speed_tick_ms = furi_get_tick();
        }
    }
}

// Transmit every step (repeat x, delay between), keeping the speed interlock
// live and re-checking it before each frame. Returns frames actually sent.
static uint32_t send_run_burst(TeslaFSDApp* app, MCP2515* mcp, FSDState* state) {
    uint32_t sent = 0;
    for(uint8_t i = 0; i < app->send_step_count; i++) {
        FsdProfileStep* st = &app->send_steps[i];
        for(uint16_t r = 0; r < st->repeat; r++) {
            if(furi_thread_flags_get() & WorkerFlagStop) return sent;
            if(!fsd_profile_tx_allowed(state, furi_get_tick())) return sent; // aborts if unsafe

            CANFRAME f;
            memset(&f, 0, sizeof(f));
            f.canId = st->can_id;
            f.data_lenght = st->dlc;
            memcpy(f.buffer, st->data, st->dlc);
            send_can_frame(mcp, &f);
            sent++;

            uint32_t until = furi_get_tick() + furi_ms_to_ticks(st->delay_ms);
            while(furi_get_tick() < until) {
                if(furi_thread_flags_get() & WorkerFlagStop) return sent;
                send_run_pump_speed(mcp, state);
                furi_delay_ms(1);
            }
        }
    }
    return sent;
}

static void send_run_write_result(TeslaFSDApp* app, uint32_t sent) {
    storage_common_mkdir(app->storage, "/ext/apps_data/tesla_mod/tests");
    storage_common_mkdir(app->storage, SEND_RESULTS_DIR);
    char path[110];
    snprintf(path, sizeof(path), SEND_RESULTS_DIR "/send_%lu.log", (unsigned long)furi_get_tick());

    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[160];
        int n = snprintf(
            line,
            sizeof(line),
            "# Tesla CAN test result\n# profile: %s\n# app: %s\n# frames sent: %lu\n",
            app->send_name,
            TESLA_FSD_VERSION,
            (unsigned long)sent);
        storage_file_write(f, line, n);
        for(uint8_t i = 0; i < app->send_step_count; i++) {
            FsdProfileStep* st = &app->send_steps[i];
            n = snprintf(line, sizeof(line), "sent %03lX#", (unsigned long)st->can_id);
            for(uint8_t b = 0; b < st->dlc && n < (int)sizeof(line) - 4; b++)
                n += snprintf(line + n, sizeof(line) - n, "%02X", st->data[b]);
            n += snprintf(line + n, sizeof(line) - n, " x%u\n", st->repeat);
            storage_file_write(f, line, n);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

static void
    send_run_display(TeslaFSDApp* app, const FSDState* state, bool done, const char* blocked) {
    widget_reset(app->widget);
    char l[48];

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Send CAN Test");

    snprintf(l, sizeof(l), "%.20s", app->send_name);
    widget_add_string_element(app->widget, 64, 14, AlignCenter, AlignTop, FontSecondary, l);

    snprintf(
        l,
        sizeof(l),
        "%u frames   speed %.0f",
        app->send_step_count,
        (double)state->vehicle_speed_kph);
    widget_add_string_element(app->widget, 2, 26, AlignLeft, AlignTop, FontSecondary, l);

    const char* status;
    if(done) {
        snprintf(l, sizeof(l), "DONE - sent %lu (saved)", (unsigned long)app->send_sent);
        status = l;
    } else if(blocked) {
        snprintf(l, sizeof(l), "BLOCKED: %s", blocked);
        status = l;
    } else {
        status = "DRY RUN - not sending";
    }
    widget_add_string_element(app->widget, 2, 38, AlignLeft, AlignTop, FontSecondary, status);

    if(!done) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "ARM SEND", send_run_button_cb, app);
    }
    widget_add_string_element(
        app->widget, 64, 53, AlignCenter, AlignTop, FontSecondary, "parked only - BACK to stop");
}

static int32_t send_run_worker(void* context) {
    TeslaFSDApp* app = context;
    MCP2515* mcp = app->mcp_can;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FSDState state = app->fsd_state;
    state.op_mode = app->op_mode;
    OpMode mode = app->op_mode;
    app->send_armed = false;
    app->send_sent = 0;
    furi_mutex_release(app->mutex);

    // Listen-Only => hardware can't TX (defense in depth); Active/Service => normal.
    mcp->mode = (mode == OpMode_ListenOnly) ? MCP_LISTENONLY : MCP_NORMAL;
    mcp->bitRate = MCP_500KBPS;
    switch(app->mcp_clock) {
    case 1:
        mcp->clck = MCP_8MHZ;
        break;
    case 2:
        mcp->clck = MCP_12MHZ;
        break;
    default:
        mcp->clck = MCP_16MHZ;
        break;
    }
    if(mcp2515_init(mcp) != ERROR_OK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TeslaFSDEventNoDevice);
        return 0;
    }
    // Wide-open so we receive DI_speed (0x257) for the interlock.
    init_mask(mcp, 0, 0x000);
    init_mask(mcp, 1, 0x000);

    uint32_t last_display = 0;
    bool done = false;
    const char* blocked = NULL;

    while(true) {
        if(furi_thread_flags_get() & WorkerFlagStop) break;
        uint32_t now = furi_get_tick();

        send_run_pump_speed(mcp, &state);

        bool armed;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        armed = app->send_armed;
        furi_mutex_release(app->mutex);

        if(armed && !done) {
            if(fsd_profile_tx_allowed(&state, now)) {
                blocked = NULL;
                uint32_t sent = send_run_burst(app, mcp, &state);
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->send_sent = sent;
                app->send_armed = false;
                furi_mutex_release(app->mutex);
                send_run_write_result(app, sent);
                done = true;
            } else {
                blocked = send_run_block_reason(&state, now);
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->send_armed = false; // require a deliberate re-arm once safe
                furi_mutex_release(app->mutex);
            }
        }

        if((now - last_display) >= furi_ms_to_ticks(250)) {
            send_run_display(app, &state, done, blocked);
            last_display = now;
        }
        furi_delay_ms(2);
    }

    deinit_mcp2515(mcp);
    return 0;
}

void tesla_fsd_scene_send_run_on_enter(void* context) {
    TeslaFSDApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "Starting...");
    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewWidget);

    app->worker_thread = furi_thread_alloc_ex("SendRun", 4096, send_run_worker, app);
    furi_thread_start(app->worker_thread);
}

bool tesla_fsd_scene_send_run_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SendRunEventArm) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->send_armed = true;
            furi_mutex_release(app->mutex);
            consumed = true;
        } else if(event.event == TeslaFSDEventNoDevice) {
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget,
                64,
                28,
                AlignCenter,
                AlignCenter,
                FontPrimary,
                "CAN Module\nNot Found");
            consumed = true;
        }
    }
    return consumed;
}

void tesla_fsd_scene_send_run_on_exit(void* context) {
    TeslaFSDApp* app = context;
    if(app->worker_thread) {
        furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerFlagStop);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    widget_reset(app->widget);
}
