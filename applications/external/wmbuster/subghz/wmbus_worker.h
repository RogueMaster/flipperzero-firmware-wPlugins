#pragma once
/*
 * SubGHz reception worker.
 *
 * Configures the CC1101 for the chosen wM-Bus mode, captures bit-level
 * data, performs sync detection + 3-of-6 / Manchester decoding + CRC
 * verification, and hands off Format A/B link frames to the app.
 *
 * Implementation strategy (see wmbus_worker.c):
 *   - Use furi_hal_subghz with a fully custom register preset.
 *   - Configure CC1101 sync word + infinite packet length.
 *   - Read RXFIFO bytes from a thread; feed them through the codec.
 *   - All heavy work happens off the GUI thread.
 */

#include <stdint.h>
#include <stddef.h>
#include "../wmbus_app_i.h"

WmbusWorker* wmbus_worker_alloc(WmbusApp* app);
void         wmbus_worker_free(WmbusWorker* w);

void wmbus_worker_start(WmbusWorker* w, WmbusMode mode);
void wmbus_worker_stop(WmbusWorker* w);
bool wmbus_worker_is_running(const WmbusWorker* w);

/* Inject a synthetic raw frame (3-of-6 already decoded, CRC included).
 * Used by the "Test Decode" menu and unit tests. */
void wmbus_worker_inject(WmbusWorker* w, const uint8_t* raw, size_t len, int8_t rssi);

/* Live receiver statistics — fed to the scan footer so the user can tell
 * at a glance whether the radio is hearing nothing (sync_locks=0) or
 * hearing plenty but failing CRC (sync_locks high, decoded low).         */
typedef struct {
    uint32_t sync_locks;        /* every time we found a fresh frame      */
    uint32_t decoded_a;         /* Format A frames passing CRC            */
    uint32_t decoded_b;         /* Format B frames passing CRC            */
    uint32_t crc_fails;         /* sync ok, payload CRC failed            */
    uint32_t three_of_six_err;  /* T-mode chip stream malformed           */
    uint32_t fifo_overflows;    /* CC1101 RX FIFO ran full                */
} WmbusWorkerStats;

void wmbus_worker_get_stats(const WmbusWorker* w, WmbusWorkerStats* out);
