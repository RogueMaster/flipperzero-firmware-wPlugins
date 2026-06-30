#include "co2_logger.h"
#include <furi.h>
#include <furi_hal.h>

#define co2_logger_UART_EXCHANGE_SIZE (9u)
#define co2_logger_UART_COMMAND_AUTOCALIBRATION (0x79)
#define co2_logger_UART_COMMAND_GAS_CONCENTRATION (0x86)
#define co2_logger_UART_COMMAND_CALIBRATE_ZERO (0x87)
#define co2_logger_UART_COMMAND_CALIBRATE_SPAN (0x88)
#define co2_logger_UART_COMMAND_SET_RANGE (0x89)

struct co2_logger {
    FuriStreamBuffer* stream;
    FuriHalSerialHandle* serial;
};

static uint8_t co2_logger_checksum(uint8_t* packet) {
    uint8_t checksum = 0;
    for(size_t i = 1; i < 8; i++) {
        checksum += packet[i];
    }
    checksum = 0xff - checksum;
    checksum += 1;
    return checksum;
}

static void co2_logger_uart_worker_uart_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    FuriStreamBuffer* stream = context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(stream, &data, 1, 0);
    }
}

co2_logger* co2_logger_alloc() {
    FURI_LOG_D("CO2", "Allocating CO2 logger instance");
    co2_logger* instance = malloc(sizeof(co2_logger));
    if (!instance) {
        FURI_LOG_E("CO2", "Failed to allocate memory for CO2 logger");
        return NULL;
    }
    
    FURI_LOG_D("CO2", "Allocating stream buffer");
    instance->stream = furi_stream_buffer_alloc(32, co2_logger_UART_EXCHANGE_SIZE);
    if (!instance->stream) {
        FURI_LOG_E("CO2", "Failed to allocate stream buffer");
        free(instance);
        return NULL;
    }
    
    FURI_LOG_D("CO2", "Acquiring serial control");
    instance->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if (!instance->serial) {
        FURI_LOG_W("CO2", "Failed to acquire LPUART, trying USART1");
        instance->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
        if (!instance->serial) {
            FURI_LOG_E("CO2", "Failed to acquire any serial control");
            furi_stream_buffer_free(instance->stream);
            free(instance);
            return NULL;
        }
    }
    
    FURI_LOG_I("CO2", "CO2 logger instance allocated successfully");
    return instance;
}

void co2_logger_free(co2_logger* instance) {
    furi_assert(instance);
    FURI_LOG_D("CO2", "Freeing CO2 logger instance");
    
    furi_hal_serial_control_release(instance->serial);
    furi_stream_buffer_free(instance->stream);
    free(instance);
    
    FURI_LOG_D("CO2", "CO2 logger instance freed");
}

void co2_logger_open(co2_logger* instance) {
    furi_assert(instance);
    FURI_LOG_I("CO2", "Opening CO2 logger connection");
    
    FURI_LOG_D("CO2", "Initializing serial at 9600 baud");
    furi_hal_serial_init(instance->serial, 9600);
    
    FURI_LOG_D("CO2", "Starting async RX");
    furi_hal_serial_async_rx_start(
        instance->serial, co2_logger_uart_worker_uart_cb, instance->stream, false);
    
    FURI_LOG_D("CO2", "Enabling OTG power");
    furi_hal_power_enable_otg();
    
    FURI_LOG_I("CO2", "CO2 logger opened successfully");
}

void co2_logger_close(co2_logger* instance) {
    furi_assert(instance);
    FURI_LOG_I("CO2", "Closing CO2 logger connection");
    
    FURI_LOG_D("CO2", "Disabling OTG power");
    furi_hal_power_disable_otg();
    
    FURI_LOG_D("CO2", "Stopping async RX");
    furi_hal_serial_async_rx_stop(instance->serial);
    
    FURI_LOG_D("CO2", "Deinitializing serial");
    furi_hal_serial_deinit(instance->serial);
    
    FURI_LOG_I("CO2", "CO2 logger closed successfully");
}

bool co2_logger_read_gas_concentration(co2_logger* instance, uint32_t* value) {
    furi_assert(instance);

    uint8_t buffer[co2_logger_UART_EXCHANGE_SIZE] = {0};
    furi_stream_buffer_reset(instance->stream);

    FURI_LOG_D("UART", "Sending request for gas concentration");
    
    // Send Request
    buffer[0] = 0xff;
    buffer[1] = 0x01;
    buffer[2] = co2_logger_UART_COMMAND_GAS_CONCENTRATION;
    buffer[8] = co2_logger_checksum(buffer);
    
    FURI_LOG_D("UART", "Sending command: FF %02X %02X 00 00 00 00 00 %02X", 
               buffer[1], buffer[2], buffer[8]);
    
    furi_hal_serial_tx(instance->serial, (uint8_t*)buffer, sizeof(buffer));

    FURI_LOG_D("UART", "Waiting for response...");
    
    // Get response
    bool ret = false;
    do {
        size_t read_size =
            furi_stream_buffer_receive(instance->stream, buffer, sizeof(buffer), 50);
        
        FURI_LOG_D("UART", "Received %zu bytes (expected %d)", read_size, co2_logger_UART_EXCHANGE_SIZE);
        
        if(read_size != co2_logger_UART_EXCHANGE_SIZE) {
            FURI_LOG_D("UART", "RX failed %zu (expected %d)", read_size, co2_logger_UART_EXCHANGE_SIZE);
            break;
        }

        // Log the received data
        FURI_LOG_D("UART", "RX Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], 
                   buffer[5], buffer[6], buffer[7], buffer[8]);

        uint8_t calculated_checksum = co2_logger_checksum(buffer);
        if(buffer[8] != calculated_checksum) {
            FURI_LOG_E("UART", "Incorrect checksum %02X!=%02X", buffer[8], calculated_checksum);
            break;
        }

        *value = (uint32_t)buffer[2] * 256 + buffer[3];
        FURI_LOG_D("UART", "Successfully read CO2: %lu ppm (bytes[2]=%02X, bytes[3]=%02X)", 
                   *value, buffer[2], buffer[3]);

        ret = true;
    } while(false);

    if (!ret) {
        FURI_LOG_W("UART", "Failed to read CO2 concentration");
    }

    return ret;
}
