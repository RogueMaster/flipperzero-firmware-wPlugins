#include "sam_api.h"
#include "sam_key_label.h"
#include "trace_log.h"
#include "uhf_snmp_probe.h"
#include "uhf_status_label.h"
#include <toolbox/path.h>
#include <toolbox/version.h>
#include <bit_lib/bit_lib.h>

// #define ASN1_DEBUG true

#define TAG "SAMAPI"

#define ASN1_PREFIX                     6
#define SEADER_ICLASS_SR_SIO_BASE_BLOCK 10
#define SEADER_SERIAL_FILE_NAME         "sam_serial"

const uint8_t picopass_iclass_key[] = {0xaf, 0xa7, 0x85, 0xa7, 0xda, 0xb3, 0x33, 0x78};
const uint8_t seader_oid[] =
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x02, 0x04};

static void seader_sam_set_state(
    Seader* seader,
    SeaderSamState state,
    SeaderSamIntent intent,
    SamCommand_PR command);

static void seader_publish_sam_status(Seader* seader) {
    if(seader && seader->view_dispatcher) {
        view_dispatcher_send_custom_event(
            seader->view_dispatcher, SeaderCustomEventSamStatusUpdated);
    }
}

static void seader_update_sam_key_label(Seader* seader, const uint8_t* value, size_t value_len) {
    if(!seader) {
        return;
    }

    seader_sam_key_label_format(
        seader->sam_present,
        value,
        value_len,
        seader->sam_key_label,
        sizeof(seader->sam_key_label));
    seader_publish_sam_status(seader);
}

static void seader_update_uhf_status_label(Seader* seader) {
    if(!seader) {
        return;
    }

    seader_uhf_status_label_format(
        seader->snmp_probe.has_monza4qt,
        seader->snmp_probe.monza4qt_key_present,
        seader->snmp_probe.has_higgs3,
        seader->snmp_probe.higgs3_key_present,
        seader->uhf_status_label,
        sizeof(seader->uhf_status_label));
    seader_publish_sam_status(seader);
}

static bool seader_snmp_probe_send_next_request(Seader* seader) {
    SeaderWorker* seader_worker = seader ? seader->worker : NULL;
    SeaderUartBridge* seader_uart = seader_worker ? seader_worker->uart : NULL;
    uint8_t* scratch = seader_uart ? (seader_uart->tx_buf + MAX_FRAME_HEADERS) : NULL;
    uint8_t* message = seader_uart ? seader_uart->rx_buf : NULL;
    size_t message_len = 0U;

    if(!seader || !scratch || !message) {
        return false;
    }

    if(!seader_uhf_snmp_probe_build_next_request(
           &seader->snmp_probe,
           scratch,
           SEADER_UART_RX_BUF_SIZE - MAX_FRAME_HEADERS,
           message,
           SEADER_UART_RX_BUF_SIZE,
           &message_len)) {
        return false;
    }

    return seader_worker_send_process_snmp_message(seader, message, message_len);
}

static void seader_snmp_probe_finish(Seader* seader) {
    if(!seader) {
        return;
    }

    seader_sam_set_state(seader, SeaderSamStateIdle, SeaderSamIntentNone, SamCommand_PR_NOTHING);
}

static void seader_start_snmp_probe(Seader* seader) {
    if(!seader || !seader->sam_present) {
        return;
    }

    seader_uhf_snmp_probe_init(&seader->snmp_probe);
    seader_update_uhf_status_label(seader);
    seader_sam_set_state(
        seader,
        SeaderSamStateCapabilityPending,
        SeaderSamIntentMaintenance,
        SamCommand_PR_processSNMPMessage);

    if(!seader_snmp_probe_send_next_request(seader)) {
        seader_snmp_probe_finish(seader);
    }
}

#ifdef ASN1_DEBUG
char asn1_log[SEADER_UART_RX_BUF_SIZE] = {0};
#endif

// Helper function to log hex data efficiently without large static buffer
static void
    seader_log_hex_data(const char* tag, const char* prefix, const uint8_t* data, size_t len) {
    if(len == 0) return;

    const size_t chunk_size =
        32; // Process 32 bytes at a time (64 chars + null terminator = 65 bytes on stack)
    char hex_chunk[chunk_size * 2 + 1];

    if(len <= chunk_size) {
        // Small data - single chunk
        for(size_t i = 0; i < len; i++) {
            snprintf(hex_chunk + (i * 2), sizeof(hex_chunk) - (i * 2), "%02x", data[i]);
        }
        hex_chunk[len * 2] = '\0';
        FURI_LOG_D(tag, "%s: %s", prefix, hex_chunk);
    } else {
        // Large data - process in chunks
        for(size_t offset = 0; offset < len; offset += chunk_size) {
            size_t current_chunk = (len - offset > chunk_size) ? chunk_size : (len - offset);
            for(size_t i = 0; i < current_chunk; i++) {
                snprintf(
                    hex_chunk + (i * 2), sizeof(hex_chunk) - (i * 2), "%02x", data[offset + i]);
            }
            hex_chunk[current_chunk * 2] = '\0';
            FURI_LOG_D(
                tag, "%s[%zu-%zu]: %s", prefix, offset, offset + current_chunk - 1, hex_chunk);
        }
    }
}

#ifdef SEADER_ENABLE_TRACE_LOG

static void seader_trace_mfc_packed_frame(const char* prefix, const uint8_t* buffer, size_t len) {
    if(!buffer || len == 0) {
        seader_trace(TAG, "%s <empty>", prefix);
        return;
    }

    if(len < 2) {
        seader_trace_hex(TAG, prefix, buffer, len);
        return;
    }

    uint8_t packed[SEADER_POLLER_MAX_BUFFER_SIZE] = {0};
    if(len > sizeof(packed)) {
        seader_trace_hex(TAG, prefix, buffer, len);
        return;
    }
    memcpy(packed, buffer, len);

    uint8_t parity = 0;
    size_t decoded_len = len - 1;
    uint8_t decoded[SEADER_POLLER_MAX_BUFFER_SIZE] = {0};
    char parity_bits[SEADER_POLLER_MAX_BUFFER_SIZE + 1] = {0};

    for(size_t i = 0; i < len; i++) {
        bit_lib_reverse_bits(packed + i, 0, 8);
    }

    for(size_t i = 0; i < decoded_len; i++) {
        bool val = bit_lib_get_bit(packed + i + 1, i);
        bit_lib_set_bit(&parity, i, val);
    }

    for(size_t i = 0; i < decoded_len; i++) {
        packed[i] = (packed[i] << i) | (packed[i + 1] >> (8 - i));
        bit_lib_reverse_bits(packed + i, 0, 8);
        decoded[i] = packed[i];
        parity_bits[i] = bit_lib_get_bit(&parity, i) ? '1' : '0';
    }
    parity_bits[decoded_len] = '\0';

    seader_trace_hex(TAG, prefix, buffer, len);
    seader_trace_hex(TAG, "mfc tx decoded", decoded, decoded_len);
    seader_trace(TAG, "mfc tx parity bits=%s", parity_bits);
}

static void
    seader_trace_mfc_bitbuffer(const char* prefix, BitBuffer* buffer, bool include_parity) {
    if(!buffer) {
        seader_trace(TAG, "%s <null>", prefix);
        return;
    }

    size_t len = bit_buffer_get_size_bytes(buffer);
    uint8_t bytes[SEADER_POLLER_MAX_BUFFER_SIZE] = {0};
    char parity_bits[SEADER_POLLER_MAX_BUFFER_SIZE + 1] = {0};

    if(len > sizeof(bytes)) len = sizeof(bytes);

    for(size_t i = 0; i < len; i++) {
        bytes[i] = bit_buffer_get_byte(buffer, i);
        if(include_parity) {
            const uint8_t* parity = bit_buffer_get_parity(buffer);
            parity_bits[i] = bit_lib_get_bit(parity, i) ? '1' : '0';
        }
    }

    if(include_parity) {
        parity_bits[len] = '\0';
    }

    seader_trace_hex(TAG, prefix, bytes, len);
    if(include_parity) {
        seader_trace(TAG, "%s parity=%s", prefix, parity_bits);
    }
}

#else

static void seader_trace_mfc_packed_frame(const char* prefix, const uint8_t* buffer, size_t len) {
    (void)prefix;
    (void)buffer;
    (void)len;
}

static void
    seader_trace_mfc_bitbuffer(const char* prefix, BitBuffer* buffer, bool include_parity) {
    (void)prefix;
    (void)buffer;
    (void)include_parity;
}

#endif

uint8_t updateBlock2[] = {RFAL_PICOPASS_CMD_UPDATE, 0x02};

uint8_t select_seos_app[] =
    {0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00};
uint8_t select_desfire_app_no_le[] =
    {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
uint8_t FILE_NOT_FOUND[] = {0x6a, 0x82};

void* calloc(size_t count, size_t size) {
    void* ptr = malloc(count * size);
    if(ptr) {
        memset(ptr, 0, count * size);
    }
    return ptr;
}

// Forward declarations
void seader_send_nfc_rx(Seader* seader, uint8_t* buffer, size_t len);
static void seader_abort_active_read(Seader* seader);

static void seader_sam_set_state(
    Seader* seader,
    SeaderSamState state,
    SeaderSamIntent intent,
    SamCommand_PR command) {
    seader->sam_state = state;
    seader->sam_intent = intent;
    seader->samCommand = command;
    seader_trace(TAG, "sam state=%d intent=%d cmd=%d", state, intent, command);
}

static SeaderSamIntent seader_sam_card_intent(const Seader* seader) {
    if(seader->credential->type == SeaderCredentialTypeConfig) {
        return SeaderSamIntentConfig;
    } else {
        return SeaderSamIntentReadPacs2;
    }
}

bool seader_sam_can_accept_card(const Seader* seader) {
    return seader->sam_state == SeaderSamStateIdle;
}

bool seader_sam_has_active_card(const Seader* seader) {
    return seader->sam_state == SeaderSamStateDetectPending ||
           seader->sam_state == SeaderSamStateConversation ||
           seader->sam_state == SeaderSamStateFinishing;
}

PicopassError seader_worker_fake_epurse_update(BitBuffer* tx_buffer, BitBuffer* rx_buffer) {
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    uint8_t fake_response[8];
    memset(fake_response, 0, sizeof(fake_response));
    memcpy(fake_response + 0, buffer + 6, 4);
    memcpy(fake_response + 4, buffer + 2, 4);

    bit_buffer_append_bytes(rx_buffer, fake_response, sizeof(fake_response));
    iso13239_crc_append(Iso13239CrcTypePicopass, rx_buffer);

    seader_log_hex_data(
        TAG,
        "Fake update E-Purse response",
        bit_buffer_get_data(rx_buffer),
        bit_buffer_get_size_bytes(rx_buffer));

    return PicopassErrorNone;
}

void seader_virtual_picopass_state_machine(Seader* seader, uint8_t* buffer, size_t len) {
    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    bit_buffer_append_bytes(tx_buffer, buffer, len);
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    uint8_t config[PICOPASS_BLOCK_LEN] = {0x12, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0xff, 0x3c};
    uint8_t sr_aia[PICOPASS_BLOCK_LEN] = {0xFF, 0xff, 0xff, 0xff, 0xFF, 0xFf, 0xff, 0xFF};
    uint8_t epurse[PICOPASS_BLOCK_LEN] = {0xff, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff};
    uint8_t pacs_sr_cfg[PICOPASS_BLOCK_LEN] = {0xA3, 0x03, 0x03, 0x03, 0x00, 0x03, 0xe0, 0x14};
    uint8_t zeroes[PICOPASS_BLOCK_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8_t tmac[4] = {};
    uint8_t cc_p[12] = {};
    uint8_t div_key[PICOPASS_BLOCK_LEN] = {};
    uint8_t offset; // for READ4

    do {
        switch(buffer[0]) {
        case RFAL_PICOPASS_CMD_READ_OR_IDENTIFY:
            if(buffer[1] == AIA_INDEX) {
                bit_buffer_append_bytes(rx_buffer, sr_aia, sizeof(sr_aia));
            } else if(buffer[1] == PACS_CFG_INDEX) {
                bit_buffer_append_bytes(rx_buffer, pacs_sr_cfg, sizeof(pacs_sr_cfg));
            } else { // What i've seen is 0c 12
                offset = buffer[1] - SEADER_ICLASS_SR_SIO_BASE_BLOCK;
                bit_buffer_append_bytes(
                    rx_buffer,
                    seader->credential->sio + (PICOPASS_BLOCK_LEN * offset),
                    PICOPASS_BLOCK_LEN);
            }
            iso13239_crc_append(Iso13239CrcTypePicopass, rx_buffer);
            break;
        case RFAL_PICOPASS_CMD_UPDATE:
            seader_worker_fake_epurse_update(tx_buffer, rx_buffer);
            break;
        case RFAL_PICOPASS_CMD_READCHECK_KD:
            if(buffer[1] == EPURSE_INDEX) {
                bit_buffer_append_bytes(rx_buffer, epurse, sizeof(epurse));
            }
            break;
        case RFAL_PICOPASS_CMD_CHECK:
            loclass_iclass_calc_div_key(
                seader->credential->diversifier, picopass_iclass_key, div_key, false);
            memcpy(cc_p, epurse, PICOPASS_BLOCK_LEN);
            memcpy(cc_p + 8, buffer + 1, PICOPASS_MAC_LEN);
            loclass_opt_doTagMAC(cc_p, div_key, tmac);
            bit_buffer_append_bytes(rx_buffer, tmac, sizeof(tmac));
            break;
        case RFAL_PICOPASS_CMD_READ4:
            if(buffer[1] < SEADER_ICLASS_SR_SIO_BASE_BLOCK) {
                if(buffer[1] == PACS_CFG_INDEX) {
                    bit_buffer_append_bytes(rx_buffer, pacs_sr_cfg, sizeof(pacs_sr_cfg));
                    bit_buffer_append_bytes(rx_buffer, zeroes, sizeof(zeroes));
                    bit_buffer_append_bytes(rx_buffer, zeroes, sizeof(zeroes));
                    bit_buffer_append_bytes(rx_buffer, zeroes, sizeof(zeroes));
                }
            } else {
                offset = buffer[1] - SEADER_ICLASS_SR_SIO_BASE_BLOCK;
                bit_buffer_append_bytes(
                    rx_buffer,
                    seader->credential->sio + (PICOPASS_BLOCK_LEN * offset),
                    PICOPASS_BLOCK_LEN * 4);
            }
            iso13239_crc_append(Iso13239CrcTypePicopass, rx_buffer);
            break;
        case RFAL_PICOPASS_CMD_PAGESEL:
            // this should be considered an attempt, but realisticly not working
            bit_buffer_append_bytes(rx_buffer, config, sizeof(config));
            iso13239_crc_append(Iso13239CrcTypePicopass, rx_buffer);
            break;
        }

        seader_send_nfc_rx(
            seader,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

bool seader_send_apdu(
    Seader* seader,
    uint8_t CLA,
    uint8_t INS,
    uint8_t P1,
    uint8_t P2,
    uint8_t* payload,
    uint8_t payloadLen,
    bool in_scratchpad) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    bool extended = seader_uart->T == 1;
    uint8_t header_len = extended ? 7 : 5;

    // Must account for MAX_FRAME_HEADERS headroom in scratchpad mode
    if(MAX_FRAME_HEADERS + header_len + payloadLen > SEADER_UART_RX_BUF_SIZE) {
        FURI_LOG_E(TAG, "Cannot send message, too long: %d", header_len + payloadLen);
        return false;
    }

    uint8_t length = header_len + payloadLen;
    uint8_t* apdu;
    bool must_free = false;
    uintptr_t tx_start = (uintptr_t)seader_uart->tx_buf;
    uintptr_t tx_end = tx_start + SEADER_UART_RX_BUF_SIZE;
    uintptr_t payload_addr = (uintptr_t)payload;
    bool scratchpad_payload = false;

    // in_scratchpad is only valid when the full payload range is inside tx_buf.
    if(in_scratchpad && payload_addr >= tx_start + header_len && payload_addr <= tx_end) {
        size_t available = (size_t)(tx_end - payload_addr);
        scratchpad_payload = payloadLen <= available;
    }

    if(scratchpad_payload) {
        apdu = (uint8_t*)(payload_addr - header_len);
    } else {
        apdu = malloc(length);
        if(!apdu) {
            FURI_LOG_E(TAG, "Failed to allocate memory for apdu in seader_send_apdu");
            return false;
        }
        memcpy(apdu + header_len, payload, payloadLen);
        must_free = true;
    }

    apdu[0] = CLA;
    apdu[1] = INS;
    apdu[2] = P1;
    apdu[3] = P2;

    if(extended) {
        apdu[4] = 0x00;
        apdu[5] = 0x00;
        apdu[6] = payloadLen;
    } else {
        apdu[4] = payloadLen;
    }

    seader_log_hex_data(TAG, "seader_send_apdu", apdu, length);

    if(seader_uart->T == 1) {
        seader_send_t1(seader_uart, apdu, length);
    } else {
        seader_ccid_XfrBlock(seader_uart, apdu, length);
    }

    if(must_free) {
        free(apdu);
    }

    return true;
}

#ifdef ASN1_DEBUG
static int seader_print_struct_callback(const void* buffer, size_t size, void* app_key) {
    if(app_key) {
        char* str = (char*)app_key;
        size_t next = strlen(str);
        strncpy(str + next, buffer, size);
    } else {
        uint8_t next = strlen(asn1_log);
        strncpy(asn1_log + next, buffer, size);
    }
    return 0;
}
#endif

void seader_send_payload(
    Seader* seader,
    Payload_t* payload,
    uint8_t from,
    uint8_t to,
    uint8_t replyTo) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    uint8_t* scratchpad = seader_uart->tx_buf + MAX_FRAME_HEADERS;
    size_t scratchpad_size = SEADER_UART_RX_BUF_SIZE - MAX_FRAME_HEADERS;
    size_t max_der_len = UINT8_MAX - ASN1_PREFIX;
    uint8_t* payload_buf = scratchpad;
    bool payload_in_scratchpad = true;

    asn_enc_rval_t er = der_encode_to_buffer(
        &asn_DEF_Payload, payload, scratchpad + ASN1_PREFIX, scratchpad_size - ASN1_PREFIX);

    if(er.encoded < 0 || ((size_t)er.encoded + ASN1_PREFIX) > UINT8_MAX) {
        payload_buf = malloc(ASN1_PREFIX + max_der_len);
        if(!payload_buf) {
            FURI_LOG_E(TAG, "Failed to allocate DER fallback buffer");
            return;
        }
        payload_in_scratchpad = false;

        er = der_encode_to_buffer(
            &asn_DEF_Payload, payload, payload_buf + ASN1_PREFIX, max_der_len);
    }

    if(er.encoded < 0) {
        FURI_LOG_E(TAG, "Failed to encode payload");
        if(!payload_in_scratchpad) {
            free(payload_buf);
        }
        return;
    }

    size_t apdu_payload_len = ASN1_PREFIX + (size_t)er.encoded;
    if(apdu_payload_len > UINT8_MAX) {
        FURI_LOG_E(TAG, "Encoded payload too large for APDU: %d", (int)apdu_payload_len);
        if(!payload_in_scratchpad) {
            free(payload_buf);
        }
        return;
    }

#ifdef ASN1_DEBUG
    if(er.encoded > -1) {
        char payloadDebug[384] = {0};
        memset(payloadDebug, 0, sizeof(payloadDebug));
        (&asn_DEF_Payload)
            ->op->print_struct(
                &asn_DEF_Payload, payload, 1, seader_print_struct_callback, payloadDebug);
        if(strlen(payloadDebug) > 0) {
            FURI_LOG_D(TAG, "Sending payload[%d %d %d]: %s", to, from, replyTo, payloadDebug);
        }
    } else {
        FURI_LOG_W(TAG, "Failed to print_struct payload");
    }
#endif
    //0xa0, 0xda, 0x02, 0x63, 0x00, 0x00, 0x0a,
    //0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x96, 0x00
    payload_buf[0] = from;
    payload_buf[1] = to;
    payload_buf[2] = replyTo;
    payload_buf[3] = 0x00;
    payload_buf[4] = 0x00;
    payload_buf[5] = 0x00;

    seader_send_apdu(
        seader,
        0xA0,
        0xDA,
        0x02,
        0x63,
        payload_buf,
        (uint8_t)apdu_payload_len,
        payload_in_scratchpad);

    if(!payload_in_scratchpad) {
        free(payload_buf);
    }
}

void seader_send_process_config_card(Seader* seader) {
    SamCommand_t samCommand = {0};
    Payload_t payload = {0};

    samCommand.present = SamCommand_PR_processConfigCard;
    seader_sam_set_state(
        seader, SeaderSamStateConversation, SeaderSamIntentConfig, samCommand.present);

    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;

    seader_send_payload(seader, &payload, 0x44, 0x0a, 0x44);
}

void seader_send_response(
    Seader* seader,
    Response_t* response,
    uint8_t from,
    uint8_t to,
    uint8_t replyTo) {
    Payload_t payload = {0};

    payload.present = Payload_PR_response;
    payload.choice.response = *response;

    seader_send_payload(seader, &payload, from, to, replyTo);
}

void seader_send_request_pacs2(Seader* seader) {
    OCTET_STRING_t oid = {
        .buf = (uint8_t*)seader_oid,
        .size = sizeof(seader_oid),
    };

    RequestPacs_t requestPacs = {0};
    requestPacs.contentElementTag = ContentElementTag_implicitFormatPhysicalAccessBits;
    requestPacs.oid = &oid;

    SamCommand_t samCommand = {0};
    samCommand.present = SamCommand_PR_requestPacs2;
    seader_sam_set_state(
        seader, SeaderSamStateConversation, SeaderSamIntentReadPacs2, samCommand.present);
    samCommand.choice.requestPacs2 = requestPacs;

    Payload_t payload = {0};
    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;

    seader_send_payload(
        seader, &payload, ExternalApplicationA, SAMInterface, ExternalApplicationA);
}

void seader_worker_send_serial_number(Seader* seader) {
    SamCommand_t samCommand = {0};
    samCommand.present = SamCommand_PR_serialNumber;
    seader_sam_set_state(
        seader, SeaderSamStateSerialPending, SeaderSamIntentMaintenance, samCommand.present);

    Payload_t payload = {0};
    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;

    seader_send_payload(
        seader, &payload, ExternalApplicationA, SAMInterface, ExternalApplicationA);
}

void seader_worker_send_version(Seader* seader) {
    SamCommand_t samCommand = {0};
    samCommand.present = SamCommand_PR_version;
    seader->sam_present = true;
    seader_update_sam_key_label(seader, NULL, 0U);
    seader_sam_set_state(
        seader, SeaderSamStateVersionPending, SeaderSamIntentMaintenance, samCommand.present);

    Payload_t payload = {0};
    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;

    seader_send_payload(
        seader, &payload, ExternalApplicationA, SAMInterface, ExternalApplicationA);
}

bool seader_worker_send_process_snmp_message(
    Seader* seader,
    const uint8_t* message,
    size_t message_len) {
    if(!seader || !message || message_len == 0U || message_len > UINT16_MAX) return false;

    SamCommand_t samCommand = {0};
    samCommand.present = SamCommand_PR_processSNMPMessage;
    samCommand.choice.processSNMPMessage.buf = (uint8_t*)message;
    samCommand.choice.processSNMPMessage.size = message_len;

    Payload_t payload = {0};
    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;

    seader_send_payload(
        seader, &payload, ExternalApplicationA, SAMInterface, ExternalApplicationA);
    return true;
}

void seader_send_card_detected(Seader* seader, CardDetails_t* cardDetails) {
    CardDetected_t cardDetected = {
        .detectedCardDetails = *cardDetails,
    };

    SamCommand_t samCommand = {0};
    samCommand.present = SamCommand_PR_cardDetected;
    samCommand.choice.cardDetected = cardDetected;

    Payload_t payload = {0};
    payload.present = Payload_PR_samCommand;
    payload.choice.samCommand = samCommand;
    seader_trace(
        TAG, "send cardDetected state=%d intent=%d", seader->sam_state, seader->sam_intent);

    seader_send_payload(
        seader, &payload, ExternalApplicationA, SAMInterface, ExternalApplicationA);
}

void seader_send_no_card_detected(Seader* seader) {
    furi_assert(seader);

    CardDetails_t cardDetails = {0};
    uint8_t protocol_bytes[] = {0x00, FrameProtocol_none};

    OCTET_STRING_fromBuf(
        &cardDetails.protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));
    OCTET_STRING_fromBuf(&cardDetails.csn, "", 0);

    seader_sam_set_state(
        seader, SeaderSamStateClearPending, SeaderSamIntentNone, SamCommand_PR_cardDetected);
    seader_trace(TAG, "send no-card cardDetected");
    seader_send_card_detected(seader, &cardDetails);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_CardDetails, &cardDetails);
}

static bool seader_store_pacs_bits(
    SeaderCredential* credential,
    const uint8_t* payload,
    size_t payload_size,
    uint8_t unused_bits) {
    if(!credential || !payload || payload_size == 0 ||
       payload_size > sizeof(credential->credential) || unused_bits > 7) {
        return false;
    }

    const uint8_t bit_length = payload_size * 8 - unused_bits;
    if(bit_length == 0) {
        return false;
    }

    uint64_t credential_val = 0;
    memcpy(&credential_val, payload, payload_size);
    credential_val = __builtin_bswap64(credential_val);

    credential->bit_length = bit_length;
    credential->credential = credential_val >> (64 - bit_length);
    return true;
}

static bool seader_unpack_pacs2_bits(Seader* seader, const OCTET_STRING_t* pacs_bits) {
    SeaderCredential* seader_credential = seader->credential;
    if(!pacs_bits || !pacs_bits->buf || pacs_bits->size < 2) {
        FURI_LOG_W(TAG, "Malformed pacs2 bits");
        return false;
    }

    seader_log_hex_data(TAG, "PACS2 bits", pacs_bits->buf, pacs_bits->size);

    if(seader_credential->sio[0] == 0x30) {
        seader_log_hex_data(TAG, "SIO", seader_credential->sio, seader_credential->sio_len);
#ifdef ASN1_DEBUG
        asn_dec_rval_t rval;
        SIO_t sio = {0};
        SIO_t* sio_p = &sio;
        rval = asn_decode(
            0,
            ATS_DER,
            &asn_DEF_SIO,
            (void**)&sio_p,
            seader_credential->sio,
            seader_credential->sio_len);

        if(rval.code == RC_OK) {
            FURI_LOG_D(TAG, "Decoded SIO");
            char sioDebug[384] = {0};
            (&asn_DEF_SIO)
                ->op->print_struct(&asn_DEF_SIO, &sio, 1, seader_print_struct_callback, sioDebug);
            if(strlen(sioDebug) > 0) {
                FURI_LOG_D(TAG, "SIO: %s", sioDebug);
            }
        } else {
            FURI_LOG_W(TAG, "Failed to decode SIO %d consumed", rval.consumed);
        }

        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SIO, &sio);
#endif
    }

    const uint8_t unused_bits = pacs_bits->buf[0];
    const uint8_t* payload = pacs_bits->buf + 1;
    const size_t payload_size = pacs_bits->size - 1;
    FURI_LOG_D(TAG, "PACS2 unused_bits=%u payload_size=%zu", unused_bits, payload_size);

    if(!seader_store_pacs_bits(seader_credential, payload, payload_size, unused_bits)) {
        FURI_LOG_W(TAG, "Failed to store PACS2 bits");
        return false;
    }

    FURI_LOG_D(
        TAG,
        "credential (%d) %016llx",
        seader_credential->bit_length,
        seader_credential->credential);

    return true;
}

//    800201298106683d052026b6820101
//300F800201298106683D052026B6820101
// ATR3:
//    800207358106793D81F9F385820104A51E8004000000018106053000000000820B323330353139313232395A830152
#define MAX_VERSION_SIZE 60
bool seader_parse_version(SeaderWorker* seader_worker, uint8_t* buf, size_t size) {
    bool rtn = false;
    if(size > MAX_VERSION_SIZE) {
        // Too large to handle now
        FURI_LOG_W(TAG, "Version of %d is too long to parse", size);
        return false;
    }
    SamVersion_t version = {0};
    SamVersion_t* version_p = &version;

    // Add sequence prefix
    uint8_t seq[MAX_VERSION_SIZE + 2] = {0x30};
    seq[1] = (uint8_t)size;
    memcpy(seq + 2, buf, size);

    asn_dec_rval_t rval =
        asn_decode(0, ATS_DER, &asn_DEF_SamVersion, (void**)&version_p, seq, size + 2);

    if(rval.code == RC_OK) {
#ifdef ASN1_DEBUG
        char versionDebug[128] = {0};
        (&asn_DEF_SamVersion)
            ->op->print_struct(
                &asn_DEF_SamVersion, &version, 1, seader_print_struct_callback, versionDebug);
        if(strlen(versionDebug) > 0) {
            FURI_LOG_D(TAG, "Received version: %s", versionDebug);
        }
#endif
        if(version.version.size == 2) {
            memcpy(seader_worker->sam_version, version.version.buf, version.version.size);
            FURI_LOG_I(
                TAG,
                "SAM Version: %d.%d",
                seader_worker->sam_version[0],
                seader_worker->sam_version[1]);
        }

        rtn = true;
    } else {
        FURI_LOG_W(TAG, "Failed to decode SamVersion %d consumed, size %d", rval.consumed, size);
    }

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SamVersion, &version);
    return rtn;
}

bool seader_sam_save_serial(Seader* seader, uint8_t* buf, size_t size) {
    SeaderCredential* cred = seader->credential;

    const char* file_header = "SAM Serial Number";
    const uint32_t file_version = 1;
    bool use_load_path = true;
    bool saved = false;
    FlipperFormat* file = flipper_format_file_alloc(cred->storage);
    FuriString* temp_str;
    temp_str = furi_string_alloc();

    do {
        if(use_load_path && !furi_string_empty(cred->load_path)) {
            // Get directory name
            path_extract_dirname(furi_string_get_cstr(cred->load_path), temp_str);
            // Make path to file to save
            furi_string_cat_printf(temp_str, "/%s%s", SEADER_SERIAL_FILE_NAME, ".txt");
        } else {
            furi_string_printf(
                temp_str, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, SEADER_SERIAL_FILE_NAME, ".txt");
        }
        // Open file
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(temp_str))) break;
        if(!flipper_format_write_header_cstr(file, file_header, file_version)) break;

        if(!flipper_format_write_hex(file, "Chip Serial Number", buf, size)) break;
        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(cred->dialogs, "Can not save\nserial file");
    }
    furi_string_free(temp_str);
    flipper_format_free(file);
    return saved;
}

bool seader_sam_save_serial_QR(Seader* seader, char* serial) {
    SeaderCredential* cred = seader->credential;

    const char* file_header = "QRCode";
    const uint32_t file_version = 0;

    bool saved = false;
    FlipperFormat* file = flipper_format_file_alloc(cred->storage);
    FuriString* temp_str;
    temp_str = furi_string_alloc();

    do {
        storage_simply_mkdir(cred->storage, EXT_PATH("qrcodes"));
        furi_string_printf(
            temp_str, "%s/%s%s", EXT_PATH("qrcodes"), "seader_sam_serial", ".qrcode");

        // Open file
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(temp_str))) break;
        if(!flipper_format_write_header_cstr(file, file_header, file_version)) break;

        if(!flipper_format_write_string_cstr(file, "Message", serial)) break;
        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(cred->dialogs, "Can not save\nQR file");
    }
    furi_string_free(temp_str);
    flipper_format_free(file);
    return saved;
}

bool seader_parse_serial_number(Seader* seader, uint8_t* buf, size_t size) {
    // Create hex string for QR code (needs to be persistent)
    char hex_string[size * 2 + 1];
    for(size_t i = 0; i < size; i++) {
        snprintf(hex_string + (i * 2), sizeof(hex_string) - (i * 2), "%02x", buf[i]);
    }
    hex_string[size * 2] = '\0';

    seader_log_hex_data(TAG, "Received serial", buf, size);

    seader_sam_save_serial_QR(seader, hex_string);
    return seader_sam_save_serial(seader, buf, size);
}

static void seader_abort_active_read(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    FURI_LOG_W(TAG, "Abort active read stage=%d sam=%d", seader_worker->stage, seader->samCommand);
    seader_trace(
        TAG,
        "abort stage=%d sam=%d state=%d intent=%d",
        seader_worker->stage,
        seader->samCommand,
        seader->sam_state,
        seader->sam_intent);
    seader_worker->stage = SeaderPollerEventTypeFail;
    if(!seader_sam_has_active_card(seader) && seader->sam_state != SeaderSamStateClearPending) {
        seader_sam_set_state(
            seader, SeaderSamStateIdle, SeaderSamIntentNone, SamCommand_PR_NOTHING);
    }
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
}

bool seader_parse_sam_response2(Seader* seader, SamResponse2_t* samResponse) {
    switch(samResponse->present) {
    case SamResponse2_PR_pacs:
        FURI_LOG_I(TAG, "samResponse2 SamResponse2_PR_pacs");
        if((seader->sam_state != SeaderSamStateConversation &&
            seader->sam_state != SeaderSamStateFinishing) ||
           seader->sam_intent != SeaderSamIntentReadPacs2) {
            FURI_LOG_W(
                TAG,
                "Unexpected pacs2 response in state=%d intent=%d",
                seader->sam_state,
                seader->sam_intent);
            seader_abort_active_read(seader);
            break;
        }
        Pacs2_t pacs2 = samResponse->choice.pacs;
        OCTET_STRING_t* pacs = pacs2.bits;

        seader->credential->has_pacs_media_type = pacs2.type != NULL;
        seader->credential->pacs_media_type = pacs2.type ? (SeaderPacsMediaType)(*pacs2.type) :
                                                           SeaderPacsMediaTypeUnknown;

        if(seader_unpack_pacs2_bits(seader, pacs)) {
            seader->worker->stage = SeaderPollerEventTypeComplete;
            seader_sam_set_state(
                seader, SeaderSamStateIdle, SeaderSamIntentNone, SamCommand_PR_NOTHING);
        } else {
            seader_abort_active_read(seader);
        }
        break;
    case SamResponse2_PR_NOTHING:
        FURI_LOG_I(TAG, "samResponse2 SamResponse2_PR_NOTHING");
        seader_abort_active_read(seader);
        break;
    default:
        FURI_LOG_I(TAG, "Unknown samResponse2 %d", samResponse->present);
        seader_abort_active_read(seader);
        break;
    }

    return false;
}

bool seader_parse_sam_response(Seader* seader, SamResponse_t* samResponse) {
    SeaderWorker* seader_worker = seader->worker;

    switch(seader->sam_state) {
    case SeaderSamStateConversation:
    case SeaderSamStateFinishing:
        if(seader->sam_intent == SeaderSamIntentConfig) {
            FURI_LOG_I(TAG, "samResponse config");
            seader_worker->stage = SeaderPollerEventTypeFail;
            seader_sam_set_state(
                seader, SeaderSamStateIdle, SeaderSamIntentNone, SamCommand_PR_NOTHING);
        } else {
            FURI_LOG_W(TAG, "Unexpected samResponse intent=%d", seader->sam_intent);
            seader_abort_active_read(seader);
        }
        break;
    case SeaderSamStateVersionPending:
        FURI_LOG_I(TAG, "samResponse version");
        seader_parse_version(seader_worker, samResponse->buf, samResponse->size);
        seader_worker_send_serial_number(seader);
        break;
    case SeaderSamStateSerialPending:
        FURI_LOG_I(TAG, "samResponse serial");
        seader_parse_serial_number(seader, samResponse->buf, samResponse->size);
        seader_start_snmp_probe(seader);
        break;
    case SeaderSamStateCapabilityPending:
        FURI_LOG_I(TAG, "samResponse processSNMPMessage");
        if(!seader_uhf_snmp_probe_consume_response(
               &seader->snmp_probe, samResponse->buf, samResponse->size)) {
            seader_update_sam_key_label(seader, NULL, 0U);
            seader_snmp_probe_finish(seader);
            break;
        }

        if(seader->snmp_probe.stage >= SeaderUhfSnmpProbeStageReadTagConfig) {
            seader_update_sam_key_label(
                seader, seader->snmp_probe.ice_value_storage, seader->snmp_probe.ice_value_len);
            seader_update_uhf_status_label(seader);
        }

        if(seader->snmp_probe.stage == SeaderUhfSnmpProbeStageDone) {
            seader_snmp_probe_finish(seader);
        } else if(
            seader->snmp_probe.stage == SeaderUhfSnmpProbeStageFailed ||
            !seader_snmp_probe_send_next_request(seader)) {
            seader_snmp_probe_finish(seader);
        }
        break;
    case SeaderSamStateDetectPending:
        FURI_LOG_I(TAG, "samResponse cardDetected");
        if(seader->sam_intent == SeaderSamIntentConfig) {
            seader_send_process_config_card(seader);
        } else if(seader->sam_intent == SeaderSamIntentReadPacs2) {
            seader_send_request_pacs2(seader);
        } else {
            FURI_LOG_W(TAG, "Unexpected detect intent=%d", seader->sam_intent);
            seader_abort_active_read(seader);
        }
        break;
    case SeaderSamStateClearPending:
        FURI_LOG_I(TAG, "samResponse clear-detected-card ack");
        seader_trace(TAG, "cardDetected ack clear stage=%d", seader_worker->stage);
        seader_sam_set_state(
            seader, SeaderSamStateIdle, SeaderSamIntentNone, SamCommand_PR_NOTHING);
        break;
    case SeaderSamStateIdle:
        FURI_LOG_W(TAG, "Unexpected samResponse while idle");
        seader_log_hex_data(TAG, "Unexpected samResponse", samResponse->buf, samResponse->size);
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled sam state %d", seader->sam_state);
        seader_abort_active_read(seader);
        break;
    }

    return false;
}

bool seader_parse_response(Seader* seader, Response_t* response) {
    switch(response->present) {
    case Response_PR_samResponse:
        seader_parse_sam_response(seader, &response->choice.samResponse);
        break;
    case Response_PR_samResponse2:
        seader_parse_sam_response2(seader, &response->choice.samResponse2);
        break;
    default:
        FURI_LOG_D(TAG, "non-sam response");
        break;
    };
    return false;
}

void seader_send_nfc_rx(Seader* seader, uint8_t* buffer, size_t len) {
    OCTET_STRING_t rxData = {.buf = buffer, .size = len};
    uint8_t status[] = {0x00, 0x00};
    RfStatus_t rfStatus = {.buf = status, .size = 2};

    NFCRx_t nfcRx = {0};
    nfcRx.rfStatus = rfStatus;
    nfcRx.data = &rxData;

    NFCResponse_t nfcResponse = {0};
    nfcResponse.present = NFCResponse_PR_nfcRx;
    nfcResponse.choice.nfcRx = nfcRx;

    Response_t response = {0};
    response.present = Response_PR_nfcResponse;
    response.choice.nfcResponse = nfcResponse;

    seader_send_response(seader, &response, NFCInterface, SAMInterface, 0x0);
}

void seader_capture_sio(BitBuffer* tx_buffer, BitBuffer* rx_buffer, SeaderCredential* credential) {
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    size_t len = bit_buffer_get_size_bytes(tx_buffer);
    const uint8_t* rxBuffer = bit_buffer_get_data(rx_buffer);

    if(credential->type == SeaderCredentialTypePicopass) {
        if(buffer[0] == RFAL_PICOPASS_CMD_READ_OR_IDENTIFY) {
            FURI_LOG_D(TAG, "Picopass Read1 block %02x", buffer[1]);
        }
        if(buffer[0] == RFAL_PICOPASS_CMD_READ4) {
            FURI_LOG_D(TAG, "Picopass Read4 block %02x", buffer[1]);
        }

        if(buffer[0] == RFAL_PICOPASS_CMD_READ4) {
            uint8_t block_num = buffer[1];
            if(credential->sio_len == 0 && rxBuffer[0] == 0x30) {
                credential->sio_start_block = block_num;
            }
            uint8_t offset = (block_num - credential->sio_start_block) * PICOPASS_BLOCK_LEN;
            memcpy(credential->sio + offset, rxBuffer, PICOPASS_BLOCK_LEN * 4);
            credential->sio_len += PICOPASS_BLOCK_LEN * 4;
        }
    } else if(credential->type == SeaderCredentialType14A) {
        // Desfire EV1 passes SIO in the clear
        // The desfire_read command is 13 bytes in total, but we deliberately don't check the read length as newer SAM
        // firmware versions read 5 bytes first to determine the length of the SIO from the ASN.1 tag length then do a
        // second read with just the required length to skip reading any additional bytes at the end of the file
        uint8_t desfire_read[] = {0x90, 0xbd, 0x00, 0x00, 0x07, 0x0f, 0x00, 0x00, 0x00};
        if(len == 13 && memcmp(buffer, desfire_read, sizeof(desfire_read)) == 0 &&
           rxBuffer[0] == 0x30) {
            size_t sio_len =
                bit_buffer_get_size_bytes(rx_buffer) - 2; // -2 for the APDU response bytes
            if(sio_len > sizeof(credential->sio)) {
                return;
            }
            credential->sio_len = sio_len;
            memcpy(credential->sio, rxBuffer, credential->sio_len);
        }
    }
}

void seader_iso15693_transmit(
    Seader* seader,
    PicopassPoller* picopass_poller,
    uint8_t* buffer,
    size_t len) {
    SeaderWorker* seader_worker = seader->worker;

    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    PicopassError error = PicopassErrorNone;

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        if(memcmp(buffer, updateBlock2, sizeof(updateBlock2)) == 0) {
            error = seader_worker_fake_epurse_update(tx_buffer, rx_buffer);
        } else {
            error = picopass_poller_send_frame(
                picopass_poller, tx_buffer, rx_buffer, SEADER_POLLER_MAX_FWT);
        }
        if(error == PicopassErrorIncorrectCrc) {
            error = PicopassErrorNone;
        }

        if(error != PicopassErrorNone) {
            seader_worker->stage = SeaderPollerEventTypeFail;
            break;
        }

        seader_capture_sio(tx_buffer, rx_buffer, seader->credential);
        seader_send_nfc_rx(
            seader,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

/* Assumes this is called in the context of the NFC API callback */
void seader_iso14443a_transmit(
    Seader* seader,
    Iso14443_4aPoller* iso14443_4a_poller,
    uint8_t* buffer,
    size_t len,
    uint16_t timeout,
    uint8_t format[3]) {
    UNUSED(timeout);
    UNUSED(format);

    furi_assert(seader);
    furi_assert(buffer);
    furi_assert(iso14443_4a_poller);
    SeaderWorker* seader_worker = seader->worker;
    SeaderCredential* credential = seader->credential;

    BitBuffer* tx_buffer =
        bit_buffer_alloc(len + 1); // extra byte to allow for appending a Le byte sometimes
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        if(seader->credential->isDesfireEV2 && sizeof(select_desfire_app_no_le) == len &&
           memcmp(buffer, select_desfire_app_no_le, len) == 0) {
            // If a DESFire EV2 card has previously sent a dodgy reply to a SELECT SeosApp
            // future SELECT DESFire commands with no Le byte (Ne == 0) fail with SW 6C00 (Wrong length Le)
            // If it has responded with a file not found (ie non-EV2 cards) to the SELECT SeosApp
            // then the SELECT DESFire without the Le byte is accepted fine.
            // No clue why this happens, but we have to deal with it annoyingly
            // We can't just always add the Le byte as this breaks OG D40 cards, so only do it when needed
            bit_buffer_append_byte(tx_buffer, 0x00); // Le byte of 0x00 is Ne 256
        }

        Iso14443_4aError error =
            iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4aErrorNone) {
            FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
            seader_worker->stage = SeaderPollerEventTypeFail;
            break;
        }

        // if the cAPDU was select seos app and the response starts with 6F228520
        // then this is almost certainly a dodgy response from a DESFire EV2 card
        // not a Seos card which old SAM firmware don't handle very well, so fake
        // a FILD_NOT_FOUND response instead of the real response
        if(sizeof(select_seos_app) == len && memcmp(buffer, select_seos_app, len) == 0 &&
           bit_buffer_get_size_bytes(rx_buffer) == 38) {
            const uint8_t ev2_select_reply_prefix[] = {0x6F, 0x22, 0x85, 0x20};
            const uint8_t* rapdu = bit_buffer_get_data(rx_buffer);
            if(memcmp(ev2_select_reply_prefix, rapdu, sizeof(ev2_select_reply_prefix)) == 0) {
                FURI_LOG_I(
                    TAG,
                    "Intercept DESFire EV2 reply to SELECT SeosApp and return File Not Found");
                seader->credential->isDesfireEV2 = true;
                bit_buffer_reset(rx_buffer);
                bit_buffer_append_bytes(rx_buffer, FILE_NOT_FOUND, sizeof(FILE_NOT_FOUND));
            }
        }

        seader_capture_sio(tx_buffer, rx_buffer, credential);
        seader_send_nfc_rx(
            seader,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

/* Assumes this is called in the context of the NFC API callback */
#define MF_CLASSIC_FWT_FC (60000)
void seader_mfc_transmit(
    Seader* seader,
    MfClassicPoller* mfc_poller,
    uint8_t* buffer,
    size_t len,
    uint16_t timeout,
    uint8_t format[3]) {
    UNUSED(timeout);

    furi_assert(seader);
    furi_assert(buffer);
    furi_assert(mfc_poller);
    SeaderWorker* seader_worker = seader->worker;

    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    do {
        seader_trace(
            TAG,
            "mfc tx format=%02x%02x%02x len=%u",
            format[0],
            format[1],
            format[2],
            (unsigned)len);
        if((format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x40) ||
           (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x24) ||
           (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x44)) {
            seader_trace_mfc_packed_frame("mfc tx raw", buffer, len);
        } else {
            seader_trace_hex(TAG, "mfc tx raw", buffer, len);
        }

        if(format[0] == 0x00 && format[1] == 0xC0 && format[2] == 0x00) {
            bit_buffer_append_bytes(tx_buffer, buffer, len);
            MfClassicError error =
                mf_classic_poller_send_frame(mfc_poller, tx_buffer, rx_buffer, MF_CLASSIC_FWT_FC);
            if(error != MfClassicErrorNone) {
                FURI_LOG_W(TAG, "mf_classic_poller_send_frame error %d", error);
                seader_trace(TAG, "mfc send_frame error=%d", error);
                seader_worker->stage = SeaderPollerEventTypeFail;
                break;
            }

            seader_trace_hex(
                TAG,
                "mfc rx raw",
                bit_buffer_get_data(rx_buffer),
                bit_buffer_get_size_bytes(rx_buffer));
        } else if(
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x40) ||
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x24) ||
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x44)) {
            seader_log_hex_data(TAG, "NFC Send with parity", buffer, len);

            // Only handles message up to 8 data bytes
            uint8_t tx_parity = 0;
            uint8_t len_without_parity = len - 1;

            // Don't forget to swap the bits of buffer[8]
            for(size_t i = 0; i < len; i++) {
                bit_lib_reverse_bits(buffer + i, 0, 8);
            }

            // Pull out parity bits
            for(size_t i = 0; i < len_without_parity; i++) {
                bool val = bit_lib_get_bit(buffer + i + 1, i);
                bit_lib_set_bit(&tx_parity, i, val);
            }

            for(size_t i = 0; i < len_without_parity; i++) {
                buffer[i] = (buffer[i] << i) | (buffer[i + 1] >> (8 - i));
            }
            bit_buffer_append_bytes(tx_buffer, buffer, len_without_parity);

            for(size_t i = 0; i < len_without_parity; i++) {
                bit_lib_reverse_bits(buffer + i, 0, 8);
                bit_buffer_set_byte_with_parity(
                    tx_buffer, i, buffer[i], bit_lib_get_bit(&tx_parity, i));
            }
            seader_trace_mfc_bitbuffer("mfc tx bitbuffer", tx_buffer, true);

            // Log the BitBuffer contents efficiently
            size_t tx_size = bit_buffer_get_size_bytes(tx_buffer);
            uint8_t* tx_data = malloc(tx_size);
            if(tx_data) {
                for(size_t i = 0; i < tx_size; i++) {
                    tx_data[i] = bit_buffer_get_byte(tx_buffer, i);
                }
                seader_log_hex_data(TAG, "NFC Send without parity", tx_data, tx_size);
                seader_trace_hex(TAG, "mfc tx no parity", tx_data, tx_size);
                free(tx_data);
            }

            MfClassicError error = mf_classic_poller_send_custom_parity_frame(
                mfc_poller, tx_buffer, rx_buffer, MF_CLASSIC_FWT_FC);
            if(error != MfClassicErrorNone) {
                FURI_LOG_W(TAG, "mf_classic_poller_send_encrypted_frame error %d", error);
                seader_trace(TAG, "mfc send_custom_parity error=%d", error);
                if(error == MfClassicErrorTimeout &&
                   seader->credential->type == SeaderCredentialTypeMifareClassic) {
                    snprintf(
                        seader->read_error,
                        sizeof(seader->read_error),
                        "Protected read timed out.\nNo supported data\nor wrong key.");
                }
                seader_worker->stage = SeaderPollerEventTypeFail;
                break;
            }

            size_t length = bit_buffer_get_size_bytes(rx_buffer);
            const uint8_t* rx_parity = bit_buffer_get_parity(rx_buffer);
            seader_trace_mfc_bitbuffer("mfc rx bitbuffer", rx_buffer, true);

            // Log the BitBuffer contents efficiently
            uint8_t* rx_data = malloc(length);
            if(rx_data) {
                for(size_t i = 0; i < length; i++) {
                    rx_data[i] = bit_buffer_get_byte(rx_buffer, i);
                }
                seader_log_hex_data(TAG, "NFC Response without parity", rx_data, length);
                seader_trace_hex(TAG, "mfc rx no parity", rx_data, length);
                free(rx_data);
            }

            uint8_t with_parity[SEADER_POLLER_MAX_BUFFER_SIZE];
            memset(with_parity, 0, sizeof(with_parity));

            for(size_t i = 0; i < length; i++) {
                uint8_t b = bit_buffer_get_byte(rx_buffer, i);
                bit_lib_reverse_bits(&b, 0, 8);
                bit_buffer_set_byte(rx_buffer, i, b);
            }

            length = length + (length / 8) + 1;

            uint8_t parts = 1 + length / 9;
            for(size_t p = 0; p < parts; p++) {
                uint8_t doffset = p * 9;
                uint8_t soffset = p * 8;

                for(size_t i = 0; i < 9; i++) {
                    with_parity[i + doffset] = bit_buffer_get_byte(rx_buffer, i + soffset) >> i;
                    if(i > 0) {
                        with_parity[i + doffset] |= bit_buffer_get_byte(rx_buffer, i + soffset - 1)
                                                    << (9 - i);
                    }

                    if(i > 0) {
                        bool val = bit_lib_get_bit(rx_parity, i - 1);
                        bit_lib_set_bit(with_parity + i, i - 1, val);
                    }
                }
            }

            for(size_t i = 0; i < length; i++) {
                bit_lib_reverse_bits(with_parity + i, 0, 8);
            }

            bit_buffer_copy_bytes(rx_buffer, with_parity, length);

            // Log the BitBuffer contents efficiently
            uint8_t* rx_data_parity = malloc(length);
            if(rx_data_parity) {
                for(size_t i = 0; i < length; i++) {
                    rx_data_parity[i] = bit_buffer_get_byte(rx_buffer, i);
                }
                seader_log_hex_data(TAG, "NFC Response with parity", rx_data_parity, length);
                seader_trace_hex(TAG, "mfc rx parity", rx_data_parity, length);
                free(rx_data_parity);
            }

        } else {
            FURI_LOG_W(TAG, "UNHANDLED FORMAT");
            seader_trace(
                TAG, "mfc unhandled format=%02x%02x%02x", format[0], format[1], format[2]);
        }

        seader_send_nfc_rx(
            seader,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

void seader_parse_nfc_command_transmit(
    Seader* seader,
    NFCSend_t* nfcSend,
    SeaderPollerContainer* spc) {
    long timeOut = nfcSend->timeOut;
    Protocol_t protocol = nfcSend->protocol;
    FrameProtocol_t frameProtocol = protocol.buf[1];

#ifdef ASN1_DEBUG
    seader_log_hex_data(TAG, "Transmit data", nfcSend->data.buf, nfcSend->data.size);
#endif

    if(seader->credential->type == SeaderCredentialTypeVirtual) {
        seader_virtual_picopass_state_machine(seader, nfcSend->data.buf, nfcSend->data.size);
    } else if(frameProtocol == FrameProtocol_iclass) {
        seader_iso15693_transmit(
            seader, spc->picopass_poller, nfcSend->data.buf, nfcSend->data.size);
    } else if(frameProtocol == FrameProtocol_nfc) {
        if(spc->iso14443_4a_poller) {
            seader_iso14443a_transmit(
                seader,
                spc->iso14443_4a_poller,
                nfcSend->data.buf,
                nfcSend->data.size,
                (uint16_t)timeOut,
                nfcSend->format->buf);
        } else if(spc->mfc_poller) {
            seader_mfc_transmit(
                seader,
                spc->mfc_poller,
                nfcSend->data.buf,
                nfcSend->data.size,
                (uint16_t)timeOut,
                nfcSend->format->buf);
        }
    } else {
        FURI_LOG_W(TAG, "unknown frame protocol %lx", frameProtocol);
    }
}

void seader_parse_nfc_off(Seader* seader) {
    FURI_LOG_D(TAG, "Set Field Off");
    seader_trace(TAG, "nfcOff state=%d intent=%d", seader->sam_state, seader->sam_intent);
    NFCResponse_t nfcResponse = {0};
    nfcResponse.present = NFCResponse_PR_nfcAck;

    Response_t response = {0};
    response.present = Response_PR_nfcResponse;
    response.choice.nfcResponse = nfcResponse;

    seader_send_response(seader, &response, ExternalApplicationA, SAMInterface, 0);
    if(seader->sam_state == SeaderSamStateConversation &&
       (seader->sam_intent == SeaderSamIntentReadPacs2 ||
        seader->sam_intent == SeaderSamIntentConfig)) {
        seader_sam_set_state(
            seader, SeaderSamStateFinishing, seader->sam_intent, seader->samCommand);
    }
}

void seader_parse_nfc_command(Seader* seader, NFCCommand_t* nfcCommand, SeaderPollerContainer* spc) {
    switch(nfcCommand->present) {
    case NFCCommand_PR_nfcSend:
        furi_assert(spc);
        seader_parse_nfc_command_transmit(seader, &nfcCommand->choice.nfcSend, spc);
        break;
    case NFCCommand_PR_nfcOff:
        seader_parse_nfc_off(seader);
        if(spc != NULL) {
            seader->worker->stage = SeaderPollerEventTypeComplete;
        }
        break;
    default:
        FURI_LOG_W(TAG, "unparsed NFCCommand");
        break;
    };
}

bool seader_worker_state_machine(
    Seader* seader,
    Payload_t* payload,
    bool online,
    SeaderPollerContainer* spc) {
    bool processed = false;

    switch(payload->present) {
    case Payload_PR_response:
        FURI_LOG_D(TAG, "Payload_PR_response");
        seader_parse_response(seader, &payload->choice.response);
        processed = true;
        break;
    case Payload_PR_nfcCommand:
        FURI_LOG_D(TAG, "Payload_PR_nfcCommand");
        if(online) {
            seader_parse_nfc_command(seader, &payload->choice.nfcCommand, spc);
            processed = true;
        } else if(payload->choice.nfcCommand.present == NFCCommand_PR_nfcOff) {
            seader_parse_nfc_command(seader, &payload->choice.nfcCommand, NULL);
            processed = true;
        } else {
            seader_trace(
                TAG,
                "defer offline nfcSend state=%d intent=%d",
                seader->sam_state,
                seader->sam_intent);
        }
        break;
    case Payload_PR_errorResponse:
        FURI_LOG_W(TAG, "Payload_PR_errorResponse");
        processed = true;
        if(seader->sam_state == SeaderSamStateCapabilityPending) {
            ErrorResponse_t* err = &payload->choice.errorResponse;
            if(seader_uhf_snmp_probe_consume_error(
                   &seader->snmp_probe, err->errorCode, err->data.buf, err->data.size)) {
                seader_update_uhf_status_label(seader);
                if(seader->snmp_probe.stage == SeaderUhfSnmpProbeStageDone) {
                    seader_snmp_probe_finish(seader);
                } else if(!seader_snmp_probe_send_next_request(seader)) {
                    seader_snmp_probe_finish(seader);
                }
            } else {
                seader_snmp_probe_finish(seader);
            }
        } else {
            view_dispatcher_send_custom_event(
                seader->view_dispatcher, SeaderCustomEventWorkerExit);
        }
        break;
    default:
        FURI_LOG_W(TAG, "unhandled payload");
        break;
    };

    return processed;
}

bool seader_process_success_response_i(
    Seader* seader,
    uint8_t* apdu,
    size_t len,
    bool online,
    SeaderPollerContainer* spc) {
    Payload_t payload = {0};
    Payload_t* payload_p = &payload;
    bool processed = false;

    asn_dec_rval_t rval =
        asn_decode(0, ATS_DER, &asn_DEF_Payload, (void**)&payload_p, apdu + 6, len - 6);
    if(rval.code == RC_OK) {
#ifdef ASN1_DEBUG
        if(online == false) {
            seader_log_hex_data(TAG, "incoming APDU", apdu + 6, len - 6);

            char payloadDebug[384] = {0};
            memset(payloadDebug, 0, sizeof(payloadDebug));
            (&asn_DEF_Payload)
                ->op->print_struct(
                    &asn_DEF_Payload, &payload, 1, seader_print_struct_callback, payloadDebug);
            if(strlen(payloadDebug) > 0) {
                FURI_LOG_D(TAG, "Received Payload: %s", payloadDebug);
            } else {
                FURI_LOG_D(TAG, "Received empty Payload");
            }
        } else {
            FURI_LOG_D(TAG, "Online mode");
        }
#endif

        processed = seader_worker_state_machine(seader, &payload, online, spc);
    } else {
        seader_log_hex_data(TAG, "Failed to decode APDU payload", apdu, len);
        seader_abort_active_read(seader);
    }

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_Payload, &payload);
    return processed;
}

NfcCommand seader_worker_card_detect(
    Seader* seader,
    uint8_t sak,
    uint8_t* atqa,
    const uint8_t* uid,
    uint8_t uid_len,
    uint8_t* ats,
    uint8_t ats_len) {
    UNUSED(atqa);
    SeaderCredential* credential = seader->credential;

    CardDetails_t cardDetails = {0};

    OCTET_STRING_fromBuf(&cardDetails.csn, (const char*)uid, uid_len);
    OCTET_STRING_t sak_string = {.buf = &sak, .size = 1};
    OCTET_STRING_t ats_string = {.buf = ats, .size = ats_len};
    uint8_t protocol_bytes[] = {0x00, 0x00};

    // this won't hold true for Seos cards, but then we won't see the SIO from Seos cards anyway
    // so it doesn't really matter
    memcpy(credential->diversifier, uid, uid_len);
    credential->diversifier_len = uid_len;

    if(ats != NULL) { // type 4
        protocol_bytes[1] = FrameProtocol_nfc;
        OCTET_STRING_fromBuf(
            &cardDetails.protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));
        cardDetails.sak = &sak_string;
        // TODO: Update asn1 to change atqa to ats
        cardDetails.atsOrAtqbOrAtr = &ats_string;
    } else if(uid_len == 8) { // picopass
        protocol_bytes[1] = FrameProtocol_iclass;
        OCTET_STRING_fromBuf(
            &cardDetails.protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));
    } else { // MFC
        protocol_bytes[1] = FrameProtocol_nfc;
        OCTET_STRING_fromBuf(
            &cardDetails.protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));
        cardDetails.sak = &sak_string;
    }

    seader_sam_set_state(
        seader,
        SeaderSamStateDetectPending,
        seader_sam_card_intent(seader),
        SamCommand_PR_cardDetected);
    seader_send_card_detected(seader, &cardDetails);
    // Print version information for app and firmware for later review in log
    const Version* version = version_get();
    FURI_LOG_I(
        TAG,
        "Firmware origin: %s firmware version: %s app version: %s",
        version_get_firmware_origin(version),
        version_get_version(version),
        FAP_VERSION);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_CardDetails, &cardDetails);
    return NfcCommandContinue;
}
