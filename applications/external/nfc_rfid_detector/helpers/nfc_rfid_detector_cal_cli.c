#include "nfc_rfid_detector_cal_cli.h"
#include "nfc_rfid_detector_event.h"
#include "../views/nfc_rfid_detector_view_cal.h"

#include <furi.h>
#include <furi_hal_bus.h>
#include <furi_hal_power.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_tim.h>
#include <cli/cli.h>
#include <toolbox/pipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFC_RFID_DETECTOR_CAL_CLI_NAME "rfid_detector_cal"

// Protocol in tools/README.md

struct NfcRfidDetectorCalCli {
    ViewDispatcher* view_dispatcher;
    FuriMutex* mutex;
    volatile bool allowed;
    volatile bool stop;
    volatile int16_t calibration; // the saved calibration, 0.1 ppm
    volatile int16_t received; // result from the computer, saved by the session scene
    volatile int16_t received_sig; // its ±, 0.1 ppm (< 0: not sent)
    volatile int8_t save_result; // 0: not yet, 1: saved, -1: failed
    volatile NfcRfidDetectorCalCliProgress progress;
};

// The number of rfid_detector_cal calls in progress. A call can still be running after the command is deleted.
// nfc_rfid_detector_cal_cli_free() waits until the count drops to zero.
static volatile uint32_t nfc_rfid_detector_cal_cli_calls;

static bool nfc_rfid_detector_cal_cli_live(NfcRfidDetectorCalCli* cli) {
    return cli->allowed && !cli->stop;
}

static void
    nfc_rfid_detector_cal_cli_handle_set_line(NfcRfidDetectorCalCli* cli, const char* line) {
    // the value, then optionally its ±
    char* value_end;
    long value = strtol(line, &value_end, 10);
    long sig = -1;
    char* sig_end = value_end;
    if(*value_end == ' ') sig = strtol(value_end, &sig_end, 10);
    if(value_end == line || *sig_end || value > NFC_RFID_DETECTOR_CAL_LIMIT ||
       value < -NFC_RFID_DETECTOR_CAL_LIMIT || sig > NFC_RFID_DETECTOR_CAL_LIMIT || sig < -1) {
        printf("ERR bad value\r\n");
        fflush(stdout);
        return;
    }
    cli->received = (int16_t)value;
    cli->received_sig = (int16_t)sig;
    cli->save_result = 0;
    view_dispatcher_send_custom_event(cli->view_dispatcher, NfcRfidDetectorCustomEventCalReceived);
    // Wait up to 10 s for the session screen to save the value (no other screen saves it). A save takes milliseconds.
    // The long bound keeps a slow SD card from saving after the command has replied ERR. tools/ waits a little longer.
    for(int i = 0; i < 1000 && !cli->save_result && nfc_rfid_detector_cal_cli_live(cli); i++) {
        furi_delay_ms(10);
    }
    if(cli->save_result > 0) {
        printf("SET %ld\r\n", value);
    } else {
        printf("ERR not saved\r\n");
    }
    fflush(stdout);
}

static long nfc_rfid_detector_cal_cli_clamp(long value, long limit) {
    if(value > limit) return limit;
    if(value < -limit) return -limit;
    return value;
}

static long nfc_rfid_detector_cal_cli_clamp_seconds(long seconds) {
    if(seconds < 0) return 0;
    if(seconds > UINT16_MAX) return UINT16_MAX;
    return seconds;
}

static void
    nfc_rfid_detector_cal_cli_handle_progress_line(NfcRfidDetectorCalCli* cli, const char* line) {
    // elapsed, total, then optionally the estimate and its ± (-1 means no margin of error yet)
    long numbers[4] = {0, 0, 0, -1};
    const char* text = line;
    int count = 0;
    while(count < 4) {
        char* number_end;
        long number = strtol(text, &number_end, 10);
        if(number_end == text) break;
        numbers[count++] = number;
        text = number_end;
    }
    if(count < 2) return;
    cli->progress.elapsed = (uint16_t)nfc_rfid_detector_cal_cli_clamp_seconds(numbers[0]);
    cli->progress.total = (uint16_t)nfc_rfid_detector_cal_cli_clamp_seconds(numbers[1]);
    cli->progress.estimate =
        (int16_t)nfc_rfid_detector_cal_cli_clamp(numbers[2], NFC_RFID_DETECTOR_CAL_LIMIT);
    long sig = numbers[3];
    if(sig < 0) sig = -1;
    if(sig > NFC_RFID_DETECTOR_CAL_LIMIT) sig = NFC_RFID_DETECTOR_CAL_LIMIT;
    cli->progress.estimate_sig = (int16_t)sig;
    view_dispatcher_send_custom_event(cli->view_dispatcher, NfcRfidDetectorCustomEventCalProgress);
}

// Record the Flipper's temperature. The crystal oscillator frequency shifts about 0.25 ppm for every 1 °C change.
// The battery gauge measures in 0.1 °C steps, and on the bench that tracked the crystal's drift more closely than
// the MCU's own temperature sensor, which reads in whole degrees and is only accurate to a few degrees.
static void nfc_rfid_detector_cal_cli_report_temperature(void) {
    float celsius = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    printf("T %d\r\n", (int)(celsius * 10.0f + (celsius < 0 ? -0.5f : 0.5f)));
    fflush(stdout);
}

static bool nfc_rfid_detector_cal_cli_is_number_char(char character) {
    return (character >= '0' && character <= '9') || character == '-' || character == '+' ||
           character == ' ';
}

static void nfc_rfid_detector_cal_cli_session(PipeSide* pipe, NfcRfidDetectorCalCli* cli) {
    furi_hal_power_insomnia_enter(); // sleep would switch the system clock off the crystal
    furi_hal_bus_enable(FuriHalBusTIM2);
    LL_TIM_SetPrescaler(TIM2, 0);
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(TIM2, 0xFFFFFFFF);
    LL_TIM_GenerateEvent_UPDATE(TIM2);
    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_EnableCounter(TIM2);
    printf("OK lfcal 64000000 %d\r\n", cli->calibration);
    fflush(stdout);
    view_dispatcher_send_custom_event(
        cli->view_dispatcher, NfcRfidDetectorCustomEventCalSessionStart);

    char line[32];
    size_t line_length = 0;
    char kind = 0; // 's' or 'p' while reading its line
    while(nfc_rfid_detector_cal_cli_live(cli) && pipe_state(pipe) == PipeStateOpen) {
        if(!pipe_bytes_available(pipe)) {
            furi_delay_tick(1);
            continue;
        }
        char character;
        pipe_receive(pipe, &character, 1);
        if(kind) {
            if(character == '\r' || character == '\n') {
                line[line_length] = '\0';
                if(kind == 's') {
                    nfc_rfid_detector_cal_cli_handle_set_line(cli, line);
                } else {
                    nfc_rfid_detector_cal_cli_handle_progress_line(cli, line);
                }
                kind = 0;
                continue;
            }
            if(nfc_rfid_detector_cal_cli_is_number_char(character)) {
                // replace a line too long to be valid with one that fails to parse
                if(line_length < sizeof(line) - 1) {
                    line[line_length++] = character;
                } else {
                    line[0] = 'x';
                }
                continue;
            }
            // drop a line cut short, and read this character as a command
            kind = 0;
        }
        if(character == '\r' || character == '\n') continue;
        if(character == 's' || character == 'p') {
            kind = character;
            line_length = 0;
            continue;
        }
        if(character == 't') {
            nfc_rfid_detector_cal_cli_report_temperature();
            continue;
        }
        if(character != 'r') break;
        uint32_t timer_count = LL_TIM_GetCounter(TIM2);
        int on_pll = LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_PLL;
        printf("%lu %d\r\n", (unsigned long)timer_count, on_pll);
        fflush(stdout);
    }

    LL_TIM_DisableCounter(TIM2);
    furi_hal_bus_disable(FuriHalBusTIM2);
    furi_hal_power_insomnia_exit();
    view_dispatcher_send_custom_event(
        cli->view_dispatcher, NfcRfidDetectorCustomEventCalSessionEnd);
    printf("STOPPED\r\n");
}

static void nfc_rfid_detector_cal_cli_execute(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    NfcRfidDetectorCalCli* cli = context;
    __atomic_add_fetch(&nfc_rfid_detector_cal_cli_calls, 1, __ATOMIC_SEQ_CST);
    // while the app is still detecting, ask it to open the Computer calibration screen, which stops detection and
    // hands the timer to the command
    if(!cli->allowed) {
        view_dispatcher_send_custom_event(cli->view_dispatcher, NfcRfidDetectorCustomEventCalOpen);
        for(int i = 0; i < 40 && !cli->allowed; i++) {
            furi_delay_ms(50);
        }
    }
    // The mutex guards the field counter's timer (TIM2). A session holds the mutex from start to end, and a second
    // call finds it taken and replies ERR busy. Before detection restarts on the timer,
    // nfc_rfid_detector_cal_cli_allow(false) stops any session. It then waits for the mutex, which the session
    // releases when it ends.
    // allow(true) sets the allowed flag while it still holds the mutex. A call that sees the flag in that moment
    // waits here instead of replying busy.
    if(furi_mutex_acquire(cli->mutex, furi_ms_to_ticks(100)) != FuriStatusOk) {
        printf("ERR busy\r\n");
    } else {
        if(!cli->allowed || furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
            printf("ERR the RFID detector app could not open its Computer calibration screen\r\n");
        } else {
            // Clear the stop flag that an earlier session's Cancel may have left set. If allow(false) runs during this
            // session, the session still ends, because allow(false) also clears the allowed flag.
            cli->stop = false;
            nfc_rfid_detector_cal_cli_session(pipe, cli);
        }
        furi_mutex_release(cli->mutex);
    }
    __atomic_sub_fetch(&nfc_rfid_detector_cal_cli_calls, 1, __ATOMIC_SEQ_CST);
}

NfcRfidDetectorCalCli* nfc_rfid_detector_cal_cli_alloc(ViewDispatcher* view_dispatcher) {
    NfcRfidDetectorCalCli* cli = malloc(sizeof(NfcRfidDetectorCalCli));
    memset(cli, 0, sizeof(NfcRfidDetectorCalCli));
    cli->view_dispatcher = view_dispatcher;
    cli->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        registry,
        NFC_RFID_DETECTOR_CAL_CLI_NAME,
        CliCommandFlagParallelSafe | CliCommandFlagInsomniaSafe,
        nfc_rfid_detector_cal_cli_execute,
        cli);
    furi_record_close(RECORD_CLI);
    return cli;
}

void nfc_rfid_detector_cal_cli_free(NfcRfidDetectorCalCli* cli) {
    furi_assert(cli);
    nfc_rfid_detector_cal_cli_allow(cli, false);
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(registry, NFC_RFID_DETECTOR_CAL_CLI_NAME);
    furi_record_close(RECORD_CLI);
    // Give a call dispatched just before the command was deleted time to start, then wait for every call to return.
    // The delay is a best-effort bound since the CLI reports a call only once it has started.
    furi_delay_ms(100);
    while(__atomic_load_n(&nfc_rfid_detector_cal_cli_calls, __ATOMIC_SEQ_CST)) {
        furi_delay_ms(10);
    }
    furi_mutex_free(cli->mutex);
    free(cli);
}

void nfc_rfid_detector_cal_cli_allow(NfcRfidDetectorCalCli* cli, bool allow) {
    furi_assert(cli);
    if(!allow) {
        // Set the flags before waiting on the mutex. A running session ends on them, and a session about to
        // start ends at once.
        cli->allowed = false;
        cli->stop = true;
    }
    furi_check(furi_mutex_acquire(cli->mutex, FuriWaitForever) == FuriStatusOk);
    cli->allowed = allow;
    furi_mutex_release(cli->mutex);
}

void nfc_rfid_detector_cal_cli_cancel(NfcRfidDetectorCalCli* cli) {
    furi_assert(cli);
    cli->stop = true;
}

void nfc_rfid_detector_cal_cli_set_calibration(NfcRfidDetectorCalCli* cli, int16_t value) {
    furi_assert(cli);
    cli->calibration = value;
}

void nfc_rfid_detector_cal_cli_get_progress(
    NfcRfidDetectorCalCli* cli,
    NfcRfidDetectorCalCliProgress* progress) {
    furi_assert(cli);
    furi_assert(progress);
    progress->elapsed = cli->progress.elapsed;
    progress->total = cli->progress.total;
    progress->estimate = cli->progress.estimate;
    progress->estimate_sig = cli->progress.estimate_sig;
}

void nfc_rfid_detector_cal_cli_get_received(
    NfcRfidDetectorCalCli* cli,
    int16_t* value,
    int16_t* sig) {
    furi_assert(cli);
    *value = cli->received;
    *sig = cli->received_sig;
}

void nfc_rfid_detector_cal_cli_set_save_result(NfcRfidDetectorCalCli* cli, bool saved) {
    furi_assert(cli);
    cli->save_result = saved ? 1 : -1;
}
