#include "chameleon_protocol.h"
#include <furi.h>
#include <string.h>

#define TAG "ChameleonProtocol"

struct ChameleonProtocol {
    ChameleonProtocolSendCallback send_callback;
    void* send_context;
};

ChameleonProtocol* chameleon_protocol_alloc() {
    ChameleonProtocol* protocol = malloc(sizeof(ChameleonProtocol));
    memset(protocol, 0, sizeof(ChameleonProtocol));
    return protocol;
}

void chameleon_protocol_free(ChameleonProtocol* protocol) {
    furi_assert(protocol);
    free(protocol);
}

void chameleon_protocol_set_send_callback(
    ChameleonProtocol* protocol,
    ChameleonProtocolSendCallback callback,
    void* context) {
    furi_assert(protocol);
    protocol->send_callback = callback;
    protocol->send_context = context;
}

uint8_t chameleon_protocol_calculate_lrc(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1); // Two's complement
}

bool chameleon_protocol_build_frame(
    ChameleonProtocol* protocol,
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* frame_buffer,
    size_t* frame_len) {

    UNUSED(protocol);

    if(data_len > CHAMELEON_MAX_DATA_LEN) {
        FURI_LOG_E(TAG, "Data length %u exceeds maximum %u", data_len, CHAMELEON_MAX_DATA_LEN);
        return false;
    }

    size_t idx = 0;

    // SOF and LRC1
    frame_buffer[idx++] = CHAMELEON_SOF;
    frame_buffer[idx++] = CHAMELEON_LRC1;

    // CMD (Big Endian)
    frame_buffer[idx++] = (cmd >> 8) & 0xFF;
    frame_buffer[idx++] = cmd & 0xFF;

    // STATUS (always 0x0000 for client->device)
    frame_buffer[idx++] = 0x00;
    frame_buffer[idx++] = 0x00;

    // LEN (Big Endian)
    frame_buffer[idx++] = (data_len >> 8) & 0xFF;
    frame_buffer[idx++] = data_len & 0xFF;

    // Calculate LRC2 (over CMD|STATUS|LEN)
    frame_buffer[idx] = chameleon_protocol_calculate_lrc(&frame_buffer[2], 6);
    idx++;

    // DATA
    if(data_len > 0 && data != NULL) {
        memcpy(&frame_buffer[idx], data, data_len);
        idx += data_len;
    }

    // Calculate LRC3 (over DATA)
    frame_buffer[idx] = chameleon_protocol_calculate_lrc(&frame_buffer[9], data_len);
    idx++;

    *frame_len = idx;

    FURI_LOG_D(TAG, "Built frame: CMD=%04X, LEN=%u, total=%zu", cmd, data_len, idx);

    return true;
}

bool chameleon_protocol_parse_frame(
    ChameleonProtocol* protocol,
    const uint8_t* frame_data,
    size_t frame_len,
    uint16_t* cmd,
    uint16_t* status,
    uint8_t* data,
    uint16_t* data_len) {

    UNUSED(protocol);

    if(frame_len < CHAMELEON_FRAME_OVERHEAD) {
        FURI_LOG_E(TAG, "Frame too short: %zu bytes", frame_len);
        return false;
    }

    // Check SOF and LRC1
    if(frame_data[0] != CHAMELEON_SOF || frame_data[1] != CHAMELEON_LRC1) {
        FURI_LOG_E(TAG, "Invalid SOF/LRC1: %02X %02X", frame_data[0], frame_data[1]);
        return false;
    }

    // Extract CMD (Big Endian)
    *cmd = ((uint16_t)frame_data[2] << 8) | frame_data[3];

    // Extract STATUS (Big Endian)
    *status = ((uint16_t)frame_data[4] << 8) | frame_data[5];

    // Extract LEN (Big Endian)
    *data_len = ((uint16_t)frame_data[6] << 8) | frame_data[7];

    // Verify LRC2
    uint8_t expected_lrc2 = chameleon_protocol_calculate_lrc(&frame_data[2], 6);
    if(frame_data[8] != expected_lrc2) {
        FURI_LOG_E(TAG, "LRC2 mismatch: expected %02X, got %02X", expected_lrc2, frame_data[8]);
        return false;
    }

    // Check frame length
    size_t expected_len = CHAMELEON_FRAME_OVERHEAD + *data_len;
    if(frame_len != expected_len) {
        FURI_LOG_E(TAG, "Frame length mismatch: expected %zu, got %zu", expected_len, frame_len);
        return false;
    }

    // Extract data if present
    if(*data_len > 0) {
        memcpy(data, &frame_data[9], *data_len);

        // Verify LRC3
        uint8_t expected_lrc3 = chameleon_protocol_calculate_lrc(&frame_data[9], *data_len);
        if(frame_data[9 + *data_len] != expected_lrc3) {
            FURI_LOG_E(TAG, "LRC3 mismatch: expected %02X, got %02X", expected_lrc3, frame_data[9 + *data_len]);
            return false;
        }
    }

    FURI_LOG_D(TAG, "Parsed frame: CMD=%04X, STATUS=%04X, LEN=%u", *cmd, *status, *data_len);

    return true;
}

bool chameleon_protocol_validate_frame(const uint8_t* frame_data, size_t frame_len) {
    if(frame_len < CHAMELEON_FRAME_OVERHEAD) {
        return false;
    }

    if(frame_data[0] != CHAMELEON_SOF || frame_data[1] != CHAMELEON_LRC1) {
        return false;
    }

    uint16_t data_len = ((uint16_t)frame_data[6] << 8) | frame_data[7];
    size_t expected_len = CHAMELEON_FRAME_OVERHEAD + data_len;

    return (frame_len == expected_len);
}

size_t chameleon_protocol_get_expected_frame_len(const uint8_t* header) {
    if(header[0] != CHAMELEON_SOF || header[1] != CHAMELEON_LRC1) {
        return 0;
    }

    uint16_t data_len = ((uint16_t)header[6] << 8) | header[7];
    return CHAMELEON_FRAME_OVERHEAD + data_len;
}

bool chameleon_protocol_build_cmd_no_data(
    ChameleonProtocol* protocol,
    uint16_t cmd,
    uint8_t* buffer,
    size_t* len) {
    return chameleon_protocol_build_frame(protocol, cmd, NULL, 0, buffer, len);
}

bool chameleon_protocol_build_cmd_with_data(
    ChameleonProtocol* protocol,
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* buffer,
    size_t* len) {
    return chameleon_protocol_build_frame(protocol, cmd, data, data_len, buffer, len);
}
