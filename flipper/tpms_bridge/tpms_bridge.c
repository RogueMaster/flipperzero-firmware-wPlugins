#include "tpms_bridge.h"
#include "tpms_lf.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#define TAG "TpmsBridge"

#define TPMS_INPUT_QUEUE_SIZE 8

void tpms_bridge_report_frame(TpmsBridgeApp* app, const TpmsRenaultFrame* frame) {
    furi_check(app);
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->frames++;
    app->last_id = frame->id;
    app->last_pressure_raw = frame->pressure_raw;
    app->last_temperature_c = frame->temperature_c;
    app->last_frame_tick = furi_get_tick();
    app->has_frame = true;
    furi_mutex_release(app->state_mutex);

    if(app->view_port) view_port_update(app->view_port);
}

static void tpms_bridge_draw_callback(Canvas* canvas, void* context) {
    TpmsBridgeApp* app = context;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "TPMS Bridge");

    canvas_set_font(canvas, FontSecondary);

    char line[48];
    if(app->exit_blocked) {
        canvas_draw_str(canvas, 2, 22, "Ждём завершения USB-сессии");
    } else if(app->usb_streaming) {
        canvas_draw_str(canvas, 2, 22, "USB: streaming");
    } else if(app->local_rx) {
        canvas_draw_str(canvas, 2, 22, "Local RX: on");
    } else {
        canvas_draw_str(canvas, 2, 22, "Idle - connect via USB");
    }

    snprintf(line, sizeof(line), "Frames: %lu", (unsigned long)app->frames);
    canvas_draw_str(canvas, 2, 32, line);

    if(app->has_frame) {
        snprintf(line, sizeof(line), "ID %06lx", (unsigned long)app->last_id);
        canvas_draw_str(canvas, 2, 42, line);

        /* Давление в кПа = raw * 0.75; считаем целочисленно. */
        const uint32_t kpa_x100 = (uint32_t)app->last_pressure_raw * 75UL;
        snprintf(
            line,
            sizeof(line),
            "%lu.%02lu kPa  %d C",
            (unsigned long)(kpa_x100 / 100),
            (unsigned long)(kpa_x100 % 100),
            (int)app->last_temperature_c);
        canvas_draw_str(canvas, 2, 52, line);
    } else {
        canvas_draw_str(canvas, 2, 42, "No frames yet");
    }

    canvas_draw_str(canvas, 2, 62, "OK:RX  Right:wake  Back:exit");

    furi_mutex_release(app->state_mutex);
}

static void tpms_bridge_input_callback(InputEvent* event, void* context) {
    TpmsBridgeApp* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/** Позволяет закрыть приложение снаружи: `loader close`, `ufbt launch`.
 * Без этого система умеет только просить нажать Back вручную. */
static bool tpms_bridge_signal_callback(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);
    TpmsBridgeApp* app = context;

    if(signal != FuriSignalExit) return false;

    const InputEvent event = {.type = InputTypeShort, .key = InputKeyBack};
    furi_message_queue_put(app->input_queue, &event, 0);
    return true;
}

static void tpms_bridge_local_frame_callback(const TpmsRenaultFrame* frame, void* context) {
    tpms_bridge_report_frame(context, frame);
}

static int32_t tpms_bridge_local_rx_thread(void* context) {
    TpmsBridgeApp* app = context;

    if(furi_mutex_acquire(app->radio_mutex, 0) != FuriStatusOk) {
        FURI_LOG_W(TAG, "radio busy, local rx cancelled");
        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        app->local_rx = false;
        furi_mutex_release(app->state_mutex);
        view_port_update(app->view_port);
        return 0;
    }

    TpmsSession* session = tpms_session_alloc();
    tpms_session_set_frame_callback(session, tpms_bridge_local_frame_callback, app);

    if(tpms_session_start(session, TPMS_DEFAULT_FREQUENCY)) {
        while(app->local_rx && !app->stop_requested) {
            if(app->wake_requested) {
                app->wake_requested = false;
                tpms_session_wake_pulse(session, TPMS_LF_PULSE_MS);
            }
            tpms_session_pump(session, 100);
        }
        tpms_session_stop(session);
    } else {
        FURI_LOG_E(TAG, "cannot start local rx");
    }

    tpms_session_free(session);
    furi_mutex_release(app->radio_mutex);

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->local_rx = false;
    furi_mutex_release(app->state_mutex);
    view_port_update(app->view_port);
    return 0;
}

static void tpms_bridge_stop_local_rx(TpmsBridgeApp* app) {
    if(!app->local_thread) return;

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->local_rx = false;
    furi_mutex_release(app->state_mutex);

    furi_thread_join(app->local_thread);
    furi_thread_free(app->local_thread);
    app->local_thread = NULL;
}

static void tpms_bridge_toggle_local_rx(TpmsBridgeApp* app) {
    if(app->local_thread) {
        tpms_bridge_stop_local_rx(app);
        return;
    }

    if(app->usb_streaming) {
        FURI_LOG_W(TAG, "usb session owns the radio");
        return;
    }

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->local_rx = true;
    furi_mutex_release(app->state_mutex);

    app->local_thread =
        furi_thread_alloc_ex("TpmsLocalRx", 2048, tpms_bridge_local_rx_thread, app);
    furi_thread_start(app->local_thread);
}

static void tpms_bridge_wake_sensor(TpmsBridgeApp* app) {
    /* Если приём идёт, импульс должен пройти без остановки приёмника:
     * датчик отвечает сразу. Разбор крутит владелец сессии, поэтому здесь
     * только поднимаем флаг. */
    if(app->local_thread || app->usb_streaming) {
        app->wake_requested = true;
        return;
    }

    tpms_lf_wake(TPMS_LF_PULSE_MS);
}

static TpmsBridgeApp* tpms_bridge_app_alloc(void) {
    TpmsBridgeApp* app = malloc(sizeof(TpmsBridgeApp));
    memset(app, 0, sizeof(TpmsBridgeApp));

    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->radio_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(TPMS_INPUT_QUEUE_SIZE, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, tpms_bridge_draw_callback, app);
    view_port_input_callback_set(app->view_port, tpms_bridge_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    furi_thread_set_signal_callback(
        furi_thread_get_current(), tpms_bridge_signal_callback, app);

    app->cli_registry = furi_record_open(RECORD_CLI);
    /* Стек задаём явно: команда печатает форматированные строки и держит
     * FuriString, а размер стека по умолчанию у CLI-команд невелик. */
    cli_registry_add_command_ex(
        app->cli_registry,
        TPMS_CLI_COMMAND_NAME,
        CliCommandFlagParallelSafe,
        tpms_cli_command,
        app,
        TPMS_CLI_STACK_SIZE);

    return app;
}

static void tpms_bridge_app_free(TpmsBridgeApp* app) {
    cli_registry_delete_command(app->cli_registry, TPMS_CLI_COMMAND_NAME);
    furi_record_close(RECORD_CLI);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->radio_mutex);
    furi_mutex_free(app->state_mutex);
    free(app);
}

int32_t tpms_bridge_app(void* p) {
    UNUSED(p);
    TpmsBridgeApp* app = tpms_bridge_app_alloc();

    bool running = true;
    InputEvent event;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, 200) == FuriStatusOk) {
            if(event.type != InputTypeShort) continue;

            if(event.key == InputKeyBack) {
                running = false;
            } else if(event.key == InputKeyOk) {
                tpms_bridge_toggle_local_rx(app);
            } else if(event.key == InputKeyRight) {
                tpms_bridge_wake_sensor(app);
            }
        }
        view_port_update(app->view_port);
    }

    tpms_bridge_stop_local_rx(app);

    /* Пока CLI-команда крутится, её код лежит в этом .fap — выгружать
     * приложение нельзя. Просим сессию завершиться и ждём.
     *
     * Ждём ограниченное время: если сессия почему-то не отвечает, лучше
     * зависнуть на экране с понятной надписью, чем выгрузить код из-под
     * работающего потока. */
    app->stop_requested = true;
    uint32_t waited_ms = 0;
    while(app->cli_sessions > 0) {
        furi_delay_ms(20);
        waited_ms += 20;
        if(waited_ms > TPMS_CLI_STOP_TIMEOUT_MS) {
            FURI_LOG_E(TAG, "cli session did not stop, keeping app loaded");
            furi_mutex_acquire(app->state_mutex, FuriWaitForever);
            app->exit_blocked = true;
            furi_mutex_release(app->state_mutex);
            view_port_update(app->view_port);
            waited_ms = 0;
        }
    }

    tpms_bridge_app_free(app);
    return 0;
}
