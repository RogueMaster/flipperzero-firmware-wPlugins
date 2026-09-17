#include "dfc_virtual_picc.h"

#include <stdio.h>

#define HOST_STATUS_OK    0x00
#define HOST_STATUS_ERROR 0x01

#define HOST_OP_INIT                   0x01
#define HOST_OP_SCAN_ISO14443A         0x02
#define HOST_OP_EXCHANGE               0x03
#define HOST_OP_FIELD_OFF              0x04
#define HOST_OP_EXCHANGE_ISO_DEP_FRAME 0x05

typedef struct {
    const uint8_t* data;
    size_t length;
    size_t offset;
} HostPayloadReader;

static bool read_exact(uint8_t* buffer, size_t length) {
    return fread(buffer, 1, length, stdin) == length;
}

static bool write_response(uint8_t status, const uint8_t* payload, uint16_t payload_len) {
    uint8_t header[] = {status, (uint8_t)(payload_len & 0xFF), (uint8_t)(payload_len >> 8)};
    if(fwrite(header, 1, sizeof(header), stdout) != sizeof(header)) return false;
    if(payload_len > 0 && fwrite(payload, 1, payload_len, stdout) != payload_len) return false;
    return fflush(stdout) == 0;
}

static bool reader_byte(HostPayloadReader* reader, uint8_t* value) {
    if(reader->offset >= reader->length) return false;
    *value = reader->data[reader->offset++];
    return true;
}

static bool
    reader_bytes(HostPayloadReader* reader, uint8_t* value, size_t capacity, size_t* value_len) {
    uint8_t length = 0;
    if(!reader_byte(reader, &length)) return false;
    if(length > capacity || reader->offset + length > reader->length) return false;
    memcpy(value, reader->data + reader->offset, length);
    reader->offset += length;
    *value_len = length;
    return true;
}

static bool reader_fixed(HostPayloadReader* reader, uint8_t* value, size_t length) {
    if(reader->offset + length > reader->length) return false;
    memcpy(value, reader->data + reader->offset, length);
    reader->offset += length;
    return true;
}

static bool reader_u16(HostPayloadReader* reader, uint16_t* value) {
    uint8_t lo = 0;
    uint8_t hi = 0;
    if(!reader_byte(reader, &lo) || !reader_byte(reader, &hi)) return false;
    *value = (uint16_t)(lo | (hi << 8));
    return true;
}

static bool
    init_credential(DfcCredential* credential, const uint8_t* payload, size_t payload_len) {
    HostPayloadReader reader = {.data = payload, .length = payload_len, .offset = 0};
    dfc_credential_clear(credential);

    if(!reader_bytes(&reader, credential->uid, sizeof(credential->uid), &credential->uid_len))
        return false;
    if(credential->num_apps >= DFC_MAX_APPS) return false;
    DfcApplication* app = &credential->apps[credential->num_apps++];
    dfc_credential_reset_application(app);
    if(!reader_fixed(&reader, app->aid, sizeof(app->aid))) return false;
    if(!reader_bytes(&reader, app->iso_aid, sizeof(app->iso_aid), &app->iso_aid_len)) return false;
    if(!reader_byte(&reader, &app->key_settings_1)) return false;
    if(!reader_byte(&reader, &app->key_settings_2)) return false;
    if(!reader_byte(&reader, &app->auth_command)) return false;

    uint8_t key_count = 0;
    if(!reader_byte(&reader, &key_count)) return false;
    if(key_count > DFC_MAX_KEYS) return false;
    if(!dfc_credential_keys_resize(
           credential, app, key_count, dfc_credential_key_length(app->key_settings_2))) {
        return false;
    }
    for(size_t i = 0; i < key_count; i++) {
        uint8_t key_number = 0;
        size_t key_len = 0;
        uint8_t raw[DFC_MAX_KEY_LEN];
        if(!reader_byte(&reader, &key_number)) return false;
        if(key_number >= key_count) return false;
        if(!reader_bytes(&reader, raw, sizeof(raw), &key_len)) return false;
        if(key_len != app->key_len) return false;
        uint8_t* dst = dfc_credential_key(credential, app, key_number);
        if(!dst) return false;
        memcpy(dst, raw, key_len);
    }

    uint8_t file_count = 0;
    if(!reader_byte(&reader, &file_count)) return false;
    if(file_count > DFC_MAX_FILES) return false;
    credential->num_files = file_count;
    for(size_t i = 0; i < file_count; i++) {
        DfcFile* file = &credential->files[i];
        file->app_index = 0;
        if(!reader_byte(&reader, &file->number)) return false;
        file->type = 0x00;
        if(!reader_byte(&reader, &file->comm_settings)) return false;
        if(!reader_u16(&reader, &file->access_rights)) return false;
        uint8_t tmp[DFC_MAX_FILE_DATA];
        size_t data_len = 0;
        if(!reader_bytes(&reader, tmp, DFC_MAX_FILE_DATA, &data_len)) return false;
        if(!dfc_file_resize(credential, file, data_len)) return false;
        if(data_len > 0) {
            uint8_t* dest = dfc_file_data(credential, file);
            if(!dest) return false;
            memcpy(dest, tmp, data_len);
        }
    }

    return reader.offset == reader.length && dfc_credential_uid_is_detectable(credential);
}

static uint16_t write_activation(uint8_t* out, DfcVirtualPiccActivation activation) {
    size_t offset = 0;
    out[offset++] = (uint8_t)activation.protocol_len;
    memcpy(out + offset, activation.protocol, activation.protocol_len);
    offset += activation.protocol_len;
    out[offset++] = (uint8_t)activation.uid_len;
    memcpy(out + offset, activation.uid, activation.uid_len);
    offset += activation.uid_len;
    out[offset++] = (uint8_t)activation.ats_len;
    memcpy(out + offset, activation.ats, activation.ats_len);
    offset += activation.ats_len;
    out[offset++] = activation.sak;
    out[offset++] = (uint8_t)activation.atqa_len;
    memcpy(out + offset, activation.atqa, activation.atqa_len);
    offset += activation.atqa_len;
    out[offset++] = (uint8_t)activation.rf_detail_len;
    memcpy(out + offset, activation.rf_detail, activation.rf_detail_len);
    offset += activation.rf_detail_len;
    return (uint16_t)offset;
}

int main(void) {
    DfcCredential* credential = dfc_credential_alloc();
    DfcVirtualPiccSession* session = NULL;

    while(true) {
        uint8_t header[3];
        if(!read_exact(header, sizeof(header))) break;

        uint8_t operation = header[0];
        uint16_t payload_len = (uint16_t)(header[1] | (header[2] << 8));
        uint8_t payload[DFC_WORKER_MAX_BUFFER_SIZE * 4];
        if(payload_len > sizeof(payload) || !read_exact(payload, payload_len)) {
            write_response(HOST_STATUS_ERROR, NULL, 0);
            break;
        }

        if(operation == HOST_OP_INIT) {
            if(session) {
                dfc_virtual_picc_session_free(session);
                session = NULL;
            }
            if(!init_credential(credential, payload, payload_len)) {
                write_response(HOST_STATUS_ERROR, NULL, 0);
                continue;
            }
            session = dfc_virtual_picc_session_alloc(credential);
            write_response(HOST_STATUS_OK, NULL, 0);
            continue;
        }

        if(!session) {
            write_response(HOST_STATUS_ERROR, NULL, 0);
            continue;
        }

        if(operation == HOST_OP_SCAN_ISO14443A) {
            DfcVirtualPiccActivation activation;
            uint8_t response[64];
            DfcVirtualPiccStatus status = dfc_virtual_picc_scan_iso14443a(session, &activation);
            if(status != DfcVirtualPiccStatusOk) {
                write_response(HOST_STATUS_ERROR, NULL, 0);
                continue;
            }
            uint16_t response_len = write_activation(response, activation);
            write_response(HOST_STATUS_OK, response, response_len);
            continue;
        }

        if(operation == HOST_OP_EXCHANGE) {
            uint8_t response[DFC_WORKER_MAX_BUFFER_SIZE];
            size_t response_len = 0;
            DfcVirtualPiccStatus status = dfc_virtual_picc_iso_dep_exchange(
                session, payload, payload_len, response, sizeof(response), &response_len);
            write_response(
                status == DfcVirtualPiccStatusOk ? HOST_STATUS_OK : HOST_STATUS_ERROR,
                response,
                (uint16_t)response_len);
            continue;
        }

        if(operation == HOST_OP_FIELD_OFF) {
            dfc_virtual_picc_field_off(session);
            write_response(HOST_STATUS_OK, NULL, 0);
            continue;
        }

        if(operation == HOST_OP_EXCHANGE_ISO_DEP_FRAME) {
            uint8_t response[DFC_WORKER_MAX_BUFFER_SIZE];
            size_t response_len = 0;
            DfcVirtualPiccStatus status = dfc_virtual_picc_iso_dep_frame_exchange(
                session, payload, payload_len, response, sizeof(response), &response_len);
            write_response(
                status == DfcVirtualPiccStatusOk ? HOST_STATUS_OK : HOST_STATUS_ERROR,
                response,
                (uint16_t)response_len);
            continue;
        }

        write_response(HOST_STATUS_ERROR, NULL, 0);
    }

    if(session) {
        dfc_virtual_picc_session_free(session);
    }
    dfc_credential_free(credential);
    return 0;
}
