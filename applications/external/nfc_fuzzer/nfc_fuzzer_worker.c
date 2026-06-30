/* nfc_fuzzer_worker.c — Background worker thread for NFC Fuzzer.
 * Drives the NFC poller to emit fuzzed UIDs and logs results to SD card. */

#include "nfc_fuzzer_worker.h"
#include "nfc_fuzzer_profiles.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>

#define WORKER_TAG          "NfcFuzzerWorker"
#define WORKER_THREAD_STACK (8 * 1024)
#define WORKER_THREAD_NAME  "NfcFuzzerWorkerThread"

/* Average response time tracking for timing-anomaly detection. */
#define TIMING_WINDOW_SIZE    16
#define TIMING_ANOMALY_FACTOR 3

struct NfcFuzzerWorker {
    FuriThread* thread;
    volatile bool running;
    volatile bool stop_requested;

    NfcFuzzerProfile profile;
    NfcFuzzerStrategy strategy;
    NfcFuzzerSettings settings;

    NfcFuzzerWorkerCallback callback;
    void* cb_context;

    NfcFuzzerWorkerDoneCallback done_callback;
    void* done_cb_context;
};

/* ───── NFC listener event callback ───── */

typedef struct {
    volatile bool response_received;
    BitBuffer* rx_buf;
    uint32_t response_tick;
} NfcFuzzerListenerCtx;

static NfcCommand nfc_fuzzer_listener_callback(NfcGenericEvent event, void* context) {
    NfcFuzzerListenerCtx* ctx = context;
    furi_assert(ctx);

    /* event_data may be NULL for field-on/off events — guard before deref. */
    if(!event.event_data) return NfcCommandContinue;

    Iso14443_3aListenerEvent* iso_event = event.event_data;
    if(iso_event->type == Iso14443_3aListenerEventTypeReceivedData) {
        ctx->response_received = true;
        ctx->response_tick = furi_get_tick();
        if(ctx->rx_buf && iso_event->data && iso_event->data->buffer) {
            bit_buffer_copy_bytes(
                ctx->rx_buf,
                bit_buffer_get_data(iso_event->data->buffer),
                bit_buffer_get_size_bytes(iso_event->data->buffer));
        }
    }
    return NfcCommandContinue;
}

/* ───── Timing anomaly helpers ───── */

typedef struct {
    uint32_t samples[TIMING_WINDOW_SIZE];
    uint32_t count;
    uint32_t index;
    uint32_t sum;
} TimingTracker;

static void timing_tracker_init(TimingTracker* t) {
    memset(t, 0, sizeof(TimingTracker));
}

static bool timing_tracker_check(TimingTracker* t, uint32_t elapsed_ms) {
    bool anomaly = false;
    if(t->count >= 4) {
        uint32_t avg = t->sum / t->count;
        if(avg > 0 && elapsed_ms > avg * TIMING_ANOMALY_FACTOR) {
            anomaly = true;
        }
    }
    /* Update rolling window */
    if(t->count < TIMING_WINDOW_SIZE) {
        t->samples[t->count] = elapsed_ms;
        t->sum += elapsed_ms;
        t->count++;
    } else {
        t->sum -= t->samples[t->index];
        t->samples[t->index] = elapsed_ms;
        t->sum += elapsed_ms;
        t->index = (t->index + 1) % TIMING_WINDOW_SIZE;
    }
    return anomaly;
}

/* ───── Helper: build Iso14443_3aData for listener with given UID/ATQA/SAK ───── */

static Iso14443_3aData* nfc_fuzzer_build_iso14443_3a_data(
    const uint8_t* uid,
    uint8_t uid_len,
    const uint8_t* atqa,
    uint8_t sak) {
    Iso14443_3aData* data = iso14443_3a_alloc();
    data->uid_len = uid_len;
    memcpy(data->uid, uid, uid_len);
    data->atqa[0] = atqa[0];
    data->atqa[1] = atqa[1];
    data->sak = sak;
    return data;
}

/* ───── Listener-mode fuzz loop (fuzzing readers) ───── */

static void nfc_fuzzer_worker_run_listener(NfcFuzzerWorker* worker) {
    Nfc* nfc = nfc_alloc();
    furi_assert(nfc);

    /* Heap-allocate large buffers to reduce stack usage */
    BitBuffer* tx_buf = bit_buffer_alloc(NFC_FUZZER_MAX_PAYLOAD_LEN);
    BitBuffer* rx_buf = bit_buffer_alloc(NFC_FUZZER_MAX_PAYLOAD_LEN);

    NfcFuzzerListenerCtx listener_ctx = {
        .response_received = false,
        .rx_buf = rx_buf,
        .response_tick = 0,
    };

    /* Heap-allocate test case and result to reduce stack usage */
    NfcFuzzerTestCase* test_case = malloc(sizeof(NfcFuzzerTestCase));
    NfcFuzzerResult* result = malloc(sizeof(NfcFuzzerResult));
    if(!test_case || !result) {
        FURI_LOG_E(WORKER_TAG, "Failed to allocate test_case or result");
        free(test_case);
        free(result);
        bit_buffer_free(tx_buf);
        bit_buffer_free(rx_buf);
        nfc_free(nfc);
        return;
    }

    nfc_fuzzer_profile_init(worker->profile, worker->strategy);

    uint32_t max = nfc_fuzzer_max_cases(worker->settings.max_cases_index);
    uint32_t total = nfc_fuzzer_profile_total_cases(worker->profile, worker->strategy);
    if(total > max) total = max;

    uint32_t timeout_ms = nfc_fuzzer_timeout_ms(worker->settings.timeout_index);
    uint32_t delay_ms = nfc_fuzzer_delay_ms(worker->settings.delay_index);

    TimingTracker timing;
    timing_tracker_init(&timing);

    /* Default collision resolution data (UID, ATQA, SAK) is set through the
     * NfcDeviceData passed to nfc_listener_alloc(). No separate
     * nfc_iso14443a_listener_set_col_res_data() call is needed. */
    uint8_t default_uid[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t default_atqa[] = {0x44, 0x00};
    uint8_t default_sak = 0x00;

    /* Build initial NFC device data and allocate listener using the correct API */
    Iso14443_3aData* nfc_data = nfc_fuzzer_build_iso14443_3a_data(
        default_uid, sizeof(default_uid), default_atqa, default_sak);

    NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3a, nfc_data);
    nfc_listener_start(listener, nfc_fuzzer_listener_callback, &listener_ctx);

    for(uint32_t i = 0; i < total && !worker->stop_requested; i++) {
        /* Generate next test case */
        bool has_case = nfc_fuzzer_profile_next(worker->profile, worker->strategy, i, test_case);
        if(!has_case) break;

        /* For UID / ATQA+SAK profiles, we must restart the listener with new
         * collision resolution data. The UID/ATQA/SAK is embedded in the
         * NfcDeviceData passed to nfc_listener_alloc(). */
        if(worker->profile == NfcFuzzerProfileUid) {
            nfc_listener_stop(listener);
            nfc_listener_free(listener);
            iso14443_3a_free(nfc_data);
            uint8_t atqa_default[] = {0x44, 0x00};
            nfc_data = nfc_fuzzer_build_iso14443_3a_data(
                test_case->data, test_case->data_len, atqa_default, 0x00);
            listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3a, nfc_data);
            nfc_listener_start(listener, nfc_fuzzer_listener_callback, &listener_ctx);
        } else if(worker->profile == NfcFuzzerProfileAtqaSak) {
            nfc_listener_stop(listener);
            nfc_listener_free(listener);
            iso14443_3a_free(nfc_data);
            /* data[0..1] = ATQA, data[2] = SAK */
            uint8_t uid7[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
            uint8_t atqa_buf[2];
            atqa_buf[0] = test_case->data[0];
            atqa_buf[1] = test_case->data[1];
            uint8_t sak_val = test_case->data[2];
            nfc_data = nfc_fuzzer_build_iso14443_3a_data(uid7, sizeof(uid7), atqa_buf, sak_val);
            listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3a, nfc_data);
            nfc_listener_start(listener, nfc_fuzzer_listener_callback, &listener_ctx);
        }

        /* Prepare TX frame for frame-level profiles */
        if(worker->profile == NfcFuzzerProfileFrame || worker->profile == NfcFuzzerProfileNtag ||
           worker->profile == NfcFuzzerProfileIso15693) {
            bit_buffer_reset(tx_buf);
            bit_buffer_copy_bytes(tx_buf, test_case->data, test_case->data_len);
        }

        /* Wait for reader interaction or timeout */
        listener_ctx.response_received = false;
        uint32_t start_tick = furi_get_tick();

        /* For frame-level profiles, send malformed data via the listener */
        if(worker->profile == NfcFuzzerProfileFrame || worker->profile == NfcFuzzerProfileNtag ||
           worker->profile == NfcFuzzerProfileIso15693) {
            nfc_listener_tx(nfc, tx_buf);
        }

        /* Wait for response with timeout */
        while(!listener_ctx.response_received && !worker->stop_requested) {
            uint32_t elapsed = furi_get_tick() - start_tick;
            if(elapsed >= timeout_ms) break;
            furi_delay_ms(1);
        }

        uint32_t elapsed_ms = furi_get_tick() - start_tick;

        /* Analyze response */
        NfcFuzzerAnomalyType anomaly = NfcFuzzerAnomalyNone;

        if(!listener_ctx.response_received) {
            /* Timeout -- only flag as anomaly after we have baseline timing */
            if(i > TIMING_WINDOW_SIZE) {
                anomaly = NfcFuzzerAnomalyTimeout;
            }
        } else {
            /* Check for timing anomaly */
            if(timing_tracker_check(&timing, elapsed_ms)) {
                anomaly = NfcFuzzerAnomalyTimingAnomaly;
            }
        }

        /* Build result and report */
        memset(result, 0, sizeof(NfcFuzzerResult));
        result->test_num = i + 1;
        memcpy(result->payload, test_case->data, test_case->data_len);
        result->payload_len = test_case->data_len;
        result->anomaly = anomaly;

        if(listener_ctx.response_received && bit_buffer_get_size_bytes(rx_buf) > 0) {
            size_t rx_len = bit_buffer_get_size_bytes(rx_buf);
            if(rx_len > NFC_FUZZER_MAX_PAYLOAD_LEN) rx_len = NFC_FUZZER_MAX_PAYLOAD_LEN;
            bit_buffer_write_bytes(rx_buf, result->response, rx_len);
            result->response_len = (uint8_t)rx_len;
        }

        if(worker->callback) {
            worker->callback(
                (anomaly != NfcFuzzerAnomalyNone) ? result : NULL,
                i + 1,
                total,
                test_case->data,
                test_case->data_len,
                worker->cb_context);
        }

        /* Auto-stop on anomaly */
        if(anomaly != NfcFuzzerAnomalyNone && worker->settings.auto_stop) {
            break;
        }

        /* Inter-test delay */
        if(delay_ms > 0 && !worker->stop_requested) {
            furi_delay_ms(delay_ms);
        }

        bit_buffer_reset(rx_buf);
    }

    /* Clean up: stop and free listener */
    nfc_listener_stop(listener);
    nfc_listener_free(listener);
    iso14443_3a_free(nfc_data);
    free(result);
    free(test_case);
    bit_buffer_free(tx_buf);
    bit_buffer_free(rx_buf);
    nfc_free(nfc);
}

/* ───── Poller-mode fuzz context (shared with poller callback) ───── */

typedef struct {
    NfcFuzzerWorker* worker;
    BitBuffer* tx_buf;
    BitBuffer* rx_buf;
    NfcFuzzerTestCase* test_case;
    NfcFuzzerResult* result;
    TimingTracker timing;
    uint32_t current_index;
    uint32_t total;
    uint32_t timeout_fc;
    uint32_t delay_ms;
    volatile bool done;
} PollerFuzzCtx;

/* Poller callback — runs on the NFC poller thread.
 * nfc_poller_trx() MUST be called from here, not from an external thread.
 * Calling it from the worker thread triggers furi_check in SDK 1.4.3. */
static NfcCommand nfc_fuzzer_poller_fuzz_cb(NfcGenericEvent event, void* context) {
    PollerFuzzCtx* ctx = (PollerFuzzCtx*)context;
    NfcFuzzerWorker* worker = ctx->worker;

    if(ctx->done || worker->stop_requested || ctx->current_index >= ctx->total) {
        ctx->done = true;
        return NfcCommandStop;
    }

    uint32_t i = ctx->current_index;

    bool has_case = nfc_fuzzer_profile_next(worker->profile, worker->strategy, i, ctx->test_case);
    if(!has_case) {
        ctx->done = true;
        return NfcCommandStop;
    }

    bit_buffer_reset(ctx->tx_buf);
    bit_buffer_reset(ctx->rx_buf);
    bit_buffer_copy_bytes(ctx->tx_buf, ctx->test_case->data, ctx->test_case->data_len);

    uint32_t start_tick = furi_get_tick();
    NfcError err = nfc_poller_trx(event.instance, ctx->tx_buf, ctx->rx_buf, ctx->timeout_fc);
    uint32_t elapsed_ms = furi_get_tick() - start_tick;

    NfcFuzzerAnomalyType anomaly = NfcFuzzerAnomalyNone;
    if(err == NfcErrorTimeout) {
        anomaly = NfcFuzzerAnomalyTimeout;
    } else if(err != NfcErrorNone) {
        anomaly = NfcFuzzerAnomalyUnexpectedResponse;
    } else if(timing_tracker_check(&ctx->timing, elapsed_ms)) {
        anomaly = NfcFuzzerAnomalyTimingAnomaly;
    }

    memset(ctx->result, 0, sizeof(NfcFuzzerResult));
    ctx->result->test_num = i + 1;
    memcpy(ctx->result->payload, ctx->test_case->data, ctx->test_case->data_len);
    ctx->result->payload_len = ctx->test_case->data_len;
    ctx->result->anomaly = anomaly;

    if(bit_buffer_get_size_bytes(ctx->rx_buf) > 0) {
        size_t rx_len = bit_buffer_get_size_bytes(ctx->rx_buf);
        if(rx_len > NFC_FUZZER_MAX_PAYLOAD_LEN) rx_len = NFC_FUZZER_MAX_PAYLOAD_LEN;
        bit_buffer_write_bytes(ctx->rx_buf, ctx->result->response, rx_len);
        ctx->result->response_len = (uint8_t)rx_len;
    }

    if(worker->callback) {
        worker->callback(
            (anomaly != NfcFuzzerAnomalyNone) ? ctx->result : NULL,
            i + 1,
            ctx->total,
            ctx->test_case->data,
            ctx->test_case->data_len,
            worker->cb_context);
    }

    ctx->current_index++;

    if(anomaly != NfcFuzzerAnomalyNone && worker->settings.auto_stop) {
        ctx->done = true;
        return NfcCommandStop;
    }

    if(ctx->delay_ms > 0) {
        furi_delay_ms(ctx->delay_ms);
    }

    return NfcCommandContinue;
}

/* ───── Poller-mode fuzz loop (fuzzing tags) ───── */

static void nfc_fuzzer_worker_run_poller(NfcFuzzerWorker* worker) {
    Nfc* nfc = nfc_alloc();
    furi_assert(nfc);

    BitBuffer* tx_buf = bit_buffer_alloc(NFC_FUZZER_MAX_PAYLOAD_LEN);
    BitBuffer* rx_buf = bit_buffer_alloc(NFC_FUZZER_MAX_PAYLOAD_LEN);
    NfcFuzzerTestCase* test_case = malloc(sizeof(NfcFuzzerTestCase));
    NfcFuzzerResult* result = malloc(sizeof(NfcFuzzerResult));
    if(!test_case || !result) {
        FURI_LOG_E(WORKER_TAG, "Failed to allocate test_case or result");
        free(test_case);
        free(result);
        bit_buffer_free(tx_buf);
        bit_buffer_free(rx_buf);
        nfc_free(nfc);
        return;
    }

    nfc_fuzzer_profile_init(worker->profile, worker->strategy);

    uint32_t max = nfc_fuzzer_max_cases(worker->settings.max_cases_index);
    uint32_t total = nfc_fuzzer_profile_total_cases(worker->profile, worker->strategy);
    if(total > max) total = max;

    PollerFuzzCtx ctx = {
        .worker = worker,
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .test_case = test_case,
        .result = result,
        .current_index = 0,
        .total = total,
        .timeout_fc = nfc_fuzzer_timeout_ms(worker->settings.timeout_index) * 13560,
        .delay_ms = nfc_fuzzer_delay_ms(worker->settings.delay_index),
        .done = false,
    };
    timing_tracker_init(&ctx.timing);

    /* Start poller with the fuzz callback.  nfc_poller_trx() is called
     * inside the callback on the NFC poller thread — the only safe way
     * to use the poller TRX API.  Calling it from our worker thread
     * triggers furi_check in SDK 1.4.3. */
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(poller, nfc_fuzzer_poller_fuzz_cb, &ctx);

    /* Wait for the poller callback to complete or abort */
    while(!ctx.done && !worker->stop_requested) {
        furi_delay_ms(100);
    }

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    free(result);
    free(test_case);
    bit_buffer_free(tx_buf);
    bit_buffer_free(rx_buf);
    nfc_free(nfc);
}

/* ───── NFC-B listener fuzz loop ───── */
/*
 * Each test case provides a 4-byte PUPI (Pseudo-Unique PICC Identifier).
 * The listener is restarted with each new PUPI so the reader sees a
 * different tag identity on every poll. Tests reader collision-resolution
 * robustness for ISO 14443-B devices (e-passports, transit cards, etc.).
 */
static void nfc_fuzzer_worker_run_listener_nfcb(NfcFuzzerWorker* worker) {
    Nfc* nfc = nfc_alloc();
    furi_assert(nfc);

    NfcFuzzerTestCase* test_case = malloc(sizeof(NfcFuzzerTestCase));
    if(!test_case) {
        FURI_LOG_E(WORKER_TAG, "NFC-B: OOM allocating test buffer");
        nfc_free(nfc);
        return;
    }

    nfc_fuzzer_profile_init(worker->profile, worker->strategy);

    uint32_t max = nfc_fuzzer_max_cases(worker->settings.max_cases_index);
    uint32_t total = nfc_fuzzer_profile_total_cases(worker->profile, worker->strategy);
    if(total > max) total = max;

    uint32_t timeout_ms = nfc_fuzzer_timeout_ms(worker->settings.timeout_index);
    uint32_t delay_ms = nfc_fuzzer_delay_ms(worker->settings.delay_index);

    /* Build an initial ISO 14443-3B data block with a placeholder PUPI. */
    uint8_t default_pupi[4] = {0x01, 0x02, 0x03, 0x04};
    Iso14443_3bData* nfc_data = iso14443_3b_alloc();
    iso14443_3b_set_uid(nfc_data, default_pupi, sizeof(default_pupi));

    NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3b, nfc_data);
    nfc_listener_start(listener, NULL, NULL);

    for(uint32_t i = 0; i < total && !worker->stop_requested; i++) {
        bool has_case = nfc_fuzzer_profile_next(worker->profile, worker->strategy, i, test_case);
        if(!has_case) break;

        /* Restart the listener with the fuzzed PUPI. */
        nfc_listener_stop(listener);
        nfc_listener_free(listener);
        iso14443_3b_free(nfc_data);

        nfc_data = iso14443_3b_alloc();
        iso14443_3b_set_uid(nfc_data, test_case->data, test_case->data_len);
        listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3b, nfc_data);
        nfc_listener_start(listener, NULL, NULL);

        /* Brief exposure window per test case */
        uint32_t start_tick = furi_get_tick();
        while(!worker->stop_requested &&
              (furi_get_tick() - start_tick) < furi_ms_to_ticks(timeout_ms)) {
            furi_delay_ms(10);
        }

        /* Report progress */
        if(worker->callback) {
            worker->callback(
                NULL, i + 1, total, test_case->data, test_case->data_len, worker->cb_context);
        }

        if(delay_ms > 0) furi_delay_ms(delay_ms);
    }

    nfc_listener_stop(listener);
    nfc_listener_free(listener);
    iso14443_3b_free(nfc_data);
    free(test_case);
    nfc_free(nfc);
}

/* ───── FeliCa listener fuzz loop ───── */
/*
 * Each test case provides an 8-byte IDm (Manufacture ID).
 * The listener is restarted with each new IDm so the reader sees a
 * different FeliCa tag identity. Tests reader robustness for FeliCa /
 * ISO 18092 readers (transit gates, mobile payment terminals, etc.).
 */
static void nfc_fuzzer_worker_run_listener_felica(NfcFuzzerWorker* worker) {
    Nfc* nfc = nfc_alloc();
    furi_assert(nfc);

    NfcFuzzerTestCase* test_case = malloc(sizeof(NfcFuzzerTestCase));
    if(!test_case) {
        FURI_LOG_E(WORKER_TAG, "FeliCa: OOM allocating test buffer");
        nfc_free(nfc);
        return;
    }

    nfc_fuzzer_profile_init(worker->profile, worker->strategy);

    uint32_t max = nfc_fuzzer_max_cases(worker->settings.max_cases_index);
    uint32_t total = nfc_fuzzer_profile_total_cases(worker->profile, worker->strategy);
    if(total > max) total = max;

    uint32_t timeout_ms = nfc_fuzzer_timeout_ms(worker->settings.timeout_index);
    uint32_t delay_ms = nfc_fuzzer_delay_ms(worker->settings.delay_index);

    /* Build an initial FeliCa data block with a placeholder IDm. */
    uint8_t default_idm[8] = {0x01, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    FelicaData* felica_data = felica_alloc();
    felica_set_uid(felica_data, default_idm, sizeof(default_idm));

    NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolFelica, felica_data);
    nfc_listener_start(listener, NULL, NULL);

    for(uint32_t i = 0; i < total && !worker->stop_requested; i++) {
        bool has_case = nfc_fuzzer_profile_next(worker->profile, worker->strategy, i, test_case);
        if(!has_case) break;

        /* Restart the listener with the fuzzed IDm. */
        nfc_listener_stop(listener);
        nfc_listener_free(listener);
        felica_free(felica_data);

        felica_data = felica_alloc();
        felica_set_uid(felica_data, test_case->data, test_case->data_len);
        listener = nfc_listener_alloc(nfc, NfcProtocolFelica, felica_data);
        nfc_listener_start(listener, NULL, NULL);

        /* Brief exposure window per test case */
        uint32_t start_tick = furi_get_tick();
        while(!worker->stop_requested &&
              (furi_get_tick() - start_tick) < furi_ms_to_ticks(timeout_ms)) {
            furi_delay_ms(10);
        }

        /* Report progress */
        if(worker->callback) {
            worker->callback(
                NULL, i + 1, total, test_case->data, test_case->data_len, worker->cb_context);
        }

        if(delay_ms > 0) furi_delay_ms(delay_ms);
    }

    nfc_listener_stop(listener);
    nfc_listener_free(listener);
    felica_free(felica_data);
    free(test_case);
    nfc_free(nfc);
}

/* ───── Thread entry point ───── */

static int32_t nfc_fuzzer_worker_thread(void* context) {
    NfcFuzzerWorker* worker = context;
    furi_assert(worker);

    FURI_LOG_I(
        WORKER_TAG, "Worker started: profile=%d strategy=%d", worker->profile, worker->strategy);

    if(worker->profile == NfcFuzzerProfileNfcB) {
        nfc_fuzzer_worker_run_listener_nfcb(worker);
    } else if(worker->profile == NfcFuzzerProfileFelica) {
        nfc_fuzzer_worker_run_listener_felica(worker);
    } else if(
        worker->profile == NfcFuzzerProfileReaderCommands ||
        worker->profile == NfcFuzzerProfileMifareAuth ||
        worker->profile == NfcFuzzerProfileMifareRead || worker->profile == NfcFuzzerProfileRats) {
        nfc_fuzzer_worker_run_poller(worker);
    } else {
        nfc_fuzzer_worker_run_listener(worker);
    }

    worker->running = false;
    FURI_LOG_I(WORKER_TAG, "Worker finished");

    if(worker->done_callback) {
        worker->done_callback(worker->done_cb_context);
    }

    return 0;
}

/* ───── Public API ───── */

NfcFuzzerWorker* nfc_fuzzer_worker_alloc(void) {
    NfcFuzzerWorker* worker = malloc(sizeof(NfcFuzzerWorker));
    if(!worker) {
        FURI_LOG_E(WORKER_TAG, "Failed to allocate NfcFuzzerWorker");
        return NULL;
    }
    memset(worker, 0, sizeof(NfcFuzzerWorker));

    worker->thread = furi_thread_alloc_ex(
        WORKER_THREAD_NAME, WORKER_THREAD_STACK, nfc_fuzzer_worker_thread, worker);

    return worker;
}

void nfc_fuzzer_worker_free(NfcFuzzerWorker* worker) {
    furi_assert(worker);
    furi_assert(!worker->running);
    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);
    free(worker);
}

void nfc_fuzzer_worker_set_callback(
    NfcFuzzerWorker* worker,
    NfcFuzzerWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    worker->callback = callback;
    worker->cb_context = context;
}

void nfc_fuzzer_worker_set_done_callback(
    NfcFuzzerWorker* worker,
    NfcFuzzerWorkerDoneCallback callback,
    void* context) {
    furi_assert(worker);
    worker->done_callback = callback;
    worker->done_cb_context = context;
}

void nfc_fuzzer_worker_start(
    NfcFuzzerWorker* worker,
    NfcFuzzerProfile profile,
    NfcFuzzerStrategy strategy,
    const NfcFuzzerSettings* settings) {
    furi_assert(worker);
    furi_assert(!worker->running);

    furi_thread_join(worker->thread);

    worker->profile = profile;
    worker->strategy = strategy;
    worker->settings = *settings;
    worker->stop_requested = false;
    worker->running = true;

    furi_thread_start(worker->thread);
}

void nfc_fuzzer_worker_stop(NfcFuzzerWorker* worker) {
    furi_assert(worker);
    worker->stop_requested = true;
    if(worker->running) {
        furi_thread_join(worker->thread);
    }
    worker->running = false;
}

bool nfc_fuzzer_worker_is_running(NfcFuzzerWorker* worker) {
    furi_assert(worker);
    return worker->running;
}
