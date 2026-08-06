#include "tpms_bridge.h"
#include "tpms_lf.h"

#include <furi.h>
#include <toolbox/args.h>
#include <toolbox/cli/cli_command.h>
#include <toolbox/pipe.h>

#include <stdio.h>

#define TAG "TpmsCli"

/* Сколько интервалов копим в raw-режиме перед выводом. Держим порцию
 * небольшой: буфер pipe невелик, длинные строки только мешают. */
#define TPMS_RAW_FLUSH_EVERY 24

/* Сколько ждать места в буфере pipe на одну строку, мс. */
#define TPMS_WRITE_TIMEOUT_MS 500

/* Ограничение на raw-режим: без него FSK-шум забьёт канал за секунды. */
#define TPMS_RAW_MAX_INTERVALS 200000UL

/* Если хост перестал читать на столько миллисекунд — считаем, что он ушёл,
 * и освобождаем радиомодуль. Иначе сессия висит вечно и держит его. */
#define TPMS_HOST_STALL_MS 5000

#define TPMS_LINE_MAX 256

typedef struct {
    PipeSide* pipe;
    TpmsBridgeApp* app;
    TpmsSession* session;

    uint32_t frames;
    uint32_t raw_count;
    uint32_t raw_pending;
    uint32_t dropped;
    uint32_t stall_since; /**< 0 — записи проходят */
    FuriString* raw_buffer;
} TpmsCliSession;

/** Запись в CLI с ограниченным ожиданием.
 *
 * Писать через printf нельзя: если хост отвалился, не закрыв команду,
 * буфер pipe переполняется и поток команды блокируется в записи навсегда,
 * удерживая радиомодуль.
 *
 * Пишем порциями по свободному месту — буфер pipe заметно меньше строки
 * raw-режима, и требовать место под всю строку сразу означало бы
 * выбрасывать вообще всё. Если места нет дольше TPMS_WRITE_TIMEOUT_MS,
 * строка теряется, но управление возвращается, и цикл успевает заметить
 * Ctrl+C или уход хоста.
 */
static bool tpms_cli_emit(TpmsCliSession* cli, const char* text) {
    const char* cursor = text;
    size_t remaining = strlen(text);
    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(TPMS_WRITE_TIMEOUT_MS);

    while(remaining > 0) {
        if(pipe_state(cli->pipe) == PipeStateBroken) return false;

        const size_t space = pipe_spaces_available(cli->pipe);
        if(space == 0) {
            if(furi_get_tick() > deadline) {
                cli->dropped++;
                if(cli->stall_since == 0) cli->stall_since = furi_get_tick();
                return false;
            }
            furi_delay_ms(2);
            continue;
        }

        const size_t chunk = space < remaining ? space : remaining;
        const size_t sent = pipe_send(cli->pipe, cursor, chunk);
        cursor += sent;
        remaining -= sent;
    }

    cli->stall_since = 0;
    return true;
}

static void tpms_cli_emit_direct(PipeSide* pipe, const char* text) {
    if(pipe_state(pipe) == PipeStateBroken) return;
    pipe_send(pipe, text, strlen(text));
}

static void tpms_cli_print_frame(const TpmsRenaultFrame* frame, void* context) {
    TpmsCliSession* cli = context;
    cli->frames++;

    char raw_hex[TPMS_RENAULT_FRAME_BYTES * 2 + 1];
    for(size_t i = 0; i < TPMS_RENAULT_FRAME_BYTES; i++) {
        static const char digits[] = "0123456789abcdef";
        raw_hex[i * 2] = digits[frame->raw[i] >> 4];
        raw_hex[i * 2 + 1] = digits[frame->raw[i] & 0x0F];
    }
    raw_hex[sizeof(raw_hex) - 1] = '\0';

    /* Только целые числа: printf прошивки не обязан уметь %f.
     * Давление отдаём в сотых долях кПа (raw * 0.75 * 100 == raw * 75),
     * RSSI — в десятых долях дБм. */
    const int32_t rssi_x10 = (int32_t)(tpms_session_get_rssi(cli->session) * 10.0f);

    char line[TPMS_LINE_MAX];
    snprintf(
        line,
        sizeof(line),
        "{\"t\":%lu,\"proto\":\"renault\",\"id\":\"%06lx\",\"raw\":\"%s\","
        "\"pressure_kpa_x100\":%lu,\"temp_c\":%d,\"flags\":%u,\"unknown\":%u,"
        "\"rssi_dbm_x10\":%ld}\r\n",
        (unsigned long)furi_get_tick(),
        (unsigned long)frame->id,
        raw_hex,
        (unsigned long)frame->pressure_raw * 75UL,
        (int)frame->temperature_c,
        (unsigned)frame->flags,
        (unsigned)frame->unknown,
        (long)rssi_x10);

    tpms_cli_emit(cli, line);
    tpms_bridge_report_frame(cli->app, frame);
}

static void tpms_cli_collect_raw(bool level, uint32_t duration, void* context) {
    TpmsCliSession* cli = context;
    if(cli->raw_count >= TPMS_RAW_MAX_INTERVALS) return;

    cli->raw_count++;
    cli->raw_pending++;
    furi_string_cat_printf(cli->raw_buffer, "%c%lu ", level ? '+' : '-', (unsigned long)duration);

    if(cli->raw_pending >= TPMS_RAW_FLUSH_EVERY) {
        furi_string_cat_str(cli->raw_buffer, "\r\n");
        tpms_cli_emit(cli, furi_string_get_cstr(cli->raw_buffer));
        furi_string_reset(cli->raw_buffer);
        cli->raw_pending = 0;
    }
}

void tpms_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    TpmsBridgeApp* app = context;

    uint32_t frequency = TPMS_DEFAULT_FREQUENCY;
    bool raw_mode = false;
    bool wake_enabled = false;

    FuriString* word = furi_string_alloc();
    if(args_read_string_and_trim(args, word)) {
        const char* text = furi_string_get_cstr(word);
        char* end = NULL;
        const unsigned long parsed = strtoul(text, &end, 10);
        if(end == text || parsed == 0) {
            tpms_cli_emit_direct(
                pipe, "Usage: " TPMS_CLI_COMMAND_NAME " [frequency_hz] [json|raw] [wake]\r\n");
            furi_string_free(word);
            return;
        }
        frequency = (uint32_t)parsed;

        while(args_read_string_and_trim(args, word)) {
            if(furi_string_equal_str(word, "raw")) {
                raw_mode = true;
            } else if(furi_string_equal_str(word, "wake")) {
                wake_enabled = true;
            } else if(!furi_string_equal_str(word, "json")) {
                tpms_cli_emit_direct(pipe, "Unknown option, expected json, raw or wake\r\n");
                furi_string_free(word);
                return;
            }
        }
    }
    furi_string_free(word);

    if(furi_mutex_acquire(app->radio_mutex, 0) != FuriStatusOk) {
        tpms_cli_emit_direct(pipe, "{\"error\":\"radio busy\"}\r\n");
        return;
    }

    TpmsCliSession cli = {
        .pipe = pipe,
        .app = app,
        .session = tpms_session_alloc(),
        .raw_buffer = furi_string_alloc(),
    };

    tpms_session_set_frame_callback(cli.session, tpms_cli_print_frame, &cli);
    if(raw_mode) tpms_session_set_raw_callback(cli.session, tpms_cli_collect_raw, &cli);

    if(!tpms_session_start(cli.session, frequency)) {
        tpms_cli_emit_direct(pipe, "{\"error\":\"cannot start radio\"}\r\n");
        tpms_session_free(cli.session);
        furi_string_free(cli.raw_buffer);
        furi_mutex_release(app->radio_mutex);
        return;
    }

    app->cli_sessions++;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->usb_streaming = true;
    furi_mutex_release(app->state_mutex);

    char started[TPMS_LINE_MAX];
    snprintf(
        started,
        sizeof(started),
        "{\"event\":\"started\",\"freq\":%lu,\"mode\":\"%s\",\"wake\":%s}\r\n",
        (unsigned long)frequency,
        raw_mode ? "raw" : "json",
        wake_enabled ? "true" : "false");
    tpms_cli_emit(&cli, started);

    const char* stop_reason = "user";
    uint32_t last_wake = 0;

    while(true) {
        if(cli_is_pipe_broken_or_is_etx_next_char(pipe)) break;
        if(app->stop_requested) {
            stop_reason = "app closing";
            break;
        }
        if(cli.stall_since != 0 &&
           furi_get_tick() - cli.stall_since > furi_ms_to_ticks(TPMS_HOST_STALL_MS)) {
            stop_reason = "host not reading";
            break;
        }

        const bool wake_due =
            wake_enabled &&
            (last_wake == 0 || furi_get_tick() - last_wake > furi_ms_to_ticks(TPMS_LF_PERIOD_MS));

        /* Кнопка Right на самом Flipper тоже должна работать во время
         * USB-сессии — радиомодулем владеет она. */
        if(wake_due || app->wake_requested) {
            app->wake_requested = false;
            last_wake = furi_get_tick();
            tpms_cli_emit(&cli, "{\"event\":\"wake\"}\r\n");
            tpms_session_wake_pulse(cli.session, TPMS_LF_PULSE_MS);
        }

        tpms_session_pump(cli.session, 100);
    }

    if(raw_mode && cli.raw_pending > 0) {
        furi_string_cat_str(cli.raw_buffer, "\r\n");
        tpms_cli_emit(&cli, furi_string_get_cstr(cli.raw_buffer));
    }

    tpms_session_stop(cli.session);

    char stopped[TPMS_LINE_MAX];
    snprintf(
        stopped,
        sizeof(stopped),
        "{\"event\":\"stopped\",\"frames\":%lu,\"overruns\":%lu,\"dropped\":%lu,"
        "\"reason\":\"%s\"}\r\n",
        (unsigned long)cli.frames,
        (unsigned long)tpms_session_get_overruns(cli.session),
        (unsigned long)cli.dropped,
        stop_reason);
    tpms_cli_emit_direct(pipe, stopped);

    tpms_session_free(cli.session);
    furi_string_free(cli.raw_buffer);

    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->usb_streaming = false;
    furi_mutex_release(app->state_mutex);
    app->cli_sessions--;

    furi_mutex_release(app->radio_mutex);
}
