#include "tpms_bridge.h"
#include "tpms_lf.h"

#include <furi.h>
#include <toolbox/args.h>
#include <toolbox/cli/cli_command.h>
#include <toolbox/pipe.h>

#include <stdio.h>

#define TAG "TpmsCli"

/* How many intervals raw mode accumulates before printing. Keep the batch
 * small: the pipe buffer is modest and long lines only get in the way. */
#define TPMS_RAW_FLUSH_EVERY 24

/* How long to wait for pipe buffer space for a single line, ms. */
#define TPMS_WRITE_TIMEOUT_MS 500

/* Cap for raw mode: without it FSK noise floods the channel in seconds. */
#define TPMS_RAW_MAX_INTERVALS 200000UL

/* If the host stops reading for this many milliseconds we assume it is
 * gone and release the radio. Otherwise the session hangs forever and
 * keeps holding it. */
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
    uint32_t stall_since; /**< 0 means writes are getting through */
    FuriString* raw_buffer;
} TpmsCliSession;

/** Write to the CLI with a bounded wait.
 *
 * printf is not an option here: if the host went away without closing the
 * command, the pipe buffer fills up and the command thread blocks in the
 * write forever, holding the radio.
 *
 * We write in chunks that fit the free space — the pipe buffer is
 * noticeably smaller than a raw mode line, and demanding room for the
 * whole line at once would mean dropping everything. If there is no room
 * for longer than TPMS_WRITE_TIMEOUT_MS the line is lost, but control
 * returns and the loop gets a chance to notice Ctrl+C or a departed host.
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

static void tpms_cli_print_frame(const TpmsFrame* frame, float rssi_dbm, void* context) {
    TpmsCliSession* cli = context;
    cli->frames++;

    char raw_hex[TPMS_RAW_MAX * 2 + 1];
    for(size_t i = 0; i < frame->raw_len; i++) {
        static const char digits[] = "0123456789abcdef";
        raw_hex[i * 2] = digits[frame->raw[i] >> 4];
        raw_hex[i * 2 + 1] = digits[frame->raw[i] & 0x0F];
    }
    raw_hex[frame->raw_len * 2] = '\0';

    /* Integers only: the firmware printf is not required to support %f.
     * Pressure goes out in hundredths of a kPa, RSSI in tenths of a dBm.
     * Fields the protocol does not carry are left out rather than sent
     * as a zero. */
    char pressure[32] = "";
    if(frame->have & TPMS_HAS_PRESSURE) {
        snprintf(
            pressure,
            sizeof(pressure),
            ",\"pressure_kpa_x100\":%ld",
            (long)frame->pressure_kpa_x100);
    }

    char temperature[24] = "";
    if(frame->have & TPMS_HAS_TEMP) {
        snprintf(temperature, sizeof(temperature), ",\"temp_c\":%d", (int)frame->temperature_c);
    }

    char battery[24] = "";
    if(frame->have & TPMS_HAS_BATTERY) {
        snprintf(
            battery,
            sizeof(battery),
            ",\"battery_ok\":%s",
            (frame->have & TPMS_BATTERY_LOW) ? "false" : "true");
    }

    const uint8_t digits = tpms_protocols[frame->protocol].id_digits;

    char line[TPMS_LINE_MAX];
    snprintf(
        line,
        sizeof(line),
        "{\"t\":%lu,\"proto\":\"%s\",\"id\":\"%0*lx\",\"raw\":\"%s\"%s%s%s,"
        "\"flags\":%u,\"rssi_dbm_x10\":%ld}\r\n",
        (unsigned long)furi_get_tick(),
        tpms_protocol_id(frame->protocol),
        (int)digits,
        (unsigned long)frame->id,
        raw_hex,
        pressure,
        temperature,
        battery,
        (unsigned)frame->flags,
        (long)(rssi_dbm * 10.0f));

    tpms_cli_emit(cli, line);
    tpms_bridge_report_frame(cli->app, frame, rssi_dbm);
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

    const uint8_t current_slot = app->config == TpmsConfigScan ?
                                     app->active_slot :
                                     (uint8_t)(app->config % TPMS_SLOT_COUNT);

    uint32_t frequency = tpms_slot_frequency(current_slot);
    TpmsModulation modulation = tpms_slot_modulation(current_slot);
    uint8_t band = (uint8_t)(current_slot / 2);
    bool pinned_frequency = false;
    bool scan_requested = app->config == TpmsConfigScan;
    bool raw_mode = false;
    bool wake_enabled = false;

    FuriString* word = furi_string_alloc();
    if(args_read_string_and_trim(args, word)) {
        const char* text = furi_string_get_cstr(word);
        char* end = NULL;
        const unsigned long parsed = strtoul(text, &end, 10);
        if(end == text || parsed == 0) {
            tpms_cli_emit_direct(
                pipe,
                "Usage: " TPMS_CLI_COMMAND_NAME
                " [frequency_hz] [json|raw] [wake] [fsk|ook|scan]\r\n");
            furi_string_free(word);
            return;
        }
        frequency = (uint32_t)parsed;

        /* A frequency that is one of the bands the keys switch between
         * keeps following those keys; any other one is pinned. */
        pinned_frequency = true;
        for(uint8_t i = 0; i < TPMS_FREQUENCY_COUNT; i++) {
            if(tpms_frequencies[i] == frequency) {
                band = i;
                pinned_frequency = false;
                break;
            }
        }

        while(args_read_string_and_trim(args, word)) {
            if(furi_string_equal_str(word, "raw")) {
                raw_mode = true;
            } else if(furi_string_equal_str(word, "wake")) {
                wake_enabled = true;
            } else if(furi_string_equal_str(word, "fsk")) {
                modulation = TpmsModulationFsk;
                scan_requested = false;
            } else if(furi_string_equal_str(word, "ook")) {
                modulation = TpmsModulationOok;
                scan_requested = false;
            } else if(furi_string_equal_str(word, "scan")) {
                scan_requested = true;
            } else if(!furi_string_equal_str(word, "json")) {
                tpms_cli_emit_direct(
                    pipe, "Unknown option, expected json, raw, wake, fsk, ook or scan\r\n");
                furi_string_free(word);
                return;
            }
        }
    }
    furi_string_free(word);

    /* Unless the frequency was pinned to something outside the two bands,
     * the session follows the same setting the screen shows, so the keys
     * keep working while a computer is listening. */
    if(!pinned_frequency) {
        app->config = scan_requested ?
                          (uint8_t)TpmsConfigScan :
                          (uint8_t)(band * 2 + (modulation == TpmsModulationOok ? 1 : 0));
    }

    /* The radio is almost always busy with local reception — it starts on
     * its own so that the app works without a computer. Raise the flag:
     * local RX will see it, release the radio and stay off it for as long
     * as the USB session lasts. */
    app->radio_yield_requested = true;

    bool radio_acquired = false;
    for(uint32_t waited = 0; waited <= TPMS_RADIO_YIELD_TIMEOUT_MS; waited += 50) {
        if(furi_mutex_acquire(app->radio_mutex, 50) == FuriStatusOk) {
            radio_acquired = true;
            break;
        }
    }

    if(!radio_acquired) {
        app->radio_yield_requested = false;
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

    if(!tpms_session_start(cli.session, frequency, modulation)) {
        tpms_cli_emit_direct(pipe, "{\"error\":\"cannot start radio\"}\r\n");
        tpms_session_free(cli.session);
        furi_string_free(cli.raw_buffer);
        furi_mutex_release(app->radio_mutex);
        app->radio_yield_requested = false;
        return;
    }
    if(!pinned_frequency)
        app->active_slot = (uint8_t)(band * 2 + (modulation == TpmsModulationOok ? 1 : 0));

    app->cli_sessions++;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);
    app->usb_streaming = true;
    furi_mutex_release(app->state_mutex);

    char started[TPMS_LINE_MAX];
    snprintf(
        started,
        sizeof(started),
        "{\"event\":\"started\",\"freq\":%lu,\"mode\":\"%s\",\"wake\":%s,\"radio\":\"%s\","
        "\"protocols\":%u}\r\n",
        (unsigned long)frequency,
        raw_mode ? "raw" : "json",
        wake_enabled ? "true" : "false",
        app->config == TpmsConfigScan ? "scan" : (modulation == TpmsModulationOok ? "ook" : "fsk"),
        (unsigned)tpms_protocol_count);
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

        /* Periodic wake-up is enabled either by a command argument or by
         * a key on the Flipper itself. */
        const bool wake_due =
            (wake_enabled || app->auto_wake) &&
            (last_wake == 0 || furi_get_tick() - last_wake > furi_ms_to_ticks(TPMS_LF_PERIOD_MS));

        /* The Right key on the Flipper must keep working during a USB
         * session too — the session is the one that owns the radio. */
        if(wake_due || app->wake_requested) {
            app->wake_requested = false;
            last_wake = furi_get_tick();
            tpms_cli_emit(&cli, "{\"event\":\"wake\"}\r\n");
            tpms_session_wake_pulse(cli.session, TPMS_LF_PULSE_MS);
        }

        if(!pinned_frequency) tpms_bridge_tune_radio(app, cli.session);
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
    /* The radio is free now — local reception will come back on its own. */
    app->radio_yield_requested = false;
}
