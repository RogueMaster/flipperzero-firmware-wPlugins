#pragma once

#include "tpms_radio.h"
#include "tpms_session.h"
#include "tpms_store.h"

#include <furi.h>
#include <cli/cli.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#define TPMS_CLI_COMMAND_NAME "tpms_rx"

/** Stack size of the CLI command thread. */
#define TPMS_CLI_STACK_SIZE (4 * 1024)

/** How long to wait for the CLI session to finish on exit, ms. */
#define TPMS_CLI_STOP_TIMEOUT_MS 3000

/** How long the CLI command waits for local RX to release the radio. */
#define TPMS_RADIO_YIELD_TIMEOUT_MS 2000

/** Application screen. */
typedef enum {
    TpmsScreenList, /**< sensor list */
    TpmsScreenDetail, /**< details of the selected sensor */
} TpmsScreen;

/** Shared state: what gets drawn on the screen, plus arbitration of the
 * radio between local reception and the USB session. */
typedef struct {
    FuriMutex* state_mutex;

    /* The radio is owned either by local RX or by the CLI session. */
    FuriMutex* radio_mutex;

    /* While a CLI command is running the app must not be unloaded: the
     * command code lives in this very .fap. */
    volatile uint32_t cli_sessions;
    volatile bool stop_requested;

    /* The CLI session asks local RX to release the radio and to keep off
     * it while the flag is raised. */
    volatile bool radio_yield_requested;

    volatile bool local_rx; /**< the local RX thread should be running */
    uint32_t radio_retry_tick; /**< when the radio was last claimed */
    bool usb_streaming;
    bool exit_blocked;
    volatile bool wake_requested; /**< single pulse requested by a key */
    volatile bool auto_wake; /**< periodic pulses */

    /* Radio configuration, changed from the keys and applied by whoever
     * owns the radio. */
    volatile uint8_t config; /**< TpmsConfig */
    volatile uint8_t active_slot; /**< the combination on air right now */
    uint8_t scan_step; /**< which combination the scan is on */
    uint32_t scan_tick; /**< when the scan last stepped */

    TpmsStore store;
    TpmsScreen screen;
    uint8_t selected;
    uint8_t scroll;

    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    CliRegistry* cli_registry;
    FuriThread* local_thread;
} TpmsBridgeApp;

/** Store a received frame in the shared state (for the screen). */
void tpms_bridge_report_frame(TpmsBridgeApp* app, const TpmsFrame* frame, float rssi_dbm);

/** Keep the radio in step with the settings, and step the scan along.
 * Called from whichever loop owns the radio. */
void tpms_bridge_tune_radio(TpmsBridgeApp* app, TpmsSession* session);

/** Implementation of the tpms_rx CLI command. */
void tpms_cli_command(PipeSide* pipe, FuriString* args, void* context);
