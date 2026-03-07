#include "subghz_jammer_worker.h"
#include "subghz_jammer.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/preset.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "JammerWorker"

struct JammerWorker {
    FuriThread* thread;
    volatile bool running;
    /* Shared state — caller holds the mutex around reads */
    JammerState* state;
    FuriMutex* mutex;
    /* Callback fired when a new jammer alert is raised */
    JammerWorkerCallback alert_cb;
    void* alert_cb_ctx;
};

/* ── Rolling-window helpers ── */

static void window_push(JammerState* s, uint8_t idx, float rssi) {
    uint8_t pos = s->window_pos[idx];
    s->rssi_window[idx][pos] = rssi;
    s->window_pos[idx] = (pos + 1) % RSSI_WINDOW_SIZE;

    /* Recompute max over the whole window */
    float max = s->rssi_window[idx][0];
    for(uint8_t i = 1; i < RSSI_WINDOW_SIZE; i++) {
        if(s->rssi_window[idx][i] > max) max = s->rssi_window[idx][i];
    }
    s->window_max[idx] = max;
}

/* ── Worker thread ── */

static int32_t jammer_worker_thread(void* context) {
    JammerWorker* worker = context;

    FURI_LOG_I(TAG, "Worker started");

    /* Grab the notification service for alerts */
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Bring up the SubGHz device stack */
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!device) {
        FURI_LOG_E(TAG, "CC1101 device not found");
        subghz_devices_deinit();
        furi_record_close(RECORD_NOTIFICATION);
        return -1;
    }

    if(!subghz_devices_begin(device)) {
        FURI_LOG_E(TAG, "subghz_devices_begin failed");
        subghz_devices_deinit();
        furi_record_close(RECORD_NOTIFICATION);
        return -1;
    }

    subghz_devices_reset(device);
    subghz_devices_idle(device);

    /* OOK 650 kHz bandwidth — wide enough to catch carrier blobs without
   * demodulation artefacts. Null preset_data uses the built-in register set. */
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);

    while(worker->running) {
        for(uint8_t i = 0; i < MONITOR_FREQ_COUNT && worker->running; i++) {
            /* Tune and receive */
            subghz_devices_idle(device);
            subghz_devices_set_frequency(device, MONITOR_FREQS[i]);
            subghz_devices_set_rx(device);

            /* Let the CC1101 RSSI register settle */
            furi_delay_ms(DWELL_TIME_MS);

            float rssi = subghz_devices_get_rssi(device);
            subghz_devices_idle(device);

            /* Update shared state under the mutex */
            if(furi_mutex_acquire(worker->mutex, 50) == FuriStatusOk) {
                JammerState* s = worker->state;
                s->rssi[i] = rssi;
                window_push(s, i, rssi);

                /* Apply detection thresholds to rolling max */
                float max = s->window_max[i];
                FreqStatus prev_status = s->status[i];

                if(max >= s->threshold_jammer) {
                    s->status[i] = FreqStatusJammer;
                } else if(max >= s->threshold_suspicious) {
                    s->status[i] = FreqStatusSuspicious;
                } else {
                    s->status[i] = FreqStatusOk;
                    s->consecutive[i] = 0;
                }

                /* Consecutive cycle accounting — only count upgrades and holds */
                if(s->status[i] >= FreqStatusSuspicious) {
                    if(s->status[i] >= prev_status) {
                        s->consecutive[i]++;
                    }
                } else {
                    s->consecutive[i] = 0;
                }

                /* Determine worst alert frequency */
                int8_t worst = -1;
                for(uint8_t j = 0; j < MONITOR_FREQ_COUNT; j++) {
                    if(s->status[j] == FreqStatusJammer &&
                       s->consecutive[j] >= ALERT_CONSECUTIVE_COUNT) {
                        if(worst < 0 || s->window_max[j] > s->window_max[(uint8_t)worst]) {
                            worst = (int8_t)j;
                        }
                    }
                }
                /* Fall back to suspicious if no confirmed jammer */
                if(worst < 0) {
                    for(uint8_t j = 0; j < MONITOR_FREQ_COUNT; j++) {
                        if(s->status[j] == FreqStatusSuspicious &&
                           s->consecutive[j] >= ALERT_CONSECUTIVE_COUNT) {
                            if(worst < 0 || s->window_max[j] > s->window_max[(uint8_t)worst]) {
                                worst = (int8_t)j;
                            }
                        }
                    }
                }

                bool newly_alerting = (worst >= 0 && s->alert_freq_idx < 0);
                s->alert_freq_idx = worst;

                furi_mutex_release(worker->mutex);

                /* Fire notifications outside the mutex */
                if(newly_alerting) {
                    AlertMode mode;
                    if(furi_mutex_acquire(worker->mutex, 50) == FuriStatusOk) {
                        mode = worker->state->alert_mode;
                        furi_mutex_release(worker->mutex);
                    } else {
                        mode = AlertModeBlink;
                    }

                    if(mode == AlertModeBlink) {
                        notification_message(notifications, &sequence_blink_red_100);
                    } else if(mode == AlertModeVibrate) {
                        notification_message(notifications, &sequence_blink_red_100);
                        notification_message(notifications, &sequence_single_vibro);
                    }

                    if(worker->alert_cb) {
                        worker->alert_cb(worker->alert_cb_ctx);
                    }
                }
            }
        }
    }

    subghz_devices_idle(device);
    subghz_devices_sleep(device);
    subghz_devices_end(device);
    subghz_devices_deinit();

    furi_record_close(RECORD_NOTIFICATION);

    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

/* ── Public API ── */

JammerWorker* jammer_worker_alloc(JammerState* state, FuriMutex* mutex) {
    JammerWorker* worker = malloc(sizeof(JammerWorker));
    worker->thread = furi_thread_alloc_ex(TAG, 4096, jammer_worker_thread, worker);
    worker->running = false;
    worker->state = state;
    worker->mutex = mutex;
    worker->alert_cb = NULL;
    worker->alert_cb_ctx = NULL;
    return worker;
}

void jammer_worker_free(JammerWorker* worker) {
    furi_assert(worker);
    furi_assert(!worker->running);
    furi_thread_free(worker->thread);
    free(worker);
}

void jammer_worker_set_callback(JammerWorker* worker, JammerWorkerCallback cb, void* ctx) {
    furi_assert(worker);
    worker->alert_cb = cb;
    worker->alert_cb_ctx = ctx;
}

void jammer_worker_start(JammerWorker* worker) {
    furi_assert(worker);
    furi_assert(!worker->running);
    worker->running = true;
    furi_thread_start(worker->thread);
}

void jammer_worker_stop(JammerWorker* worker) {
    furi_assert(worker);
    if(!worker->running) return;
    worker->running = false;
    furi_thread_join(worker->thread);
}

bool jammer_worker_is_running(JammerWorker* worker) {
    furi_assert(worker);
    return worker->running;
}
