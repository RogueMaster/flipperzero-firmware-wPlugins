#include "dallas_test_worker.h"

#include <furi.h>
#include <furi_hal.h> // DWT->CYCCNT, FURI_CRITICAL_ENTER/EXIT
#include <furi_hal_ibutton.h>
#include <furi_hal_resources.h> // gpio_ibutton
#include <furi_hal_cortex.h> // instructions_per_microsecond

#include <one_wire/one_wire_host.h>
#include <one_wire/maxim_crc.h>

#include <power/power_service/power.h> // OTG (5V pin) control for the 1-Wire pull-up rail

#include <math.h>
#include <string.h>

#define TAG "DallasTester"

// --- Measurement tuning ----------------------------------------------------
#define DALLAS_TEST_SAMPLES      24 // timed resets per batch (median is taken)
#define DALLAS_RESET_LOW_US      500 // master reset pulse (>= 480us tRSTL)
#define DALLAS_EDGE_TIMEOUT_US   400 // max wait per edge (presence window <= 240us)
#define DALLAS_SETTLE_US         5 // idle-high settle before driving a reset
#define DALLAS_INTERSAMPLE_US    1000 // gap between resets: bus recovery + lets BLE/USB run
#define DALLAS_BATCH_INTERVAL_MS 120 // refresh period of the live reading
#define DALLAS_OTG_SETTLE_MS     20 // conservative headroom for the 5V (OTG) rail to ramp up

// Plausible tPDL window. Anything outside is a glitch / contact bounce and is dropped.
#define DALLAS_TPDL_FLOOR_US 20
#define DALLAS_TPDL_CEIL_US  320

#define DALLAS_CMD_READ_ROM 0x33 // Read ROM, for the family code / CRC

struct DallasTestWorker {
    FuriThread* thread;
    OneWireHost* host;
    Power* power; // held for the worker's lifetime; used to power the 5V (OTG) rail while testing
    DallasTestResultCallback callback;
    void* callback_ctx;
    volatile bool running;
};

typedef enum {
    DallasSampleOk, // valid presence pulse captured
    DallasSampleNoPresence, // bus recovered high but the slave never pulled low (no key)
    DallasSampleBusStuck, // bus never recovered after release, or stuck low past timeout
} DallasSampleStatus;

typedef struct {
    DallasSampleStatus status;
    uint16_t tpdl_us;
    uint16_t tpdh_us;
} DallasSample;

// One reset + presence capture, timed with the DWT cycle counter (15.6 ns/tick @ 64 MHz).
// IRQs are masked for up to ~1.7 ms (settle + 500us reset + 3x400us edge timeouts; ~0.7 ms
// typical with a key present) so the RTOS tick / USB / BLE-M4 interrupts can't stretch a
// reading. Mirrors the DWT timing technique the firmware's 1-Wire slave uses.
static DallasSample dallas_measure_once(const GpioPin* pin, uint32_t ipus) {
    DallasSample s = {.status = DallasSampleNoPresence};
    const uint32_t timeout = DALLAS_EDGE_TIMEOUT_US * ipus;

    FURI_CRITICAL_ENTER();

    furi_hal_gpio_write(pin, true);
    furi_hal_cortex_delay_us(DALLAS_SETTLE_US);
    furi_hal_gpio_write(pin, false);
    furi_hal_cortex_delay_us(DALLAS_RESET_LOW_US);

    // Release the bus; the pull-up brings it high (t_rel marks our release).
    const uint32_t t_rel = DWT->CYCCNT;
    furi_hal_gpio_write(pin, true);

    // Phase A: wait for the RC rise until the bus is high again (bounded by timeout).
    while(!furi_hal_gpio_read(pin) && (DWT->CYCCNT - t_rel) < timeout) {
    }
    if(!furi_hal_gpio_read(pin)) {
        s.status = DallasSampleBusStuck; // bus never recovered (stuck low / shorted contacts)
        FURI_CRITICAL_EXIT();
        return s;
    }

    // Phase B: wait for the slave to pull the line low = start of the presence pulse.
    const uint32_t t_high = DWT->CYCCNT;
    while(furi_hal_gpio_read(pin) && (DWT->CYCCNT - t_high) < timeout) {
    }
    if(furi_hal_gpio_read(pin)) {
        FURI_CRITICAL_EXIT();
        return s; // no presence pulse within the window -> no key present (status unchanged)
    }
    const uint32_t t_lo = DWT->CYCCNT;

    // Phase C: wait for the slave to release the line = end of the presence pulse.
    while(!furi_hal_gpio_read(pin) && (DWT->CYCCNT - t_lo) < timeout) {
    }
    const uint32_t t_hi = DWT->CYCCNT;
    const bool released = furi_hal_gpio_read(pin);

    FURI_CRITICAL_EXIT();

    if(!released) {
        s.status = DallasSampleBusStuck; // line stuck low past the timeout -> invalid
        return s;
    }

    s.status = DallasSampleOk;
    s.tpdh_us = (t_lo - t_rel) / ipus; // master release -> slave pull-low (incl. RC rise)
    s.tpdl_us = (t_hi - t_lo) / ipus; // the presence-detect-low time we care about
    return s;
}

static void dallas_insertion_sort_u16(uint16_t* v, uint8_t n) {
    for(uint8_t i = 1; i < n; i++) {
        const uint16_t key = v[i];
        int j = (int)i - 1;
        while(j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
}

static uint16_t dallas_stddev_u16(const uint16_t* v, uint8_t n) {
    if(n < 2) return 0;
    uint32_t sum = 0;
    for(uint8_t i = 0; i < n; i++)
        sum += v[i];
    const uint32_t mean = sum / n;
    uint32_t var = 0;
    for(uint8_t i = 0; i < n; i++) {
        const int32_t d = (int32_t)v[i] - (int32_t)mean;
        var += (uint32_t)(d * d);
    }
    var /= n;
    return (uint16_t)sqrtf((float)var);
}

// reset + Read ROM (0x33) + 8 bytes into rom. Returns true if the Maxim CRC8 is valid.
static bool dallas_read_rom(OneWireHost* host, uint8_t rom[8]) {
    if(!onewire_host_reset(host)) return false;
    onewire_host_write(host, DALLAS_CMD_READ_ROM);
    onewire_host_read_bytes(host, rom, 8);
    return maxim_crc8(rom, 7, MAXIM_CRC8_INIT) == rom[7];
}

static void dallas_run_batch(OneWireHost* host, DallasTestResult* result) {
    memset(result, 0, sizeof(*result));

    uint16_t tpdl[DALLAS_TEST_SAMPLES];
    uint16_t tpdh[DALLAS_TEST_SAMPLES];
    uint8_t n = 0;
    uint8_t stuck = 0;

    const uint32_t ipus = furi_hal_cortex_instructions_per_microsecond();
    for(uint8_t i = 0; i < DALLAS_TEST_SAMPLES; i++) {
        const DallasSample s = dallas_measure_once(&gpio_ibutton, ipus);
        if(s.status == DallasSampleOk && s.tpdl_us >= DALLAS_TPDL_FLOOR_US &&
           s.tpdl_us <= DALLAS_TPDL_CEIL_US) {
            tpdl[n] = s.tpdl_us;
            tpdh[n] = s.tpdh_us;
            n++;
        } else if(s.status == DallasSampleBusStuck) {
            stuck++;
        }
        furi_delay_us(DALLAS_INTERSAMPLE_US);
    }

    result->samples_ok = n;
    // No valid pulse but the bus was held low: a real fault (shorted/stuck contacts),
    // distinct from "no key present" - surface it instead of the idle prompt.
    result->bus_fault = (n == 0 && stuck > 0);

    if(n > 0) {
        dallas_insertion_sort_u16(tpdl, n);
        dallas_insertion_sort_u16(tpdh, n);
        result->tpdl_min_us = tpdl[0];
        result->tpdl_max_us = tpdl[n - 1];
        result->tpdl_us = tpdl[n / 2];
        result->tpdh_us = tpdh[n / 2];
        result->tpdl_jitter_us = dallas_stddev_u16(tpdl, n);

        result->rom_valid = dallas_read_rom(host, result->rom);

        FURI_LOG_D(
            TAG,
            "tPDL=%uus [%u-%u] sd%u tPDH=%uus n=%u FAM=%02X CRC=%s",
            result->tpdl_us,
            result->tpdl_min_us,
            result->tpdl_max_us,
            result->tpdl_jitter_us,
            result->tpdh_us,
            n,
            result->rom_valid ? result->rom[0] : 0,
            result->rom_valid ? "ok" : "bad");

        // Surface the diagnostically interesting faults at warning level (survive release builds).
        if(!result->rom_valid) {
            FURI_LOG_W(TAG, "key present but ROM read/CRC failed (n=%u)", n);
        }
        if(stuck > 0) {
            FURI_LOG_W(TAG, "intermittent bus-stuck: %u/%u resets", stuck, DALLAS_TEST_SAMPLES);
        }
    } else if(result->bus_fault) {
        FURI_LOG_W(
            TAG,
            "1-Wire bus stuck low (%u/%u resets) - shorted contacts?",
            stuck,
            DALLAS_TEST_SAMPLES);
    }
}

static int32_t dallas_test_worker_thread(void* ctx) {
    DallasTestWorker* worker = ctx;

    // We drive the 1-Wire line open-drain with no internal pull-up, so the bus only recovers high
    // via an external pull-up. Empirically that pull-up is powered from the switchable 5V (OTG)
    // rail (it's also why the stock iButton app enables OTG to read); with it off, every released
    // reset reads as a stuck-low bus ("Bus low"). Power the rail for the test, then restore the
    // prior OTG *request* - only switch it back off if we were the one that turned it on. This
    // also undoes the stock iButton Read force-disabling OTG on exit, which otherwise leaves the
    // bus unpulled when this test is opened right after a read.
    const bool otg_was_on = power_is_otg_enabled(worker->power);
    if(!otg_was_on) {
        power_enable_otg(worker->power, true);
        furi_delay_ms(DALLAS_OTG_SETTLE_MS); // let the boost ramp the rail before measuring
    }

    onewire_host_start(worker->host); // PB14 -> open-drain, idle high

    while(worker->running) {
        DallasTestResult result;
        dallas_run_batch(worker->host, &result);
        // Re-check running: a batch takes tens of ms, and stop() may have fired meanwhile.
        if(worker->running && worker->callback) worker->callback(&result, worker->callback_ctx);
        furi_delay_ms(DALLAS_BATCH_INTERVAL_MS);
    }

    onewire_host_stop(worker->host); // float the pin back to analog

    if(!otg_was_on) power_enable_otg(worker->power, false); // restore the rail we found
    return 0;
}

DallasTestWorker* dallas_test_worker_alloc(void) {
    DallasTestWorker* worker = malloc(sizeof(DallasTestWorker));
    worker->host = onewire_host_alloc(&gpio_ibutton);
    worker->power = furi_record_open(RECORD_POWER);
    worker->thread = NULL;
    worker->callback = NULL;
    worker->callback_ctx = NULL;
    worker->running = false;
    return worker;
}

void dallas_test_worker_free(DallasTestWorker* worker) {
    furi_check(worker);
    dallas_test_worker_stop(worker);
    furi_record_close(RECORD_POWER);
    onewire_host_free(worker->host);
    free(worker);
}

void dallas_test_worker_start(
    DallasTestWorker* worker,
    DallasTestResultCallback callback,
    void* context) {
    furi_check(worker);
    furi_check(!worker->running);

    worker->callback = callback;
    worker->callback_ctx = context;
    worker->running = true;

    worker->thread =
        furi_thread_alloc_ex("DallasTestWorker", 2048, dallas_test_worker_thread, worker);
    furi_thread_start(worker->thread);
}

void dallas_test_worker_stop(DallasTestWorker* worker) {
    furi_check(worker);
    if(!worker->running && worker->thread == NULL) return;

    worker->running = false;
    if(worker->thread) {
        furi_thread_join(worker->thread);
        furi_thread_free(worker->thread);
        worker->thread = NULL;
    }
}
