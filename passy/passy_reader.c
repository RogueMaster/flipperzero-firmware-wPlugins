#include "passy_reader.h"

#define ASN_EMIT_DEBUG 0
#include <lib/asn1/COM.h>

#define TAG                         "PassyReader"
#define PASSY_READER_DG1_CHUNK_SIZE 0x20
#define PASSY_READER_DG2_CHUNK_SIZE 0x20

#define PASSY_DG2_LOOKAHEAD 120

static uint8_t passport_aid[] = {0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};
static uint8_t select_header[] = {0x00, 0xA4, 0x04, 0x0C};

static uint8_t get_challenge[] = {0x00, 0x84, 0x00, 0x00, 0x08};

static uint8_t SW_success[] = {0x90, 0x00};

static const uint8_t jpeg_header[4] = {0xFF, 0xD8, 0xFF, 0xE0};
static const uint8_t jpeg2k_header[6] = {0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50};
static const uint8_t jpeg2k_cs_header[4] = {0xFF, 0x4F, 0xFF, 0x51};

static bool print_logs = true;
static bool use_secure_messaging = false;

size_t passy_asn1_length(uint8_t data[3]) {
    if(data[0] <= 0x7F) {
        return data[0];
    } else if(data[0] == 0x81) {
        return data[1];
    } else if(data[0] == 0x82) {
        return (data[1] << 8) | data[2];
    }
    return 0;
}

size_t passy_asn1_length_length(uint8_t data[3]) {
    if(data[0] <= 0x7F) {
        return 1;
    } else if(data[0] == 0x81) {
        return 2;
    } else if(data[0] == 0x82) {
        return 3;
    }
    return 0;
}

PassyReader* passy_reader_alloc(Passy* passy) {
    PassyReader* passy_reader = malloc(sizeof(PassyReader));
    memset(passy_reader, 0, sizeof(PassyReader));

    furi_assert(passy);
    passy_reader->passy = passy;
    passy_reader->DG1 = passy->DG1;
    passy_reader->COM = passy->COM;
    passy_reader->dg_header = passy->dg_header;
    passy_reader->tx_buffer = bit_buffer_alloc(PASSY_READER_MAX_BUFFER_SIZE);
    passy_reader->rx_buffer = bit_buffer_alloc(PASSY_READER_MAX_BUFFER_SIZE);

    char passport_number[PASSY_PASSPORT_NUMBER_MAX_LENGTH];
    memset(passport_number, 0, sizeof(passport_number));
    strlcpy(passport_number, passy->passport_number, PASSY_PASSPORT_NUMBER_MAX_LENGTH - 1);
    // strlcpy leaves at least 1 byte for check digit
    // the next byte is already '\0' because of the memset
    passport_number[strlen(passy->passport_number)] = passy_checksum(passy->passport_number);
    FURI_LOG_I(TAG, "Passport number: %s", passport_number);

    char date_of_birth[PASSY_DOB_MAX_LENGTH];
    memset(date_of_birth, 0, sizeof(date_of_birth));
    strlcpy(date_of_birth, passy->date_of_birth, PASSY_DOB_MAX_LENGTH - 1);
    date_of_birth[strlen(passy->date_of_birth)] = passy_checksum(passy->date_of_birth);
    FURI_LOG_I(TAG, "Date of birth: %s", date_of_birth);

    char date_of_expiry[PASSY_DOE_MAX_LENGTH];
    memset(date_of_expiry, 0, sizeof(date_of_expiry));
    strlcpy(date_of_expiry, passy->date_of_expiry, PASSY_DOE_MAX_LENGTH - 1);
    date_of_expiry[strlen(passy->date_of_expiry)] = passy_checksum(passy->date_of_expiry);
    FURI_LOG_I(TAG, "Date of expiry: %s", date_of_expiry);

    passy_reader->secure_messaging = passy_secure_messaging_alloc(
        (uint8_t*)passport_number, (uint8_t*)date_of_birth, (uint8_t*)date_of_expiry);

    return passy_reader;
}

void passy_reader_free(PassyReader* passy_reader) {
    furi_assert(passy_reader);
    bit_buffer_free(passy_reader->tx_buffer);
    bit_buffer_free(passy_reader->rx_buffer);
    if(passy_reader->secure_messaging) {
        passy_secure_messaging_free(passy_reader->secure_messaging);
    }
    free(passy_reader);
}

NfcCommand passy_reader_send(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;
    BitBuffer* tx_buffer = passy_reader->tx_buffer;
    BitBuffer* rx_buffer = passy_reader->rx_buffer;

    if(print_logs) {
        passy_log_bitbuffer(TAG, "Send APDU", tx_buffer);
    }

    if(use_secure_messaging) {
        passy_secure_messaging_wrap_apdu(passy_reader->secure_messaging, passy_reader->tx_buffer);
    }

    if(strcmp(passy->proto, "4a") == 0) {
        Iso14443_4aPoller* iso14443_4a_poller = passy_reader->iso14443_4a_poller;
        Iso14443_4aError error;

        if(print_logs) {
            passy_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
        }
        error = iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4aErrorNone) {
            FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
            return NfcCommandStop;
        }
    } else if(strcmp(passy->proto, "4b") == 0) {
        Iso14443_4bPoller* iso14443_4b_poller = passy_reader->iso14443_4b_poller;
        Iso14443_4bError error;

        if(print_logs) {
            passy_log_bitbuffer(TAG, "NFC transmit", tx_buffer);
        }
        error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4bErrorNone) {
            FURI_LOG_W(TAG, "iso14443_4b_poller_send_block error %d", error);
            return NfcCommandStop;
        }
    } else {
        FURI_LOG_W(TAG, "Unknown protocol %s", passy->proto);
        return NfcCommandStop;
    }
    bit_buffer_reset(tx_buffer);
    if(print_logs) {
        passy_log_bitbuffer(TAG, "NFC response", rx_buffer);
    }

    // Check SW
    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    if(length < 2) {
        FURI_LOG_W(TAG, "Invalid response length %d", length);
        return NfcCommandStop;
    }
    passy_reader->last_sw = (data[length - 2] << 8) | data[length - 1];
    if(memcmp(data + length - 2, SW_success, sizeof(SW_success)) != 0) {
        FURI_LOG_W(TAG, "Invalid SW %02x %02x", data[length - 2], data[length - 1]);
        return NfcCommandStop;
    }

    if(use_secure_messaging) {
        passy_secure_messaging_unwrap_rapdu(
            passy_reader->secure_messaging, passy_reader->rx_buffer);
    }

    return ret;
}

NfcCommand passy_reader_select_application(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;

    BitBuffer* tx_buffer = passy_reader->tx_buffer;

    bit_buffer_append_bytes(tx_buffer, select_header, sizeof(select_header));
    bit_buffer_append_byte(tx_buffer, sizeof(passport_aid));
    bit_buffer_append_bytes(tx_buffer, passport_aid, sizeof(passport_aid));
    ret = passy_reader_send(passy_reader);
    if(ret != NfcCommandContinue) {
        return ret;
    }

    return ret;
}

NfcCommand passy_reader_get_challenge(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;

    bit_buffer_append_bytes(passy_reader->tx_buffer, get_challenge, sizeof(get_challenge));
    ret = passy_reader_send(passy_reader);
    if(ret != NfcCommandContinue) {
        return ret;
    }

    const uint8_t* data = bit_buffer_get_data(passy_reader->rx_buffer);
    SecureMessaging* secure_messaging = passy_reader->secure_messaging;
    const uint8_t* rnd_icc = data;
    memcpy(secure_messaging->rndICC, rnd_icc, 8);

    return ret;
}

NfcCommand passy_reader_external_authenticate(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    BitBuffer* tx_buffer = passy_reader->tx_buffer;

    // TODO: move into secure_messaging
    SecureMessaging* secure_messaging = passy_reader->secure_messaging;
    uint8_t S[32];
    memset(S, 0, sizeof(S));
    uint8_t eifd[32];
    memcpy(S, secure_messaging->rndIFD, sizeof(secure_messaging->rndIFD));
    memcpy(
        S + sizeof(secure_messaging->rndIFD),
        secure_messaging->rndICC,
        sizeof(secure_messaging->rndICC));
    memcpy(
        S + sizeof(secure_messaging->rndIFD) + sizeof(secure_messaging->rndICC),
        secure_messaging->Kifd,
        sizeof(secure_messaging->Kifd));

    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));
    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    mbedtls_des3_set2key_enc(&ctx, secure_messaging->KENC);
    mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, sizeof(S), iv, S, eifd);
    mbedtls_des3_free(&ctx);

    passy_log_buffer(TAG, "S", S, sizeof(S));
    passy_log_buffer(TAG, "eifd", eifd, sizeof(eifd));

    uint8_t mifd[8];
    passy_mac(secure_messaging->KMAC, eifd, sizeof(eifd), mifd, false);
    passy_log_buffer(TAG, "mifd", mifd, sizeof(mifd));

    uint8_t authenticate_header[] = {0x00, 0x82, 0x00, 0x00};

    bit_buffer_append_bytes(tx_buffer, authenticate_header, sizeof(authenticate_header));
    bit_buffer_append_byte(tx_buffer, sizeof(eifd) + sizeof(mifd));
    bit_buffer_append_bytes(tx_buffer, eifd, sizeof(eifd));
    bit_buffer_append_bytes(tx_buffer, mifd, sizeof(mifd));
    bit_buffer_append_byte(tx_buffer, 0); // Le

    ret = passy_reader_send(passy_reader);
    if(ret != NfcCommandContinue) {
        return ret;
    }

    const uint8_t* data = bit_buffer_get_data(passy_reader->rx_buffer);
    size_t length = bit_buffer_get_size_bytes(passy_reader->rx_buffer);
    const uint8_t* mac = data + length - 2 - 8;
    uint8_t calculated_mac[8];
    passy_mac(secure_messaging->KMAC, (uint8_t*)data, length - 8 - 2, calculated_mac, false);
    if(memcmp(mac, calculated_mac, sizeof(calculated_mac)) != 0) {
        FURI_LOG_W(TAG, "Invalid MAC");
        return NfcCommandStop;
    }

    uint8_t decrypted[32];
    do {
        uint8_t iv[8];
        memset(iv, 0, sizeof(iv));

        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_dec(&ctx, secure_messaging->KENC);
        mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, length - 2 - 8, iv, data, decrypted);
        mbedtls_des3_free(&ctx);
    } while(false);

    if(print_logs) {
        passy_log_buffer(TAG, "decrypted", decrypted, sizeof(decrypted));
    }

    uint8_t* rnd_icc = decrypted;
    uint8_t* rnd_ifd = decrypted + 8;
    uint8_t* Kicc = decrypted + 16;

    if(memcmp(rnd_icc, secure_messaging->rndICC, sizeof(secure_messaging->rndICC)) != 0) {
        FURI_LOG_W(TAG, "Invalid rndICC");
        return NfcCommandStop;
    }

    memcpy(secure_messaging->Kicc, Kicc, sizeof(secure_messaging->Kicc));
    memcpy(secure_messaging->SSC + 0, rnd_icc + 4, 4);
    memcpy(secure_messaging->SSC + 4, rnd_ifd + 4, 4);

    return ret;
}

NfcCommand passy_reader_select_file(PassyReader* passy_reader, uint16_t file_id) {
    NfcCommand ret = NfcCommandContinue;

    uint8_t select_0101[] = {0x00, 0xa4, 0x02, 0x0c, 0x02, 0x00, 0x00};
    select_0101[5] = (file_id >> 8) & 0xFF;
    select_0101[6] = file_id & 0xFF;

    bit_buffer_append_bytes(passy_reader->tx_buffer, select_0101, sizeof(select_0101));

    ret = passy_reader_send(passy_reader);
    if(ret != NfcCommandContinue) {
        return ret;
    }

    if(print_logs) {
        passy_log_bitbuffer(TAG, "NFC response (decrypted)", passy_reader->rx_buffer);
    }

    return ret;
}

NfcCommand passy_reader_read_binary(
    PassyReader* passy_reader,
    uint16_t offset,
    uint8_t Le,
    uint8_t* output_buffer) {
    NfcCommand ret = NfcCommandContinue;

    if(offset & 0x8000) {
        FURI_LOG_W(TAG, "Invalid offset %04x", offset);
    }
    uint8_t read_binary[] = {0x00, 0xB0, (offset >> 8) & 0xff, (offset >> 0) & 0xff, Le};

    bit_buffer_append_bytes(passy_reader->tx_buffer, read_binary, sizeof(read_binary));

    ret = passy_reader_send(passy_reader);
    if(ret != NfcCommandContinue) {
        return ret;
    }

    if(print_logs) {
        passy_log_bitbuffer(TAG, "NFC response (decrypted)", passy_reader->rx_buffer);
    }
    const uint8_t* decrypted_data = bit_buffer_get_data(passy_reader->rx_buffer);
    memcpy(output_buffer, decrypted_data, Le);

    return ret;
}

NfcCommand passy_reader_read_com(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;

    bit_buffer_reset(passy_reader->COM);
    uint8_t header[4];
    ret = passy_reader_read_binary(passy_reader, 0x00, sizeof(header), header);
    if(ret != NfcCommandContinue) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
        return ret;
    }
    size_t body_size = 1 + passy_asn1_length_length(header + 1) + passy_asn1_length(header + 1);
    uint8_t body_offset = sizeof(header);
    bit_buffer_append_bytes(passy_reader->COM, header, sizeof(header));
    do {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);
        uint8_t chunk[PASSY_READER_DG1_CHUNK_SIZE];
        uint8_t Le = MIN(sizeof(chunk), (size_t)(body_size - body_offset));

        ret = passy_reader_read_binary(passy_reader, body_offset, Le, chunk);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        bit_buffer_append_bytes(passy_reader->COM, chunk, Le);
        body_offset += Le;
    } while(body_offset < body_size);

    return ret;
}

NfcCommand passy_reader_read_dg1(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;
    bit_buffer_reset(passy->DG1);
    uint8_t header[4];
    ret = passy_reader_read_binary(passy_reader, 0x00, sizeof(header), header);
    if(ret != NfcCommandContinue) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);

        return ret;
    }
    size_t body_size = 1 + passy_asn1_length_length(header + 1) + passy_asn1_length(header + 1);
    uint8_t body_offset = sizeof(header);
    bit_buffer_append_bytes(passy_reader->DG1, header, sizeof(header));
    do {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);
        uint8_t chunk[PASSY_READER_DG1_CHUNK_SIZE];
        uint8_t Le = MIN(sizeof(chunk), (size_t)(body_size - body_offset));

        ret = passy_reader_read_binary(passy_reader, body_offset, Le, chunk);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        bit_buffer_append_bytes(passy_reader->DG1, chunk, Le);
        body_offset += Le;
    } while(body_offset < body_size);
    passy_log_bitbuffer(TAG, "DG1", passy_reader->DG1);

    return ret;
}

NfcCommand passy_reader_read_dg2_or_dg7(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;

    uint8_t header[PASSY_DG2_LOOKAHEAD];
    ret = passy_reader_read_binary(passy_reader, 0x00, sizeof(header) / 2, header);
    if(ret != NfcCommandContinue) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
        return ret;
    }
    // Issue with reading whole 120 bytes at once, so we read 60 bytes and then
    ret = passy_reader_read_binary(
        passy_reader, sizeof(header) / 2, sizeof(header) / 2, header + sizeof(header) / 2);
    if(ret != NfcCommandContinue) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
        return ret;
    }

    view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);

    size_t body_size = 1 + passy_asn1_length_length(header + 1) + passy_asn1_length(header + 1);
    FURI_LOG_I(TAG, "%s length: %d", passy->read_type == PassyReadDG2 ? "DG2" : "DG7", body_size);

    void* jpeg = memmem(header, sizeof(header), jpeg_header, sizeof(jpeg_header));
    void* jpeg2k = memmem(header, sizeof(header), jpeg2k_header, sizeof(jpeg2k_header));
    void* jpeg2k_cs = memmem(header, sizeof(header), jpeg2k_cs_header, sizeof(jpeg2k_cs_header));

    FuriString* path = furi_string_alloc();
    uint8_t start = 0;
    const char* dg_type = passy->read_type == PassyReadDG2 ? "DG2" : "DG7";
    char* dg_ext = ".bin";

    if(jpeg) {
        dg_ext = ".jpeg";
        start = (uint8_t*)jpeg - header;
    } else if(jpeg2k) {
        dg_ext = ".jp2";
        start = (uint8_t*)jpeg2k - header;
    } else if(jpeg2k_cs) {
        dg_ext = ".jpc";
        start = (uint8_t*)jpeg2k_cs - header;
    } else {
        dg_ext = ".bin";
        start = 0;
    }

    furi_string_printf(
        path, "%s/%s-%s%s", STORAGE_APP_DATA_PATH_PREFIX, passy->passport_number, dg_type, dg_ext);
    passy_furi_string_filename_safe(path);
    FURI_LOG_I(TAG, "Writing offset %d to %s", start, furi_string_get_cstr(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    file_stream_open(stream, furi_string_get_cstr(path), FSAM_WRITE, FSOM_OPEN_ALWAYS);

    uint8_t chunk[PASSY_READER_DG2_CHUNK_SIZE];
    passy->offset = start;
    passy->bytes_total = body_size;
    do {
        memset(chunk, 0, sizeof(chunk));
        uint8_t Le = MIN(sizeof(chunk), (size_t)(body_size - passy->offset));

        ret = passy_reader_read_binary(passy_reader, passy->offset, Le, chunk);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        passy->offset += Le;
        // passy_log_buffer(TAG, "chunk", chunk, sizeof(chunk));
        stream_write(stream, chunk, Le);
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);
    } while(passy->offset < body_size);

    file_stream_close(stream);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);

    return ret;
}

NfcCommand passy_reader_read_dg_generic(PassyReader* passy_reader) {
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;

    uint8_t header[4];
    ret = passy_reader_read_binary(passy_reader, 0x00, sizeof(header), header);
    if(ret != NfcCommandContinue) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
        return ret;
    }

    view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);

    size_t body_size = 1 + passy_asn1_length_length(header + 1) + passy_asn1_length(header + 1);
    int dg_number = passy->read_type & 0xFF;
    FURI_LOG_I(TAG, "DG%d length: %d", dg_number, body_size);

    FuriString* path = furi_string_alloc();
    furi_string_printf(
        path, "%s/%s-DG%d.bin", STORAGE_APP_DATA_PATH_PREFIX, passy->passport_number, dg_number);
    passy_furi_string_filename_safe(path);
    FURI_LOG_I(TAG, "Writing to %s", furi_string_get_cstr(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    file_stream_open(stream, furi_string_get_cstr(path), FSAM_WRITE, FSOM_OPEN_ALWAYS);

    stream_write(stream, header, sizeof(header));

    uint8_t chunk[PASSY_READER_DG1_CHUNK_SIZE];
    passy->offset = sizeof(header);
    passy->bytes_total = body_size;
    do {
        memset(chunk, 0, sizeof(chunk));
        uint8_t Le = MIN(sizeof(chunk), (size_t)(body_size - passy->offset));

        ret = passy_reader_read_binary(passy_reader, passy->offset, Le, chunk);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        passy->offset += Le;
        stream_write(stream, chunk, Le);
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderReading);
    } while(passy->offset < body_size);

    file_stream_close(stream);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);

    return ret;
}

NfcCommand passy_reader_state_machine(PassyReader* passy_reader) {
    furi_assert(passy_reader);
    Passy* passy = passy_reader->passy;
    NfcCommand ret = NfcCommandContinue;

    use_secure_messaging = false;

    do {
        ret = passy_reader_select_application(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        ret = passy_reader_get_challenge(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        ret = passy_reader_external_authenticate(passy_reader);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }
        FURI_LOG_I(TAG, "Mutual authentication success");
        passy_secure_messaging_calculate_session_keys(passy_reader->secure_messaging);
        use_secure_messaging = true;
        view_dispatcher_send_custom_event(
            passy->view_dispatcher, PassyCustomEventReaderAuthenticated);

        ret = passy_reader_select_file(passy_reader, passy->read_type);
        if(ret != NfcCommandContinue) {
            view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
            break;
        }

        if(passy->read_type == PassyReadCOM) {
            ret = passy_reader_read_com(passy_reader);
        } else if(passy->read_type == PassyReadDG1) {
            ret = passy_reader_read_dg1(passy_reader);
        } else if(passy->read_type == PassyReadDG2) {
            print_logs = false;
            ret = passy_reader_read_dg2_or_dg7(passy_reader);
            print_logs = true;
        } else if(passy->read_type == PassyReadDG7) {
            print_logs = false;
            ret = passy_reader_read_dg2_or_dg7(passy_reader);
            print_logs = true;
        } else {
            print_logs = false;
            ret = passy_reader_read_dg_generic(passy_reader);
            print_logs = true;
        }

        // Everything done
        ret = NfcCommandStop;
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderSuccess);
    } while(false);

    return ret;
}

NfcCommand passy_reader_poller_callback(NfcGenericEvent event, void* context) {
    PassyReader* passy_reader = context;
    NfcCommand ret = NfcCommandContinue;
    Passy* passy = passy_reader->passy;
    if(strcmp(passy->proto, "4a") == 0) {
        furi_assert(event.protocol == NfcProtocolIso14443_4a);
        const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
        Iso14443_4aPoller* iso14443_4a_poller = event.instance;

        FURI_LOG_D(TAG, "iso14443_4a_event->type %i", iso14443_4a_event->type);

        passy_reader->iso14443_4a_poller = iso14443_4a_poller;
        if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
            nfc_device_set_data(
                passy_reader->passy->nfc_device,
                NfcProtocolIso14443_4a,
                nfc_poller_get_data(passy_reader->passy->poller));

            ret = passy_reader_state_machine(passy_reader);

            furi_thread_set_current_priority(FuriThreadPriorityLowest);
        } else if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeError) {
            Iso14443_4aPollerEventData* data = iso14443_4a_event->data;
            Iso14443_4aError error = data->error;
            FURI_LOG_W(TAG, "Iso14443_4aError %i", error);
            switch(error) {
            case Iso14443_4aErrorNone:
                break;
            case Iso14443_4aErrorNotPresent:
                break;
            case Iso14443_4aErrorProtocol:
                ret = NfcCommandStop;
                break;
            case Iso14443_4aErrorTimeout:
                break;
            default:
                break;
            }
        }
    } else if(strcmp(passy->proto, "4b") == 0) {
        furi_assert(event.protocol == NfcProtocolIso14443_4b);
        const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;
        Iso14443_4bPoller* iso14443_4b_poller = event.instance;

        FURI_LOG_D(TAG, "iso14443_4b_event->type %i", iso14443_4b_event->type);

        passy_reader->iso14443_4b_poller = iso14443_4b_poller;

        if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
            nfc_device_set_data(
                passy_reader->passy->nfc_device,
                NfcProtocolIso14443_4b,
                nfc_poller_get_data(passy_reader->passy->poller));

            ret = passy_reader_state_machine(passy_reader);

            furi_thread_set_current_priority(FuriThreadPriorityLowest);
        } else if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeError) {
            Iso14443_4bPollerEventData* data = iso14443_4b_event->data;
            Iso14443_4bError error = data->error;
            FURI_LOG_W(TAG, "Iso14443_4bError %i", error);
            switch(error) {
            case Iso14443_4bErrorNone:
                break;
            case Iso14443_4bErrorNotPresent:
                break;
            case Iso14443_4bErrorProtocol:
                ret = NfcCommandStop;
                break;
            case Iso14443_4bErrorTimeout:
                break;
            }
        }
    } else {
        view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventReaderError);
    }

    return ret;
}
