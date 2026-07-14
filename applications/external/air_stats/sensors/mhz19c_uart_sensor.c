/*
 * mhz19c_uart_sensor.c — MH-Z19C CO2 sensor via UART (LPUART1, 9600 baud).
 * Pin 15 (C1, TX) → sensor RX, Pin 16 (C0, RX) ← sensor TX.
 * Protocol: 9-byte request/response, checksum = 0xFF - sum(bytes[1..7]) + 1.
 */
#include "mhz19c_uart_sensor.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_power.h>
#include <string.h>

#define MHZ19_UART_BUF_SIZE 9u
#define MHZ19_CMD_GAS_CONC  0x86

typedef struct {
    FuriStreamBuffer* stream;
    FuriHalSerialHandle* serial;
} MHZ19CUARTInstance;

static uint8_t mhz19c_uart_checksum(uint8_t* pkt) {
    uint8_t cs = 0;
    for(uint8_t i = 1; i < 8; i++)
        cs += pkt[i];
    return (uint8_t)(0xFF - cs + 1);
}

static void
    mhz19c_uart_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FuriStreamBuffer* stream = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(stream, &data, 1, 0);
    }
}

/* ---- Interface wrappers (same pattern as DIRECT_GPIO in mhz19c_sensor.c) ---- */

static bool mhz19c_uart_if_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    return sensor->type->allocator(sensor, NULL);
}

static bool mhz19c_uart_if_free(Sensor* sensor) {
    return sensor->type->mem_releaser(sensor);
}

static UnitempStatus mhz19c_uart_if_update(Sensor* sensor) {
    return sensor->type->updater(sensor);
}

static const Interface DIRECT_UART = {
    .name = "DirectUART",
    .allocator = mhz19c_uart_if_alloc,
    .mem_releaser = mhz19c_uart_if_free,
    .updater = mhz19c_uart_if_update,
};

/* ---- SensorType callbacks ---- */

static bool mhz19c_uart_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    MHZ19CUARTInstance* inst = malloc(sizeof(MHZ19CUARTInstance));
    if(!inst) return false;
    inst->stream = furi_stream_buffer_alloc(32, MHZ19_UART_BUF_SIZE);
    inst->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if(!inst->serial) {
        furi_stream_buffer_free(inst->stream);
        free(inst);
        return false;
    }
    sensor->instance = inst;
    return true;
}

static bool mhz19c_uart_free(Sensor* sensor) {
    MHZ19CUARTInstance* inst = sensor->instance;
    if(!inst) return true;
    furi_hal_serial_control_release(inst->serial);
    furi_stream_buffer_free(inst->stream);
    free(inst);
    sensor->instance = NULL;
    return true;
}

static bool mhz19c_uart_init(Sensor* sensor) {
    MHZ19CUARTInstance* inst = sensor->instance;
    if(!inst) return false;
    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    furi_hal_serial_init(inst->serial, 9600);
    furi_hal_serial_async_rx_start(inst->serial, mhz19c_uart_rx_cb, inst->stream, false);
    return true;
}

static bool mhz19c_uart_deinit(Sensor* sensor) {
    MHZ19CUARTInstance* inst = sensor->instance;
    if(!inst) return true;
    furi_hal_serial_async_rx_stop(inst->serial);
    furi_hal_serial_deinit(inst->serial);
    if(!app->settings.lastOTGState) furi_hal_power_disable_otg();
    return true;
}

static UnitempStatus mhz19c_uart_update(Sensor* sensor) {
    MHZ19CUARTInstance* inst = sensor->instance;
    if(!inst) return UT_SENSORSTATUS_ERROR;

    uint8_t buf[MHZ19_UART_BUF_SIZE] = {0};
    furi_stream_buffer_reset(inst->stream);

    buf[0] = 0xFF;
    buf[1] = 0x01;
    buf[2] = MHZ19_CMD_GAS_CONC;
    buf[8] = mhz19c_uart_checksum(buf);
    furi_hal_serial_tx(inst->serial, buf, sizeof(buf));

    size_t read = furi_stream_buffer_receive(inst->stream, buf, sizeof(buf), 50);
    if(read != MHZ19_UART_BUF_SIZE) {
        sensor->co2 = -1.0f;
        return UT_SENSORSTATUS_TIMEOUT;
    }
    if(buf[8] != mhz19c_uart_checksum(buf)) {
        sensor->co2 = -1.0f;
        return UT_SENSORSTATUS_BADCRC;
    }

    sensor->co2 = (float)((uint32_t)buf[2] * 256 + buf[3]);
    return UT_SENSORSTATUS_OK;
}

/* ---- Public SensorType ---- */
const SensorType MHZ19C_UART = {
    .typename = "MHZ19C_UART",
    .altname = "MH-Z19C (UART)",
    .datatype = UT_DATA_TYPE_CO2,
    .interface = &DIRECT_UART,
    .pollingInterval = 5000,
    .allocator = mhz19c_uart_alloc,
    .mem_releaser = mhz19c_uart_free,
    .initializer = mhz19c_uart_init,
    .deinitializer = mhz19c_uart_deinit,
    .updater = mhz19c_uart_update,
};
