
#include "uhf_u107.h"

#define LOG_TAG "gs1_parser_uhf_u107_commands"

#define command_size(command) (sizeof(command) / sizeof(command[0]))

// Most commands should respond in under 1000 ticks
#define DEFAULT_TIMEOUT 1000 // ticks

int32_t uhf_u107_configure_device(UhfDevice* device) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    uint8_t response[64];
    const uint8_t baud_rate_command[] = {
        0x00, 0x11, 0x00, 0x00, 0xC0, 0x80}; // Set baud rate to 115200 (0x480 = 115200 / 100)
    const uint8_t power_command[] = {
        0x00, 0xB6, 0x00, 0x02, 0x07, 0xD0}; // Set power to 20 dBm (0x7D0 = 20 * 100)
    const uint8_t region_command[] = {0x00, 0x07, 0x00, 0x01, 0x02}; // Set the region to US
    const uint8_t enable_frequency_hopping[] = {
        0x00, 0xAD, 0x00, 0x01, 0xFF}; // Enable frequency hopping

    int32_t result = 1;

    result = uhf_u107_send_command(
        device, baud_rate_command, command_size(baud_rate_command), response, 64, DEFAULT_TIMEOUT);
    FURI_LOG_D(LOG_TAG, "Baud Rate Result: %ld", result);

    // Note: Setting the baud rate is expected to report an error (don't know why though)
    if(result > 0 || result == UHF_UART_MODULE_COMMAND_ERROR) {
        result = uhf_u107_send_command(
            device, power_command, command_size(power_command), response, 64, DEFAULT_TIMEOUT);
        FURI_LOG_D(LOG_TAG, "Power Result: %ld", result);
    }

    if(result > 0) {
        result = uhf_u107_send_command(
            device, region_command, command_size(region_command), response, 64, DEFAULT_TIMEOUT);
        FURI_LOG_D(LOG_TAG, "Region Result: %ld", result);
    }

    if(result > 0) {
        result = uhf_u107_send_command(
            device,
            enable_frequency_hopping,
            command_size(enable_frequency_hopping),
            response,
            64,
            DEFAULT_TIMEOUT);
        FURI_LOG_D(LOG_TAG, "Frequency Hopping Result: %ld", result);
    }

    return result;
}

int32_t uhf_u107_string_command(
    UhfDevice* device,
    const uint8_t* command,
    size_t command_length,
    char* response,
    size_t response_length,
    int32_t timeout) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    uint8_t local_response[response_length + 5];
    int32_t result = uhf_u107_send_command(
        device, command, command_length, local_response, response_length + 5, timeout);

    // Return the error
    if(result <= 0) {
        return result;
    }

    // Skip the first 5 bytes to get to the actual result
    result -= 5;
    memcpy(response, &local_response[5], result);
    response[result] = '\0'; // Just make sure to end the string

    return result;
}

int32_t uhf_u107_info_command(
    UhfDevice* device,
    uint8_t command,
    char* response,
    size_t response_length) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    const uint8_t info_command[] = {0x00, 0x03, 0x00, 0x01, command};

    return uhf_u107_string_command(
        device,
        info_command,
        command_size(info_command),
        response,
        response_length,
        DEFAULT_TIMEOUT);
}

int32_t uhf_u107_single_poll(UhfDevice* device, UhfEpcTagData* tag_epc) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    tag_epc->is_valid = false;

    uint8_t response[MAX_UHF_BANK_SIZE];
    const uint8_t single_poll_command[] = {0x00, 0x22, 0x00, 0x00};
    int32_t result = uhf_u107_send_command(
        device,
        single_poll_command,
        command_size(single_poll_command),
        response,
        MAX_UHF_BANK_SIZE,
        DEFAULT_TIMEOUT);

    if(result <= 0) {
        return result == UHF_UART_MODULE_COMMAND_ERROR ? UHF_UART_TIMEOUT : result;
    }

    // A minimum of 10 bytes will be returned for the type, command, parameter length (2 bytes), RSSI, PC (2 bytes), CRC (2 bytes), and at least 1 byte for the EPC
    if(result < 10) {
        FURI_LOG_E(LOG_TAG, "Insufficient data returned (%ld bytes) by reader", result);
        return UHF_UART_MODULE_COMMAND_ERROR;
    }

    tag_epc->is_valid = true;
    tag_epc->pc = (response[5] << 8) | response[6];
    tag_epc->rssi = response[4];
    tag_epc->crc = (response[result - 2] << 8) | response[result - 1];
    tag_epc->epc_size = result - 9;

    memcpy(tag_epc->epc_data, &response[7], tag_epc->epc_size);

    return result;
}

// TODO Implement actual command
// 0x00 -> Reserved
// 0x01 -> EPC
// 0x02 -> TID
// 0x03 -> User Memory
// Note: Assumes response length is MAX_UHF_BANK_SIZE
static int32_t
    uhf_u107_read_data_bank(UhfDevice* device, uint8_t data_bank, uint8_t* data_bank_contents) {
    uint8_t read_bank_command[] = {
        0x00, // Type
        0x39, // CMD
        0x00,
        0x09, // PL
        0x00,
        0x00,
        0x00,
        0x00, // Password
        data_bank, // Mem Bank (User)
        0x00,
        0x00, // Address Offset
        0x00,
        0x01 // Data Length
    };
    int32_t result = 0;
    uint8_t i;
    uint8_t response[MAX_UHF_BANK_SIZE];

    // Disable requiring to send a select mode command prior to reading other data banks
    const uint8_t disable_select_mode[] = {0x00, 0x12, 0x00, 0x01, 0x01};
    uhf_u107_send_command(
        device,
        disable_select_mode,
        command_size(disable_select_mode),
        response,
        64,
        DEFAULT_TIMEOUT);

    // Read 1 word (16 bits) at a time
    for(i = 0; i < MAX_UHF_BANK_SIZE / 2; i++) {
        read_bank_command[10] = i;

        result = uhf_u107_send_command(
            device,
            read_bank_command,
            sizeof(read_bank_command),
            response,
            MAX_UHF_BANK_SIZE,
            DEFAULT_TIMEOUT);

        if(result == UHF_UART_MODULE_COMMAND_ERROR) {
            if(response[4] == 0x16) {
                return UHF_UART_INCORRECT_ACCESS_PASSWORD;
            } else if(response[4] == 0xA3) {
                // Attempted to read past the end of the data bank
                break;
            } else {
                return result;
            }
        } else if(result <= 0) {
            return result;
        } else if(result <= 9) { // 7 bytes of boiler plate, and two bytes of user memory
            FURI_LOG_E(LOG_TAG, "Insufficient data returned (%ld bytes) by reader", result);
            return UHF_UART_MODULE_COMMAND_ERROR;
        }

        data_bank_contents[2 * i] = response[result - 2];
        data_bank_contents[2 * i + 1] = response[result - 1];
    }

    return 2 * i;
}

int32_t uhf_u107_read_user_memory(UhfDevice* device, UhfUserMemoryData* user_memory) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(device);

    user_memory->is_valid = false;

    int32_t result = uhf_u107_read_data_bank(device, 0x03, user_memory->user_mem_data);

    if(result <= 0) {
        return result;
    }

    user_memory->is_valid = true;
    user_memory->user_mem_size = result;

    return result;
}
