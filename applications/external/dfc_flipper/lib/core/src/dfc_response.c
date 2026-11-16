#include "dfc_response.h"

#if DFC_ENABLE_READER

#include <string.h>

static bool input_is_valid(const uint8_t* response, size_t response_len) {
    return response != NULL || response_len == 0;
}

static uint32_t read_u24_le(const uint8_t* value) {
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16);
}

static int32_t read_i32_le(const uint8_t* value) {
    uint32_t raw = (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) |
                   ((uint32_t)value[3] << 24);
    int32_t result;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static DfcResponseAccess decode_access(uint8_t nibble) {
    DfcResponseAccess access = {0};
    if(nibble == DFC_ACCESS_FREE) {
        access.kind = DfcResponseAccessFree;
    } else if(nibble == DFC_ACCESS_DENY) {
        access.kind = DfcResponseAccessDenied;
    } else {
        access.kind = DfcResponseAccessKey;
        access.key_number = nibble;
    }
    return access;
}

static DfcResponseAccessRights decode_access_rights(uint16_t rights) {
    DfcResponseAccessRights decoded = {
        .read = decode_access((uint8_t)(rights >> 12)),
        .write = decode_access(dfc_access_nibble(rights, 8)),
        .read_write = decode_access(dfc_access_nibble(rights, 4)),
        .change = decode_access((uint8_t)(rights & 0x0Fu)),
    };
    return decoded;
}

static DfcResponseVersionPart decode_version_part(const uint8_t* response) {
    DfcResponseVersionPart part = {
        .vendor_id = response[0],
        .type = response[1],
        .subtype = response[2],
        .major_version = response[3],
        .minor_version = response[4],
        .storage_size = response[5],
        .protocol = response[6],
    };
    return part;
}

const char* dfc_response_decode_status_name(DfcResponseDecodeStatus status) {
    switch(status) {
    case DfcResponseDecodeOk:
        return "ok";
    case DfcResponseDecodeMalformed:
        return "malformed";
    case DfcResponseDecodeUnsupported:
        return "unsupported";
    case DfcResponseDecodeBufferTooSmall:
        return "buffer-too-small";
    default:
        return "unknown";
    }
}

DfcResponseDecodeStatus dfc_response_decode_version(
    const uint8_t* response,
    size_t response_len,
    DfcResponseVersion* version) {
    if(!version) return DfcResponseDecodeMalformed;
    memset(version, 0, sizeof(*version));
    if(!input_is_valid(response, response_len) || response_len != DFC_RESPONSE_VERSION_LENGTH)
        return DfcResponseDecodeMalformed;

    DfcResponseVersion decoded = {
        .hardware = decode_version_part(response),
        .software = decode_version_part(response + 7),
        .production_week = response[26],
        .production_year = response[27],
    };
    memcpy(decoded.uid, response + 14, sizeof(decoded.uid));
    memcpy(decoded.batch_number, response + 21, sizeof(decoded.batch_number));
    *version = decoded;
    return DfcResponseDecodeOk;
}

DfcResponseDecodeStatus dfc_response_decode_application_ids(
    const uint8_t* response,
    size_t response_len,
    uint8_t (*aids)[DFC_RESPONSE_AID_LENGTH],
    size_t aid_capacity,
    size_t* aid_count) {
    if(!aid_count) return DfcResponseDecodeMalformed;
    *aid_count = 0;
    if(!input_is_valid(response, response_len) || response_len % DFC_RESPONSE_AID_LENGTH != 0 ||
       (!aids && aid_capacity != 0))
        return DfcResponseDecodeMalformed;

    size_t required = response_len / DFC_RESPONSE_AID_LENGTH;
    *aid_count = required;
    if(aid_capacity < required) return DfcResponseDecodeBufferTooSmall;
    for(size_t i = 0; i < required; i++) {
        const uint8_t* wire = response + i * DFC_RESPONSE_AID_LENGTH;
        // Preserve the complete wire value before writing so callers may decode
        // a response in place.
        const uint8_t first = wire[0];
        const uint8_t middle = wire[1];
        const uint8_t last = wire[2];
        aids[i][0] = last;
        aids[i][1] = middle;
        aids[i][2] = first;
    }
    return DfcResponseDecodeOk;
}

DfcResponseDecodeStatus dfc_response_decode_file_ids(
    const uint8_t* response,
    size_t response_len,
    uint8_t* file_ids,
    size_t file_id_capacity,
    size_t* file_id_count) {
    if(!file_id_count) return DfcResponseDecodeMalformed;
    *file_id_count = 0;
    if(!input_is_valid(response, response_len) || (!file_ids && file_id_capacity != 0))
        return DfcResponseDecodeMalformed;
    *file_id_count = response_len;
    if(file_id_capacity < response_len) return DfcResponseDecodeBufferTooSmall;
    // JNI and other adapters may reuse their response buffer for decoded IDs.
    if(response_len != 0 && file_ids != response) memcpy(file_ids, response, response_len);
    return DfcResponseDecodeOk;
}

DfcResponseDecodeStatus dfc_response_decode_file_settings(
    const uint8_t* response,
    size_t response_len,
    DfcResponseFileSettings* settings) {
    if(!settings) return DfcResponseDecodeMalformed;
    memset(settings, 0, sizeof(*settings));
    if(!input_is_valid(response, response_len) || response_len < 4)
        return DfcResponseDecodeMalformed;

    size_t required_len;
    switch(response[0]) {
    case DFC_FILE_TYPE_STANDARD_DATA:
    case DFC_FILE_TYPE_BACKUP_DATA:
        required_len = 7;
        break;
    case DFC_FILE_TYPE_VALUE:
        required_len = 17;
        break;
    case DFC_FILE_TYPE_LINEAR_RECORD:
    case DFC_FILE_TYPE_CYCLIC_RECORD:
        required_len = 13;
        break;
    case DFC_FILE_TYPE_TRANSACTION_MAC:
    default:
        return DfcResponseDecodeUnsupported;
    }
    // SDM-enabled responses append fields whose presence depends on the SDM
    // options. Do not publish an incomplete typed result for that layout.
    if((response[1] & (uint8_t)~DFC_COMM_MODE_MASK) != 0) return DfcResponseDecodeUnsupported;
    if(response_len != required_len) return DfcResponseDecodeMalformed;
    uint8_t communication_mode = response[1] & DFC_COMM_MODE_MASK;
    if(communication_mode != DFC_COMM_PLAIN && communication_mode != DFC_COMM_MAC &&
       communication_mode != DFC_COMM_ENCIPHERED)
        return DfcResponseDecodeUnsupported;

    DfcResponseFileSettings decoded = {
        .type = response[0],
        .communication_mode = communication_mode,
        .access_rights_raw = (uint16_t)response[2] | ((uint16_t)response[3] << 8),
    };
    decoded.access = decode_access_rights(decoded.access_rights_raw);
    if(decoded.type == DFC_FILE_TYPE_STANDARD_DATA || decoded.type == DFC_FILE_TYPE_BACKUP_DATA) {
        decoded.settings.data.size = read_u24_le(response + 4);
    } else if(decoded.type == DFC_FILE_TYPE_VALUE) {
        decoded.settings.value.lower_limit = read_i32_le(response + 4);
        decoded.settings.value.upper_limit = read_i32_le(response + 8);
        decoded.settings.value.limited_credit_value = read_i32_le(response + 12);
        if(response[16] > 1) return DfcResponseDecodeUnsupported;
        decoded.settings.value.limited_credit_enabled = response[16] != 0;
    } else {
        decoded.settings.records.record_size = read_u24_le(response + 4);
        decoded.settings.records.max_records = read_u24_le(response + 7);
        decoded.settings.records.current_records = read_u24_le(response + 10);
    }
    *settings = decoded;
    return DfcResponseDecodeOk;
}

DfcResponseDecodeStatus dfc_response_decode_key_settings(
    const uint8_t* response,
    size_t response_len,
    DfcResponseKeySettings* settings) {
    if(!settings) return DfcResponseDecodeMalformed;
    memset(settings, 0, sizeof(*settings));
    if(!input_is_valid(response, response_len) || (response_len != 2 && response_len != 6))
        return DfcResponseDecodeMalformed;

    uint8_t key_type = response[1] & DFC_KEY_TYPE_MASK;
    if(key_type != DFC_KEY_TYPE_DES_2K3DES && key_type != DFC_KEY_TYPE_3K3DES &&
       key_type != DFC_KEY_TYPE_AES)
        return DfcResponseDecodeUnsupported;
    uint8_t key_count = response[1] & DFC_NUM_KEYS_MASK;
    if(key_count > DFC_MAX_KEYS) return DfcResponseDecodeUnsupported;

    DfcResponseKeySettings decoded = {
        .settings_1 = response[0],
        .settings_2 = response[1],
        .key_type = key_type,
        .key_count = key_count,
        .has_key_set_settings = response_len == 6,
    };
    if(response_len == 6) {
        decoded.active_key_set_version = response[2];
        decoded.key_set_count = response[3];
        decoded.max_key_size = response[4];
        decoded.key_set_settings = response[5];
        if(decoded.key_set_count < 2 || decoded.key_set_count > 16 ||
           (decoded.max_key_size != 0x10 && decoded.max_key_size != 0x18))
            return DfcResponseDecodeUnsupported;
    }
    *settings = decoded;
    return DfcResponseDecodeOk;
}

DfcResponseDecodeStatus dfc_response_decode_key_version(
    const uint8_t* response,
    size_t response_len,
    uint8_t* key_version) {
    if(!key_version) return DfcResponseDecodeMalformed;
    *key_version = 0;
    if(!input_is_valid(response, response_len) || response_len != 1)
        return DfcResponseDecodeMalformed;
    *key_version = response[0];
    return DfcResponseDecodeOk;
}

#endif
