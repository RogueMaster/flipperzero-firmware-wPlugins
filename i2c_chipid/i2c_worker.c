#include "i2c_worker.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define TAG "I2CWorker"

#define WORKER_FLAG_SCAN (1UL << 0)
#define WORKER_FLAG_EXIT (1UL << 1)
#define WORKER_FLAG_LIVE (1UL << 2)
#define WORKER_FLAG_WATCH (1UL << 3)
#define WORKER_FLAG_ALL \
    (WORKER_FLAG_SCAN | WORKER_FLAG_EXIT | WORKER_FLAG_LIVE | WORKER_FLAG_WATCH)

// BNO055 registers
#define BNO055_REG_CHIP_ID 0x00
#define BNO055_REG_EUL_HEADING_LSB 0x1A
#define BNO055_REG_EUL_HEADING_MSB 0x1B
#define BNO055_REG_CALIB_STAT 0x35
#define BNO055_REG_OPR_MODE 0x3D
#define BNO055_CHIP_ID_VALUE 0xA0
#define BNO055_MODE_CONFIG 0x00
#define BNO055_MODE_NDOF 0x0C

struct I2CWorker {
    FuriThread* thread;
    FuriMutex* mutex;
    I2CWorkerCallback callback;
    void* callback_context;
    volatile bool busy;
    volatile bool live_stop;
    volatile bool watch_stop;
    volatile bool scan_abort;
    volatile uint32_t probe_timeout_ms;
    volatile uint8_t progress_addr;
    I2CFoundDevice found[I2C_SCAN_MAX_FOUND];
    size_t found_count;
    I2CLiveData live;
    I2CBusCheck bus;
};

/* ---- electrical sanity check ---- */

// Reads one line twice: once with the internal pull-down engaged, once with
// the internal pull-up. The internal resistors are ~40k, an I2C pull-up is
// 2.2k..10k, so an external pull-up wins the divider and the line still reads
// high against the pull-down. That tells floating (nothing connected) apart
// from a healthy idle-high bus, and both apart from a line shorted low.
static bool i2c_line_probe(const GpioPin* pin, bool* stuck_low) {
    furi_hal_gpio_init(pin, GpioModeInput, GpioPullDown, GpioSpeedLow);
    furi_delay_ms(2);
    bool high_with_pulldown = furi_hal_gpio_read(pin);

    furi_hal_gpio_init(pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_delay_ms(2);
    bool high_with_pullup = furi_hal_gpio_read(pin);

    // Leave the pin floating; furi_hal_i2c_acquire reconfigures it anyway.
    furi_hal_gpio_init(pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    *stuck_low = !high_with_pullup;
    return high_with_pulldown;
}

void i2c_worker_check_bus(I2CBusCheck* out) {
    bool scl_stuck = false, sda_stuck = false;
    // PC0 is SCL and PC1 is SDA per furi_hal_i2c_config.h; on the header
    // PC0 is pin 16 and PC1 is pin 15 (furi_hal_resources.c gpio_pins[]).
    out->scl_ok = i2c_line_probe(&gpio_ext_pc0, &scl_stuck);
    out->sda_ok = i2c_line_probe(&gpio_ext_pc1, &sda_stuck);
    out->scl_stuck = scl_stuck;
    out->sda_stuck = sda_stuck;

    if(scl_stuck || sda_stuck) {
        out->health = I2CBusStuckLow;
    } else if(out->scl_ok && out->sda_ok) {
        out->health = I2CBusOk;
    } else {
        out->health = I2CBusFloating;
    }
}

/* ---- single-shot bus ops: acquire/release always paired ---- */

bool i2c_worker_device_ready(uint8_t addr7, uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ready = furi_hal_i2c_is_device_ready(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ready;
}

bool i2c_worker_read_reg(uint8_t addr7, uint8_t reg, uint8_t* value, uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_read_reg_8(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), reg, value, timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool i2c_worker_read_mem(
    uint8_t addr7,
    uint8_t reg,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_read_mem(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), reg, data, len, timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool i2c_worker_write_reg(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_write_reg_8(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), reg, value, timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

/* ---- worker thread ---- */

static void i2c_worker_notify(I2CWorker* worker, I2CWorkerEvent event) {
    if(worker->callback) worker->callback(event, worker->callback_context);
}

static void i2c_worker_do_scan(I2CWorker* worker) {
    // Electrical state first: it explains an empty sweep far better than
    // "no devices found" on its own.
    I2CBusCheck check;
    i2c_worker_check_bus(&check);

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->found_count = 0;
    worker->bus = check;
    furi_mutex_release(worker->mutex);

    for(uint8_t addr = I2C_SCAN_ADDR_FIRST; addr <= I2C_SCAN_ADDR_LAST; addr++) {
        if(worker->scan_abort) break; // leaving the app must not wait for the sweep
        worker->progress_addr = addr;
        if(i2c_worker_device_ready(addr, worker->probe_timeout_ms)) {
            // Identify right away so the result list is complete when
            // the sweep finishes.
            I2CFoundDevice device;
            device.addr = addr;
            chip_db_identify(addr, &device.ident);

            furi_mutex_acquire(worker->mutex, FuriWaitForever);
            if(worker->found_count < I2C_SCAN_MAX_FOUND) {
                worker->found[worker->found_count++] = device;
            }
            furi_mutex_release(worker->mutex);
        }
        if((addr & 0x07) == 0) i2c_worker_notify(worker, I2CWorkerEventScanProgress);
    }
}

/* ---- BNO055 live test ---- */

static void i2c_worker_live_set(I2CWorker* worker, const I2CLiveData* data) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->live = *data;
    furi_mutex_release(worker->mutex);
    i2c_worker_notify(worker, I2CWorkerEventLiveUpdate);
}

// Sleeps in small chunks so a stop request is honored quickly
static void i2c_worker_live_delay(I2CWorker* worker, uint32_t ms) {
    while(ms && !worker->live_stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static bool i2c_worker_live_find(I2CWorker* worker, uint8_t* addr_out) {
    UNUSED(worker);
    const uint8_t addrs[] = {0x28, 0x29};
    for(size_t i = 0; i < sizeof(addrs); i++) {
        uint8_t chip_id = 0;
        if(i2c_worker_read_reg(addrs[i], BNO055_REG_CHIP_ID, &chip_id, I2C_REG_TIMEOUT_MS) &&
           chip_id == BNO055_CHIP_ID_VALUE) {
            *addr_out = addrs[i];
            return true;
        }
    }
    return false;
}

static void i2c_worker_do_live(I2CWorker* worker) {
    I2CLiveData data = {.status = I2CLiveStatusSearching, .addr = 0, .heading_raw = 0, .mag_cal = 0};

    while(!worker->live_stop) {
        // 1. Find a BNO055 (checks CHIP_ID, not just an ACK)
        data.status = I2CLiveStatusSearching;
        i2c_worker_live_set(worker, &data);
        if(!i2c_worker_live_find(worker, &data.addr)) {
            i2c_worker_live_delay(worker, 250);
            continue;
        }

        // 2. CONFIG then NDOF; the sensor needs time to start fusion
        data.status = I2CLiveStatusInit;
        i2c_worker_live_set(worker, &data);
        if(!i2c_worker_write_reg(
               data.addr, BNO055_REG_OPR_MODE, BNO055_MODE_CONFIG, I2C_REG_TIMEOUT_MS)) {
            continue;
        }
        i2c_worker_live_delay(worker, 30);
        if(worker->live_stop) break;
        if(!i2c_worker_write_reg(
               data.addr, BNO055_REG_OPR_MODE, BNO055_MODE_NDOF, I2C_REG_TIMEOUT_MS)) {
            continue;
        }
        i2c_worker_live_delay(worker, 700);

        // 3. Read heading and calibration until stopped or the sensor drops off
        uint8_t errors = 0;
        while(!worker->live_stop && errors < 3) {
            uint8_t heading[2] = {0}; // LSB, MSB in one transaction — no tearing
            uint8_t calib = 0;
            bool ok =
                i2c_worker_read_mem(
                    data.addr, BNO055_REG_EUL_HEADING_LSB, heading, 2, I2C_REG_TIMEOUT_MS) &&
                i2c_worker_read_reg(data.addr, BNO055_REG_CALIB_STAT, &calib, I2C_REG_TIMEOUT_MS);
            if(ok) {
                errors = 0;
                data.status = I2CLiveStatusRunning;
                data.heading_raw = (int16_t)(((uint16_t)heading[1] << 8) | heading[0]);
                data.mag_cal = calib & 0x03; // bits 1:0 = magnetometer level
                i2c_worker_live_set(worker, &data);
            } else {
                errors++;
            }
            i2c_worker_live_delay(worker, 100);
        }
        // Park the sensor back in CONFIG so it stops burning ~12 mA running
        // fusion after the user has walked away from the test.
        i2c_worker_write_reg(data.addr, BNO055_REG_OPR_MODE, BNO055_MODE_CONFIG, I2C_REG_TIMEOUT_MS);

        if(!worker->live_stop) {
            data.status = I2CLiveStatusLost;
            i2c_worker_live_set(worker, &data);
            i2c_worker_live_delay(worker, 500);
        }
    }
}

/* ---- bus watch ---- */

static void i2c_worker_do_watch(I2CWorker* worker) {
    while(!worker->watch_stop) {
        I2CBusCheck check;
        i2c_worker_check_bus(&check);

        furi_mutex_acquire(worker->mutex, FuriWaitForever);
        worker->bus = check;
        furi_mutex_release(worker->mutex);
        i2c_worker_notify(worker, I2CWorkerEventBusUpdate);

        for(uint8_t i = 0; i < 4 && !worker->watch_stop; i++) {
            furi_delay_ms(50);
        }
    }
}

static int32_t i2c_worker_thread(void* context) {
    I2CWorker* worker = context;
    for(;;) {
        uint32_t flags =
            furi_thread_flags_wait(WORKER_FLAG_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(flags & WORKER_FLAG_EXIT) break;
        if(flags & WORKER_FLAG_SCAN) {
            worker->busy = true;
            i2c_worker_do_scan(worker);
            worker->busy = false;
            i2c_worker_notify(worker, I2CWorkerEventScanDone);
        }
        if(flags & WORKER_FLAG_LIVE) {
            worker->busy = true;
            i2c_worker_do_live(worker);
            worker->busy = false;
        }
        if(flags & WORKER_FLAG_WATCH) {
            worker->busy = true;
            i2c_worker_do_watch(worker);
            worker->busy = false;
        }
    }
    return 0;
}

/* ---- public API ---- */

I2CWorker* i2c_worker_alloc(void) {
    I2CWorker* worker = malloc(sizeof(I2CWorker));
    // Zero everything before the thread starts so no getter can ever observe
    // uninitialized results.
    memset(worker, 0, sizeof(I2CWorker));
    worker->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->callback = NULL;
    worker->callback_context = NULL;
    worker->busy = false;
    worker->live_stop = true;
    worker->watch_stop = true;
    worker->scan_abort = false;
    worker->probe_timeout_ms = I2C_PROBE_TIMEOUT_MS;
    worker->progress_addr = I2C_SCAN_ADDR_FIRST;
    worker->found_count = 0;
    worker->thread = furi_thread_alloc_ex("I2CChipIdWorker", 1024, i2c_worker_thread, worker);
    furi_thread_start(worker->thread);
    return worker;
}

void i2c_worker_free(I2CWorker* worker) {
    // Break any long-running loop before asking the thread to exit
    worker->live_stop = true;
    worker->watch_stop = true;
    worker->scan_abort = true;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_EXIT);
    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);
    furi_mutex_free(worker->mutex);
    free(worker);
}

void i2c_worker_set_callback(I2CWorker* worker, I2CWorkerCallback callback, void* context) {
    worker->callback = callback;
    worker->callback_context = context;
}

void i2c_worker_start_scan(I2CWorker* worker, uint32_t probe_timeout_ms) {
    // No busy check: the thread flag stays pending, so a scan requested while
    // watch or live mode is still winding down runs as soon as that job ends.
    // Guarding on `busy` here would silently drop the request.
    worker->probe_timeout_ms = probe_timeout_ms;
    worker->scan_abort = false;
    worker->watch_stop = true; // ask any running watch loop to yield
    worker->live_stop = true;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_SCAN);
}

void i2c_worker_abort_scan(I2CWorker* worker) {
    worker->scan_abort = true;
}

bool i2c_worker_is_busy(I2CWorker* worker) {
    return worker->busy;
}

void i2c_worker_live_start(I2CWorker* worker) {
    // No busy guard: if a scan is still running, the flag stays pending and
    // the live loop starts as soon as the scan finishes.
    worker->live_stop = false;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->live = (I2CLiveData){.status = I2CLiveStatusSearching};
    furi_mutex_release(worker->mutex);
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_LIVE);
}

void i2c_worker_live_stop(I2CWorker* worker) {
    worker->live_stop = true;
}

void i2c_worker_get_live(I2CWorker* worker, I2CLiveData* out) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    *out = worker->live;
    furi_mutex_release(worker->mutex);
}

void i2c_worker_watch_start(I2CWorker* worker) {
    worker->watch_stop = false;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_WATCH);
}

void i2c_worker_watch_stop(I2CWorker* worker) {
    worker->watch_stop = true;
}

void i2c_worker_get_bus(I2CWorker* worker, I2CBusCheck* out) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    *out = worker->bus;
    furi_mutex_release(worker->mutex);
}

uint8_t i2c_worker_get_progress(I2CWorker* worker) {
    return worker->progress_addr;
}

size_t i2c_worker_get_found(I2CWorker* worker, I2CFoundDevice* out, size_t max_count) {
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    size_t count = worker->found_count;
    if(count > max_count) count = max_count;
    memcpy(out, worker->found, count * sizeof(I2CFoundDevice));
    furi_mutex_release(worker->mutex);
    return count;
}
