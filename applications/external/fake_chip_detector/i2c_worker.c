#include "i2c_worker.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>
#include <furi_hal_resources.h>

#define TAG "I2CWorker"

#define WORKER_FLAG_SCAN  (1UL << 0)
#define WORKER_FLAG_EXIT  (1UL << 1)
#define WORKER_FLAG_WATCH (1UL << 3)
#define WORKER_FLAG_PAD   (1UL << 4)
#define WORKER_FLAG_FIX   (1UL << 5)
#define WORKER_FLAG_ALL \
    (WORKER_FLAG_SCAN | WORKER_FLAG_EXIT | WORKER_FLAG_WATCH | WORKER_FLAG_PAD | WORKER_FLAG_FIX)

// How many agreeing reads it takes to move the pad meter. At 50ms a read that
// is a fifth of a second of steadiness, which is short enough to feel live and
// long enough that a hand near a floating pad does not make it flicker.
#define PAD_SETTLE_READS 4

// How long to spend trying to cut the rail from software before asking the
// person to do it. Measured on a Flipper Zero running Unleashed 0.90 with the
// bench on USB: furi_hal_power_disable_external_3_3v() does not de-power header
// pin 9 at all. The module's pull-up survived a 800ms outage sampled every
// 20ms, a 512-byte SD sector read succeeded 300ms in, and a VL6180X parked at a
// non-default address by register 0x0212 -- a value that cannot survive a
// power-on reset -- kept it. Re-issuing the call every 20ms changed nothing.
// The attempt stays because that was one device on one firmware, but nothing
// downstream may assume it worked: the lines have to be seen falling.
#define RAIL_TRY_MS   300
#define POWER_BOOT_MS 650

// Agreeing reads before the strap is called applied or lost. The pull is weak
// and the pad may be capacitive, so give it a moment before judging.
#define STRAP_SETTLE_READS 3

struct I2CWorker {
    FuriThread* thread;
    FuriMutex* mutex;
    I2CWorkerCallback callback;
    void* callback_context;
    volatile bool busy;
    volatile bool watch_stop;
    volatile bool pad_stop;
    volatile bool fix_stop;
    volatile bool scan_abort;
    volatile uint8_t pad_level; // I2CPadLevel
    volatile uint8_t fix_stage; // I2CFixStage
    volatile uint32_t fix_gen; // bumped per run so an abandoned one cannot resume
    volatile bool fix_want_high; // level the pad has to sit at for I2C
    volatile uint32_t probe_timeout_ms;
    volatile uint8_t progress_addr;
    I2CFoundDevice found[I2C_SCAN_MAX_FOUND];
    size_t found_count;
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

// Pins the user might plug the sensor into by mistake. Deliberately excludes
// 13/14 (UART), 10/12 (SWD) and 17 (iButton has its own pull-up on board),
// where a reading would be meaningless or disruptive.
typedef struct {
    const GpioPin* pin;
    uint8_t number;
} StrayCandidate;

static const StrayCandidate stray_candidates[] = {
    {&gpio_ext_pa7, 2},
    {&gpio_ext_pa6, 3},
    {&gpio_ext_pa4, 4},
    {&gpio_ext_pb3, 5},
    {&gpio_ext_pb2, 6},
    {&gpio_ext_pc3, 7},
};

// A module's pull-ups sit on whichever pins it is wired to. If they show up
// somewhere other than 15/16, the user is on the wrong pins.
static uint8_t i2c_find_stray_pullup(void) {
    for(size_t i = 0; i < COUNT_OF(stray_candidates); i++) {
        const GpioPin* pin = stray_candidates[i].pin;
        furi_hal_gpio_init(pin, GpioModeInput, GpioPullDown, GpioSpeedLow);
        furi_delay_ms(2);
        bool pulled_up = furi_hal_gpio_read(pin);
        furi_hal_gpio_init(pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        if(pulled_up) return stray_candidates[i].number;
    }
    return 0;
}

// Drives SCL low and watches SDA. A lone clock pulse is harmless to any
// slave, while a short between the two lines makes SDA follow.
static bool i2c_lines_shorted(void) {
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pc0, false);
    furi_delay_ms(2);
    bool sda_followed = !furi_hal_gpio_read(&gpio_ext_pc1);

    furi_hal_gpio_write(&gpio_ext_pc0, true);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    return sda_followed;
}

void i2c_worker_check_bus(I2CBusCheck* out) {
    memset(out, 0, sizeof(*out));

    bool scl_stuck = false, sda_stuck = false;
    // PC0 is SCL and PC1 is SDA per furi_hal_i2c_config.h; on the header
    // PC0 is pin 16 and PC1 is pin 15 (furi_hal_resources.c gpio_pins[]).
    out->scl_ok = i2c_line_probe(&gpio_ext_pc0, &scl_stuck);
    out->sda_ok = i2c_line_probe(&gpio_ext_pc1, &sda_stuck);
    out->scl_stuck = scl_stuck;
    out->sda_stuck = sda_stuck;

    // A pull-up reads high only if the module's supply is live *and* shares
    // our ground reference, so one pulled-up line already proves both. That
    // makes the power rows meaningful while the user is still part-way
    // through wiring the bus up.
    out->powered = out->scl_ok || out->sda_ok;

    if(scl_stuck || sda_stuck) {
        out->health = I2CBusStuckLow;
    } else if(out->scl_ok && out->sda_ok) {
        out->health = I2CBusOk; // both lines are needed before I2C can work
        out->shorted = i2c_lines_shorted();
    } else {
        out->health = I2CBusFloating;
        // Any incomplete bus is worth checking, not just a completely dead
        // one: with SDA on the right pin and SCL on the wrong one, the old
        // "both lines dead" condition never fired and the user got no hint.
        out->stray_pin = i2c_find_stray_pullup();
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

bool i2c_worker_read_reg16_addr(
    uint8_t addr7,
    uint16_t reg,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms) {
    const uint8_t index[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_trx(
        &furi_hal_i2c_handle_external,
        (uint8_t)(addr7 << 1),
        index,
        sizeof(index),
        data,
        len,
        timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool i2c_worker_write_reg16_addr(uint8_t addr7, uint16_t reg, uint8_t value, uint32_t timeout_ms) {
    const uint8_t frame[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), frame, sizeof(frame), timeout_ms);
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

bool i2c_worker_write_raw(uint8_t addr7, const uint8_t* data, size_t len, uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), data, len, timeout_ms);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool i2c_worker_read_raw(uint8_t addr7, uint8_t* data, size_t len, uint32_t timeout_ms) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_rx(
        &furi_hal_i2c_handle_external, (uint8_t)(addr7 << 1), data, len, timeout_ms);
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

// The pad meter. Deliberately the same measurement the bus check already
// makes, pointed at whatever the user has walked the pin-15 wire onto: the
// internal pulls are ~40k, so anything actually driving the pad or tying it to
// a rail wins, and a pad connected to nothing shows as floating.
static I2CPadLevel i2c_pad_read(void) {
    bool stuck_low = false;
    bool high_against_pulldown = i2c_line_probe(&gpio_ext_pc1, &stuck_low);
    if(stuck_low) return I2CPadLow;
    if(high_against_pulldown) return I2CPadHigh;
    return I2CPadFloating;
}

static void i2c_worker_do_pad_watch(I2CWorker* worker) {
    I2CPadLevel shown = (I2CPadLevel)worker->pad_level;
    I2CPadLevel candidate = shown;
    uint8_t agreed = 0;

    while(!worker->pad_stop) {
        I2CPadLevel now = i2c_pad_read();
        if(now == candidate) {
            if(agreed < PAD_SETTLE_READS) agreed++;
        } else {
            candidate = now;
            agreed = 1;
        }
        if(agreed >= PAD_SETTLE_READS && candidate != shown) {
            shown = candidate;
            worker->pad_level = (uint8_t)shown;
            i2c_worker_notify(worker, I2CWorkerEventPadUpdate);
        }
        furi_delay_ms(50);
    }
}

/* ---- the fix run ---- */

// Is the run that owns this generation still the one the screen is waiting on?
//
// fix_stop alone is not enough. Leaving the screen sets it, but starting a new
// run clears it again, and a run can be up to POWER_BOOT_MS deep in a delay it
// cannot check anything from. Back then OK inside that window would clear the
// stop out from under the old run, which would wake up believing it was still
// current and walk the user through the tail of a fix they had abandoned -
// including the automatic rescan at the end of it. The generation only ever
// moves forward, so an old run can never mistake itself for the new one.
static bool fix_current(I2CWorker* worker, uint32_t gen) {
    return !worker->fix_stop && worker->fix_gen == gen;
}

static void fix_set(I2CWorker* worker, uint32_t gen, I2CFixStage stage) {
    // Silent once the screen has walked away. Several steps here cannot be
    // interrupted mid-measurement, so a stop can arrive while one is still
    // finishing; announcing the next stage afterwards would drag the user back
    // into a screen they just left, and the last of those stages would kick off
    // a rescan nobody asked for.
    if(!fix_current(worker, gen)) return;
    worker->fix_stage = (uint8_t)stage;
    i2c_worker_notify(worker, I2CWorkerEventFixUpdate);
}

// The strap. Weak on purpose: the internal pull is ~40k, which is enough to
// hold a high-impedance mode input but cannot fight a driver, so if the wire
// turns out to be sitting on an output the two never argue. It also cannot
// back-feed enough current through an unpowered chip's protection diode to keep
// the part alive across the power cycle that follows, which a push-pull output
// on the same pin could.
static void fix_strap_apply(bool want_high) {
    furi_hal_gpio_init(
        &gpio_ext_pc1, GpioModeInput, want_high ? GpioPullUp : GpioPullDown, GpioSpeedLow);
}

static void fix_strap_release(void) {
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

// Reading the pin back is the whole point. A pad tied the wrong way in copper
// wins against the internal pull, and saying so is worth far more than a fix
// that quietly does nothing: it tells the buyer the board decided this, not
// them.
static bool fix_strap_took(bool want_high) {
    furi_delay_ms(20);
    for(uint8_t i = 0; i < STRAP_SETTLE_READS; i++) {
        if(furi_hal_gpio_read(&gpio_ext_pc1) != want_high) return false;
        furi_delay_ms(10);
    }
    return true;
}

// SCL only. Pin 15 is holding the strap, so the module's other pull-up is the
// one thing left that still reports whether it has power.
static bool fix_module_powered(void) {
    bool stuck = false;
    return i2c_line_probe(&gpio_ext_pc0, &stuck);
}

// Waits for the module's power to go the way asked. Returns false if the view
// went away, or was replaced by a newer run, while waiting.
static bool fix_wait_power(I2CWorker* worker, uint32_t gen, bool want_powered) {
    while(fix_current(worker, gen)) {
        if(fix_module_powered() == want_powered) return true;
        furi_delay_ms(50);
    }
    return false;
}

// Strap the pad, get the part restarted so the strap is actually sampled, then
// get the wire back on the bus. Every step ends on a measurement rather than a
// delay, and nothing here claims anything the app has not seen: if the rail
// cannot be cut from software, the person is asked, and the app waits until the
// pull-ups really do disappear.
static void i2c_worker_do_fix(I2CWorker* worker) {
    const uint32_t gen = worker->fix_gen;
    bool want_high = worker->fix_want_high;

    fix_set(worker, gen, I2CFixStrapping);
    fix_strap_apply(want_high);
    if(!fix_strap_took(want_high)) {
        fix_strap_release();
        fix_set(worker, gen, I2CFixPadHeld); // terminal: the screen offers the report
        return;
    }

    // One quiet attempt at cutting the rail ourselves, so a device where that
    // works never bothers its owner. Only counts if the module was powered to
    // begin with and stopped being powered: a sensor that arrived here already
    // dark would otherwise look like proof the cut worked, and the app would
    // skip the one step that actually restarts the part.
    bool powered_before = fix_module_powered();
    bool fell = false;
    if(powered_before) {
        furi_hal_power_disable_external_3_3v();
        for(uint32_t waited = 0; waited < RAIL_TRY_MS && fix_current(worker, gen); waited += 20) {
            if(!fix_module_powered()) {
                fell = true;
                break;
            }
            furi_delay_ms(20);
        }
        furi_hal_power_enable_external_3_3v();
    }

    if(!fell) {
        if(powered_before) {
            fix_set(worker, gen, I2CFixWantPowerOff);
            if(!fix_wait_power(worker, gen, false)) {
                fix_strap_release();
                return;
            }
        }
        fix_set(worker, gen, I2CFixWantPowerOn);
        if(!fix_wait_power(worker, gen, true)) {
            fix_strap_release();
            return;
        }
    }
    furi_delay_ms(POWER_BOOT_MS);

    // The strap has done its job the moment the part came up holding it: a mode
    // pin is sampled at reset and kept. Let go before asking for the wire back,
    // or the pull would fight the bus.
    fix_strap_release();
    fix_set(worker, gen, I2CFixWantWire);
    while(fix_current(worker, gen)) {
        I2CBusCheck bus;
        i2c_worker_check_bus(&bus);
        if(bus.health == I2CBusOk) {
            fix_set(worker, gen, I2CFixDone);
            return;
        }
        furi_delay_ms(100);
    }
}

static int32_t i2c_worker_thread(void* context) {
    I2CWorker* worker = context;
    for(;;) {
        uint32_t flags = furi_thread_flags_wait(WORKER_FLAG_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(flags & WORKER_FLAG_EXIT) break;
        if(flags & WORKER_FLAG_SCAN) {
            worker->busy = true;
            i2c_worker_do_scan(worker);
            worker->busy = false;
            i2c_worker_notify(worker, I2CWorkerEventScanDone);
        }
        if(flags & WORKER_FLAG_WATCH) {
            worker->busy = true;
            i2c_worker_do_watch(worker);
            worker->busy = false;
        }
        if(flags & WORKER_FLAG_PAD) {
            worker->busy = true;
            i2c_worker_do_pad_watch(worker);
            worker->busy = false;
        }
        if(flags & WORKER_FLAG_FIX) {
            worker->busy = true;
            i2c_worker_do_fix(worker);
            worker->busy = false;
        }
    }
    // Never leave the bench dark, and never leave a pull hanging on the bus:
    // whatever the app was doing, the sensor gets its rail and its line back
    // before this thread stops answering.
    fix_strap_release();
    furi_hal_power_enable_external_3_3v();
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
    worker->watch_stop = true;
    worker->scan_abort = false;
    worker->probe_timeout_ms = I2C_PROBE_TIMEOUT_MS;
    worker->progress_addr = I2C_SCAN_ADDR_FIRST;
    worker->found_count = 0;
    worker->thread = furi_thread_alloc_ex("FakeChipWorker", 1024, i2c_worker_thread, worker);
    furi_thread_start(worker->thread);
    return worker;
}

void i2c_worker_free(I2CWorker* worker) {
    // Break any long-running loop before asking the thread to exit
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
    // watch mode is still winding down runs as soon as that job ends. Guarding
    // on `busy` here would silently drop the request.
    worker->probe_timeout_ms = probe_timeout_ms;
    worker->scan_abort = false;
    worker->watch_stop = true; // ask any running watch loop to yield
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_SCAN);
}

void i2c_worker_abort_scan(I2CWorker* worker) {
    worker->scan_abort = true;
}

bool i2c_worker_is_busy(I2CWorker* worker) {
    return worker->busy;
}

void i2c_worker_watch_start(I2CWorker* worker) {
    worker->watch_stop = false;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_WATCH);
}

void i2c_worker_watch_stop(I2CWorker* worker) {
    worker->watch_stop = true;
}

void i2c_worker_pad_watch_start(I2CWorker* worker) {
    worker->pad_stop = false;
    // Start from nothing rather than from last time's answer. The wire has very
    // likely been moved to a different pad since, and showing the old level
    // while the new one settles is showing a measurement of the wrong pin.
    worker->pad_level = I2CPadUnknown;
    worker->watch_stop = true; // the bus watcher and the pad meter share pin 15
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_PAD);
}

void i2c_worker_pad_watch_stop(I2CWorker* worker) {
    worker->pad_stop = true;
}

I2CPadLevel i2c_worker_get_pad(I2CWorker* worker) {
    return (I2CPadLevel)worker->pad_level;
}

void i2c_worker_fix_start(I2CWorker* worker, bool want_high) {
    worker->watch_stop = true; // both of those own pin 15, and the strap needs it
    worker->pad_stop = true;
    // Bump the generation before clearing the stop, so a run still unwinding
    // from the last stop is already out of date by the time the stop it was
    // watching goes away.
    worker->fix_gen++;
    worker->fix_stop = false;
    worker->fix_want_high = want_high;
    worker->fix_stage = I2CFixStrapping;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), WORKER_FLAG_FIX);
}

void i2c_worker_fix_stop(I2CWorker* worker) {
    worker->fix_stop = true;
}

I2CFixStage i2c_worker_get_fix_stage(I2CWorker* worker) {
    return (I2CFixStage)worker->fix_stage;
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
