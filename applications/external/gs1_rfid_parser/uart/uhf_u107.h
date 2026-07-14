#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/toolbox/bit_buffer.h>

#include "uhf_u107_tag.h"

#define UHF_EPC_USER_MEM_FLAG 0x0400

#define UHF_UART_TIMEOUT                   0
#define UHF_UART_INVALID_CHECKSUM          -1
#define UHF_UART_NOT_ENOUGH_SPACE          -2
#define UHF_UART_DIFFERING_COMMAND_CODE    -3
#define UHF_UART_MODULE_COMMAND_ERROR      -4
#define UHF_UART_INCORRECT_ACCESS_PASSWORD -5

typedef enum {
    RX_IDLE,
    RX_READING,
    RX_DONE,
    RX_STOP,
} UhfUartRxState;

typedef struct {
    FuriHalSerialHandle* uart_handle;

    BitBuffer* rx_buffer;
    volatile UhfUartRxState rx_state;
} UhfDevice;

UhfDevice* uhf_u107_alloc();
void uhf_u107_free(UhfDevice* device);

// Return >= 0 -> Amount of data read
// Return <  0 -> Error during read
int32_t uhf_u107_send_command(
    UhfDevice* device,
    const uint8_t* command,
    size_t command_length,
    uint8_t* response,
    size_t response_length,
    int32_t timeout);

int32_t uhf_u107_configure_device(UhfDevice* device);

int32_t uhf_u107_string_command(
    UhfDevice* device,
    const uint8_t* command,
    size_t command_length,
    char* response,
    size_t response_length,
    int32_t timeout);
int32_t uhf_u107_info_command(
    UhfDevice* device,
    uint8_t command,
    char* response,
    size_t response_length);

#define uhf_u107_get_hw_version(device, response, response_length) \
    uhf_u107_info_command(device, 0x00, response, response_length)
#define uhf_u107_get_sw_version(device, response, response_length) \
    uhf_u107_info_command(device, 0x01, response, response_length)
#define uhf_u107_get_manufacturer(device, response, response_length) \
    uhf_u107_info_command(device, 0x02, response, response_length)

int32_t uhf_u107_single_poll(UhfDevice* device, UhfEpcTagData* tag_epc);
int32_t uhf_u107_read_user_memory(UhfDevice* device, UhfUserMemoryData* user_memory);
