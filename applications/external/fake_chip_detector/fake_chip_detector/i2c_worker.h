#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define I2C_SCAN_ADDR_FIRST 0x08
#define I2C_SCAN_ADDR_LAST  0x77
// A real bus rarely carries more than a handful of devices; each slot costs
// ~40 bytes in both the worker and the view model, so keep the cap modest.
#define I2C_SCAN_MAX_FOUND  16

// Timeout for a single address probe. Missing devices NACK immediately,
// the timeout only bounds a stuck bus, so a full sweep stays fast.
#define I2C_PROBE_TIMEOUT_MS 10
#define I2C_REG_TIMEOUT_MS   50

#include "chip_db.h"

typedef struct {
    uint8_t addr; // 7-bit
    ChipIdentification ident;
} I2CFoundDevice;

typedef struct I2CWorker I2CWorker;

typedef enum {
    I2CWorkerEventScanProgress,
    I2CWorkerEventScanDone,
    I2CWorkerEventBusUpdate,
} I2CWorkerEvent;

// Result of the electrical sanity check performed before a scan.
typedef enum {
    I2CBusOk, // both lines idle high — pull-ups present
    I2CBusFloating, // no pull-ups seen: sensor not connected or not powered
    I2CBusStuckLow, // a line is held low: short to GND or a hung device
} I2CBusHealth;

typedef struct {
    I2CBusHealth health;
    bool scl_ok; // pull-up detected on SCL
    bool sda_ok;
    bool scl_stuck; // line held low even against the internal pull-up
    bool sda_stuck;
    bool shorted; // SDA follows SCL: the two lines are tied together
    bool powered; // both lines pulled up => the module has 3V3 and GND
    uint8_t stray_pin; // header pin number carrying a stray pull-up, 0 = none
} I2CBusCheck;

// Probes the electrical state of SCL/SDA without touching the I2C peripheral.
// Blocks for a few ms and must not be called from a timer callback; the
// worker thread owns it in watch mode. Safe only while the bus is released.
void i2c_worker_check_bus(I2CBusCheck* out);

typedef void (*I2CWorkerCallback)(I2CWorkerEvent event, void* context);

I2CWorker* i2c_worker_alloc(void);
void i2c_worker_free(I2CWorker* worker);
void i2c_worker_set_callback(I2CWorker* worker, I2CWorkerCallback callback, void* context);

// probe_timeout_ms bounds each address probe during the sweep.
void i2c_worker_start_scan(I2CWorker* worker, uint32_t probe_timeout_ms);
void i2c_worker_abort_scan(I2CWorker* worker);
bool i2c_worker_is_busy(I2CWorker* worker);

// Watch mode: polls bus health in the worker thread so the wiring screen can
// react the moment the user plugs the sensor in. Emits I2CWorkerEventBusUpdate.
void i2c_worker_watch_start(I2CWorker* worker);
void i2c_worker_watch_stop(I2CWorker* worker);
void i2c_worker_get_bus(I2CWorker* worker, I2CBusCheck* out);
uint8_t i2c_worker_get_progress(I2CWorker* worker);
size_t i2c_worker_get_found(I2CWorker* worker, I2CFoundDevice* out, size_t max_count);

// Single-shot bus operations. Each call acquires the external bus handle and
// releases it before returning on every path — the handle cannot leak.
// All addresses are 7-bit; the <<1 shift the HAL expects happens inside.
bool i2c_worker_device_ready(uint8_t addr7, uint32_t timeout_ms);
bool i2c_worker_read_reg(uint8_t addr7, uint8_t reg, uint8_t* value, uint32_t timeout_ms);
bool i2c_worker_read_mem(uint8_t addr7, uint8_t reg, uint8_t* data, size_t len, uint32_t timeout_ms);

// Reads from a device that addresses its registers with a 16-bit index sent
// big-endian (ST time-of-flight parts, Goodix touch controllers). Writes the
// index, then reads back without releasing the bus.
bool i2c_worker_read_reg16_addr(
    uint8_t addr7,
    uint16_t reg,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms);
bool i2c_worker_write_reg16_addr(uint8_t addr7, uint16_t reg, uint8_t value, uint32_t timeout_ms);
bool i2c_worker_write_reg(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms);

// Raw transfers, for the many parts that have no register index at all: the
// humidity sensors take a bare command and answer with a bare block, and a
// display takes a control byte followed by a stream. Everything above is a
// convenience on top of these two.
bool i2c_worker_write_raw(uint8_t addr7, const uint8_t* data, size_t len, uint32_t timeout_ms);
bool i2c_worker_read_raw(uint8_t addr7, uint8_t* data, size_t len, uint32_t timeout_ms);
