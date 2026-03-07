#include "spectrum_worker.h"
#include <furi_hal.h>

#define TAG "SpectrumWorker"

struct SpectrumWorker {
    FuriThread* thread;
    volatile bool running;
    uint32_t freq_start;
    uint32_t freq_end;
    uint32_t step_hz;
    SpectrumWorkerCallback callback;
    void* callback_context;
};

/** Perform one RSSI sweep across the configured frequency range */
static void spectrum_worker_sweep(SpectrumWorker* worker, SpectrumData* data) {
    uint32_t freq = worker->freq_start;
    uint8_t bin = 0;
    data->max_rssi = SPECTRUM_RSSI_MIN;
    data->max_rssi_freq = worker->freq_start;
    data->freq_start = worker->freq_start;
    data->freq_step = worker->step_hz;

    while(freq <= worker->freq_end && bin < SPECTRUM_NUM_BINS && worker->running) {
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();

        /* Brief settling time for the CC1101 RSSI to stabilize */
        furi_delay_us(400);

        float rssi = furi_hal_subghz_get_rssi();

        data->rssi[bin] = rssi;
        if(rssi > data->max_rssi) {
            data->max_rssi = rssi;
            data->max_rssi_freq = freq;
        }

        freq += worker->step_hz;
        bin++;
    }

    furi_hal_subghz_idle();
    data->num_bins = bin;
}

static int32_t spectrum_worker_thread(void* context) {
    SpectrumWorker* worker = context;
    SpectrumData* data = malloc(sizeof(SpectrumData));
    if(!data) {
        FURI_LOG_E(TAG, "Failed to allocate SpectrumData");
        return -1;
    }
    memset(data, 0, sizeof(SpectrumData));
    for(uint8_t i = 0; i < SPECTRUM_NUM_BINS; i++) {
        data->peak_rssi[i] = -200.0f;
    }

    FURI_LOG_I(
        TAG,
        "Worker started: %lu - %lu, step %lu Hz",
        worker->freq_start,
        worker->freq_end,
        worker->step_hz);

    /* Initialize the sub-ghz radio */
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    /* Load a simple OOK preset for RSSI-only scanning.
     * We just need the radio in RX mode to read RSSI — no demodulation needed.
     * Use a wide-bandwidth configuration for spectrum scanning. */
    const uint8_t spectrum_preset[] = {
        /* CC1101 register settings for wideband RX */
        0x02,
        0x0D, /* IOCFG0: GDO0 serial clock */
        0x08,
        0x32, /* FIFOTHR */
        0x0B,
        0x06, /* FSCTRL1: IF frequency */
        0x10,
        0xB5, /* MDMCFG4: channel BW ~232 kHz */
        0x11,
        0x43, /* MDMCFG3: data rate */
        0x12,
        0x30, /* MDMCFG2: OOK, no sync */
        0x15,
        0x04, /* DEVIATN */
        0x18,
        0x18, /* MCSM0: autocal from idle */
        0x19,
        0x1D, /* FOCCFG */
        0x1B,
        0x43, /* AGCCTRL2 */
        0x1C,
        0x40, /* AGCCTRL1 */
        0x1D,
        0x91, /* AGCCTRL0 */
        0x00,
        0x00, /* End of register list */
        /* PATABLE (8 bytes) */
        0x00,
        0xC0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    furi_hal_subghz_load_custom_preset(spectrum_preset);

    while(worker->running) {
        spectrum_worker_sweep(worker, data);

        if(worker->callback && worker->running) {
            worker->callback(data, worker->callback_context);
        }

        /* Yield to let the UI thread run */
        furi_delay_ms(1);
    }

    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();

    free(data);
    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

SpectrumWorker* spectrum_worker_alloc(void) {
    SpectrumWorker* worker = malloc(sizeof(SpectrumWorker));
    worker->thread = furi_thread_alloc_ex(TAG, 4096, spectrum_worker_thread, worker);
    worker->running = false;
    worker->callback = NULL;
    worker->callback_context = NULL;
    return worker;
}

void spectrum_worker_free(SpectrumWorker* worker) {
    furi_assert(worker);
    furi_thread_free(worker->thread);
    free(worker);
}

void spectrum_worker_start(
    SpectrumWorker* worker,
    uint32_t freq_start,
    uint32_t freq_end,
    uint32_t step_khz) {
    furi_assert(worker);
    furi_assert(!worker->running);

    worker->freq_start = freq_start;
    worker->freq_end = freq_end;
    worker->step_hz = step_khz * 1000;
    worker->running = true;

    furi_thread_start(worker->thread);
}

void spectrum_worker_stop(SpectrumWorker* worker) {
    furi_assert(worker);
    worker->running = false;
    furi_thread_join(worker->thread);
}

bool spectrum_worker_is_running(SpectrumWorker* worker) {
    furi_assert(worker);
    return worker->running;
}

void spectrum_worker_set_callback(
    SpectrumWorker* worker,
    SpectrumWorkerCallback callback,
    void* context) {
    furi_assert(worker);
    worker->callback = callback;
    worker->callback_context = context;
}
