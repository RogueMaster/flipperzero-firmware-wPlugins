#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Protocol constants
#define CHAMELEON_SOF 0x11
#define CHAMELEON_LRC1 0xEF
#define CHAMELEON_MAX_DATA_LEN 512
#define CHAMELEON_FRAME_OVERHEAD 10

// Command IDs - Device Management (1000-1037)
#define CMD_GET_APP_VERSION 1000
#define CMD_CHANGE_DEVICE_MODE 1001
#define CMD_GET_DEVICE_MODE 1002
#define CMD_SET_ACTIVE_SLOT 1003
#define CMD_SET_SLOT_TAG_TYPE 1004
#define CMD_SET_SLOT_ENABLE 1006
#define CMD_SET_SLOT_TAG_NICK 1007
#define CMD_GET_DEVICE_CHIP_ID 1011
#define CMD_GET_GIT_VERSION 1017
#define CMD_GET_SLOT_INFO 1019
#define CMD_DELETE_SLOT_SENSE_TYPE 1024
#define CMD_GET_DEVICE_MODEL 1033
#define CMD_GET_DEVICE_CAPABILITIES 1035

// Command IDs - HF (High Frequency) 2000-2012
#define CMD_HF14A_SCAN 2000
#define CMD_MF1_DETECT_SUPPORT 2001
#define CMD_MF1_AUTH_ONE_KEY_BLOCK 2007
#define CMD_MF1_READ_ONE_BLOCK 2008
#define CMD_MF1_WRITE_ONE_BLOCK 2009
#define CMD_MF1_CHECK_KEYS_OF_SECTORS 2012

// Command IDs - LF (Low Frequency) 3000-3003
#define CMD_EM410X_SCAN 3000
#define CMD_HIDPROX_SCAN 3002

// Command IDs - Emulator Config 4000-4030
#define CMD_MF1_WRITE_EMU_BLOCK_DATA 4000
#define CMD_MF1_GET_EMULATOR_CONFIG 4009

// Command IDs - LF Emulator 5000-5003
#define CMD_EM410X_SET_EMU_ID 5000
#define CMD_HIDPROX_SET_EMU_ID 5002

// Status codes
#define STATUS_SUCCESS 0x0000
#define STATUS_HF_TAG_OK 0x0200
#define STATUS_LF_TAG_OK 0x0300
#define STATUS_HF_TAG_NO 0x0201
#define STATUS_MF_ERR_AUTH 0x0206
#define STATUS_FLASH_READ_FAIL 0x0401
#define STATUS_FLASH_WRITE_FAIL 0x0402
#define STATUS_INVALID_CMD 0x0001
#define STATUS_INVALID_PARAM 0x0002

// Frame structure
typedef struct {
    uint8_t sof;      // 0x11
    uint8_t lrc1;     // 0xEF
    uint16_t cmd;     // Command ID
    uint16_t status;  // Status code
    uint16_t len;     // Data length (0-512)
    uint8_t lrc2;     // Checksum of cmd|status|len
    uint8_t data[CHAMELEON_MAX_DATA_LEN];
    uint8_t lrc3;     // Checksum of data
} __attribute__((packed)) ChameleonFrame;

// Protocol instance
typedef struct ChameleonProtocol ChameleonProtocol;

// Protocol callbacks
typedef void (*ChameleonProtocolSendCallback)(const uint8_t* data, size_t length, void* context);

// Protocol creation and destruction
ChameleonProtocol* chameleon_protocol_alloc();
void chameleon_protocol_free(ChameleonProtocol* protocol);

// Set callback for sending data
void chameleon_protocol_set_send_callback(
    ChameleonProtocol* protocol,
    ChameleonProtocolSendCallback callback,
    void* context);

// Frame building
bool chameleon_protocol_build_frame(
    ChameleonProtocol* protocol,
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* frame_buffer,
    size_t* frame_len);

// Frame parsing
bool chameleon_protocol_parse_frame(
    ChameleonProtocol* protocol,
    const uint8_t* frame_data,
    size_t frame_len,
    uint16_t* cmd,
    uint16_t* status,
    uint8_t* data,
    uint16_t* data_len);

// LRC calculation
uint8_t chameleon_protocol_calculate_lrc(const uint8_t* data, size_t len);

// Command building helpers
bool chameleon_protocol_build_cmd_no_data(ChameleonProtocol* protocol, uint16_t cmd, uint8_t* buffer, size_t* len);

bool chameleon_protocol_build_cmd_with_data(
    ChameleonProtocol* protocol,
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* buffer,
    size_t* len);

// Validate frame
bool chameleon_protocol_validate_frame(const uint8_t* frame_data, size_t frame_len);

// Get frame length from header
size_t chameleon_protocol_get_expected_frame_len(const uint8_t* header);
