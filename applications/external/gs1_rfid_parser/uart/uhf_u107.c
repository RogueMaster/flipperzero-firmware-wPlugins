
#include "uhf_u107.h"

#define LOG_TAG "gs1_parser_uhf_u107"

#define UHF_U107_BAUD_RATE 115200

#define UHF_U107_HEADER 0xBB
#define UHF_U107_FOOTER 0x7E

static void log_array(const char* tag, const uint8_t* data, size_t data_length) {
    char data_str[3 * data_length + 1];

    for(size_t i = 0; i < data_length; i++) {
        snprintf(&data_str[3 * i], 4, " %02X", data[i]);
    }

    data_str[3 * data_length] = '\0';
    FURI_LOG_D(LOG_TAG, "%s:%s", tag, data_str);
}

static void uhf_u107_rx_callback(
    FuriHalSerialHandle* uart_handle,
    FuriHalSerialRxEvent event,
    void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    furi_assert(uart_handle);

    UhfDevice* device = (UhfDevice*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(uart_handle);
        FURI_LOG_D(LOG_TAG, "RX Data Read = %02X, RX State = %02X", data, device->rx_state);

        if(device->rx_state == RX_STOP) {
            // Read data when not expecting it
        } else if(data == UHF_U107_HEADER) {
            FURI_LOG_D(LOG_TAG, "Header Data Found");
            device->rx_state = RX_READING;
        } else if(data == UHF_U107_FOOTER) {
            FURI_LOG_D(
                LOG_TAG, "Total length read: %u", bit_buffer_get_size_bytes(device->rx_buffer));
            device->rx_state = RX_DONE;
        } else if(device->rx_state == RX_READING) {
            bit_buffer_append_byte(device->rx_buffer, data);
        }
    }
}

/** Computes the U107 data checksum.
 *  Note: This is just summing the bytes in the data mod 256.
 *  
 *  @param data The data to compute the checksum for
 *  @return The checksum of the data
 */
static uint8_t compute_checksum(const uint8_t* data, const size_t data_length) {
    uint8_t checksum = 0;

    for(size_t i = 0; i < data_length; i++) {
        checksum += data[i];
    }

    return checksum;
}

// Return >= 0 -> Amount of data read
// Return <  0 -> Error during read
int32_t uhf_u107_send_command(
    UhfDevice* device,
    const uint8_t* command,
    size_t command_length,
    uint8_t* response,
    size_t response_length,
    int32_t timeout) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);
    furi_assert(response);
    furi_assert(command);

    device->rx_state = RX_IDLE;
    bit_buffer_reset(device->rx_buffer);

    uint8_t complete_command[command_length + 3];
    complete_command[0] = UHF_U107_HEADER;
    memcpy(&complete_command[1], command, command_length);
    complete_command[command_length + 1] = compute_checksum(command, command_length);
    complete_command[command_length + 2] = UHF_U107_FOOTER;

    log_array("Sending", complete_command, command_length + 3);
    furi_hal_serial_tx(device->uart_handle, complete_command, command_length + 3);

    uint32_t start_tick = furi_get_tick();
    uint32_t end_tick = start_tick + timeout;

    while(device->rx_state != RX_DONE) {
        uint32_t current_tick = furi_get_tick();

        // Stop when the current tick is past the end tick (properly handling overflow)
        if(current_tick > end_tick &&
           ((current_tick > start_tick &&
             start_tick < end_tick) || // Normal case of no overflow during the wait
            (current_tick < start_tick &&
             start_tick > end_tick) // Special case of overflow during the wait
            )) {
            break;
        }
    }

    device->rx_state = RX_STOP;

    // Need to remove 1 to account for removing the checksum
    size_t data_length = bit_buffer_get_size_bytes(device->rx_buffer) - 1;

    if(data_length > 256) {
        FURI_LOG_D(LOG_TAG, "Failed to find data in time");
        return UHF_UART_TIMEOUT; // No data found
    }

    if(data_length > response_length) {
        FURI_LOG_E(
            LOG_TAG, "Not enough space to return result: %u > %u", data_length, response_length);
        return UHF_UART_NOT_ENOUGH_SPACE;
    }

    bit_buffer_write_bytes(device->rx_buffer, response, response_length);
    FURI_LOG_D(LOG_TAG, "RX State = %02X, Data Length = %u", device->rx_state, data_length);
    log_array("Received", response, data_length);

    if(response[1] == 0xFF) {
        FURI_LOG_E(LOG_TAG, "Module Reported Error %02X", response[4]);
        return UHF_UART_MODULE_COMMAND_ERROR;
    }

    if(command[1] != response[1]) {
        FURI_LOG_E(LOG_TAG, "Invalid command code: %02X != %02X", command[1], response[1]);
        return UHF_UART_DIFFERING_COMMAND_CODE;
    }

    uint8_t checksum = bit_buffer_get_byte(device->rx_buffer, data_length);
    uint8_t computed_checksum = compute_checksum(response, data_length);

    if(checksum != computed_checksum) {
        FURI_LOG_E(LOG_TAG, "Invalid checksum: %02X != %u", checksum, computed_checksum);
        return UHF_UART_INVALID_CHECKSUM;
    }

    return data_length;
}

UhfDevice* uhf_u107_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    UhfDevice* device = (UhfDevice*)malloc(sizeof(UhfDevice));

    device->rx_buffer = bit_buffer_alloc(256);

    device->uart_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(device->uart_handle, UHF_U107_BAUD_RATE);
    furi_hal_serial_async_rx_start(device->uart_handle, uhf_u107_rx_callback, device, false);

    return device;
}

void uhf_u107_free(UhfDevice* device) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    furi_hal_serial_async_rx_stop(device->uart_handle);
    furi_hal_serial_deinit(device->uart_handle);
    furi_hal_serial_control_release(device->uart_handle);

    bit_buffer_free(device->rx_buffer);

    free(device);
}
