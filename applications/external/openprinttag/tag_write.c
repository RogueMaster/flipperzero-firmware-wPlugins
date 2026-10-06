#include "openprinttag_i.h"

#include <nfc/helpers/iso13239_crc.h>
#include <toolbox/bit_buffer.h>

// Writes a prepared image (app->write_data) to the tag whose UID is in app->write_uid.
// Used by the update scene and by the scene that creates a new tag.

// Requests are addressed to the UID of the tag that was read, so a different tag in the field is
// ignored and a missing tag simply does not answer
#define WRITE_REQ_FLAGS                                                    \
    (ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI | \
     ISO15693_3_REQ_FLAG_T4_ADDRESSED)
// A tag can take up to ~20 ms to program a block before it answers a write command
#define WRITE_FWT_FC       (400000U)
#define WRITE_RETRIES      (3U)
#define WRITE_MAX_BLOCK    (32U)
#define WRITE_BUFFER_SIZE  (64U)
// Rounds in which the tag answered but the write still did not stick before giving up
#define WRITE_MAX_ATTEMPTS (5U)
// Pause between probes while waiting for the tag to be held near the Flipper
#define WRITE_WAIT_MS      (50U)

typedef enum {
    WriteBlockOk,
    WriteBlockNoTag, // The tag did not answer: not in the field (or a different one)
    WriteBlockFailed, // The tag answered but the block did not take the new data
    WriteBlockLocked, // The tag refused the write for good (locked or nonexistent block)
} WriteBlockResult;

// The SDK does not export the ISO15693-3 poller commands, so frames are sent through the NFC
// layer directly. This only works inside a callback of a poller started with nfc_poller_start_ex().
// Returns true if a response with a valid CRC was received (CRC is trimmed from rx).
static bool write_send_frame(Nfc* nfc, BitBuffer* tx, BitBuffer* rx, uint32_t fwt) {
    iso13239_crc_append(Iso13239CrcTypeDefault, tx);

    if(nfc_poller_trx(nfc, tx, rx, fwt) != NfcErrorNone) return false;
    if(!iso13239_crc_check(Iso13239CrcTypeDefault, rx)) return false;

    iso13239_crc_trim(rx);
    return true;
}

// Starts an addressed single-block request: flags, command, UID, block number
static void write_begin_request(OpenPrintTag* app, BitBuffer* tx, uint8_t command, uint8_t block) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, WRITE_REQ_FLAGS);
    bit_buffer_append_byte(tx, command);
    bit_buffer_append_bytes(tx, app->write_uid, ISO15693_3_UID_SIZE);
    bit_buffer_append_byte(tx, block);
}

// Reads one block with a raw addressed READ_SINGLE_BLOCK frame
static bool write_read_block(
    OpenPrintTag* app,
    BitBuffer* tx,
    BitBuffer* rx,
    uint8_t block,
    uint8_t* data,
    uint8_t block_size) {
    bit_buffer_reset(rx);
    write_begin_request(app, tx, ISO15693_3_CMD_READ_BLOCK, block);

    if(!write_send_frame(app->nfc, tx, rx, ISO15693_3_FDT_POLL_FC)) return false;

    // Response: flags + block data
    if(bit_buffer_get_size_bytes(rx) != 1U + block_size) return false;
    const uint8_t* rx_data = bit_buffer_get_data(rx);
    if(rx_data[0] & ISO15693_3_RESP_FLAG_ERROR) return false;

    memcpy(data, &rx_data[1], block_size);
    return true;
}

// Writes one block with a raw addressed WRITE_SINGLE_BLOCK frame and verifies it by reading it back
static WriteBlockResult write_write_block(
    OpenPrintTag* app,
    BitBuffer* tx,
    BitBuffer* rx,
    uint8_t block,
    const uint8_t* data,
    uint8_t block_size) {
    uint8_t current[WRITE_MAX_BLOCK];

    // The first read doubles as the "is the tag there" probe
    if(!write_read_block(app, tx, rx, block, current, block_size)) {
        return WriteBlockNoTag;
    }

    // Skip blocks that already hold the right data
    if(memcmp(current, data, block_size) == 0) {
        return WriteBlockOk;
    }

    for(uint32_t attempt = 0; attempt < WRITE_RETRIES; attempt++) {
        write_begin_request(app, tx, ISO15693_3_CMD_WRITE_BLOCK, block);
        bit_buffer_append_bytes(tx, data, block_size);
        bit_buffer_reset(rx);

        bool responded = write_send_frame(app->nfc, tx, rx, WRITE_FWT_FC);

        if(responded && bit_buffer_get_size_bytes(rx) >= 1) {
            const uint8_t* rx_data = bit_buffer_get_data(rx);
            if(rx_data[0] & ISO15693_3_RESP_FLAG_ERROR) {
                uint8_t code = bit_buffer_get_size_bytes(rx) > 1 ? rx_data[1] : 0;
                FURI_LOG_E(TAG, "Write block %d: tag error 0x%02X", block, code);
                // Retrying cannot help on locked or nonexistent blocks
                if(code == ISO15693_3_RESP_ERROR_BLOCK_LOCKED ||
                   code == ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE) {
                    return WriteBlockLocked;
                }
                continue;
            }
        }

        // Some tags program the block but answer late or not at all, so the read-back decides
        uint8_t verify[WRITE_MAX_BLOCK];
        if(write_read_block(app, tx, rx, block, verify, block_size) &&
           memcmp(verify, data, block_size) == 0) {
            return WriteBlockOk;
        }
        FURI_LOG_W(TAG, "Write block %d: attempt %lu failed", block, attempt + 1);
    }

    return WriteBlockFailed;
}

// Called for every poller tick until it returns NfcCommandStop. While the tag is not in the field
// it just keeps probing, so the user can edit the value away from the tag and hold it on later.
// A write that is cut off half way is picked up again on the next tick: blocks that already hold
// the new data are skipped.
NfcCommand openprinttag_tag_write_callback(NfcGenericEventEx event, void* context) {
    OpenPrintTag* app = context;

    // ISO15693-3 has no parent protocol, so the poller forwards the raw NFC events
    const NfcEvent* nfc_event = event.parent_event_data;
    if(nfc_event->type != NfcEventTypePollerReady) {
        return NfcCommandContinue;
    }

    const uint8_t block_size = app->write_data_size / app->write_block_count;
    BitBuffer* tx = bit_buffer_alloc(WRITE_BUFFER_SIZE);
    BitBuffer* rx = bit_buffer_alloc(WRITE_BUFFER_SIZE);

    WriteBlockResult result = WriteBlockOk;
    for(uint16_t i = 0; i < app->write_block_count; i++) {
        app->write_current_block = i;
        result = write_write_block(
            app, tx, rx, app->write_start_block + i, &app->write_data[i * block_size], block_size);
        if(result != WriteBlockOk) break;
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);

    if(result == WriteBlockOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, OpenPrintTagEventWriteDone);
        return NfcCommandStop;
    }

    if(result == WriteBlockFailed) {
        // The tag is there but the block does not take the data
        if(++app->write_attempts >= WRITE_MAX_ATTEMPTS) result = WriteBlockLocked;
    }

    if(result == WriteBlockLocked) {
        view_dispatcher_send_custom_event(app->view_dispatcher, OpenPrintTagEventWriteFailed);
        return NfcCommandStop;
    }

    // Tag not in the field (or cut off mid-write): wait a moment and probe again. The poller
    // overrides this with Stop as soon as the scene asks it to stop.
    furi_delay_ms(WRITE_WAIT_MS);
    return NfcCommandContinue;
}

OpenPrintTagReadCheck openprinttag_check_read(OpenPrintTag* app, NfcGenericCallback callback) {
    const Iso15693_3Data* data = nfc_poller_get_data(app->nfc_poller);
    if(iso15693_3_get_block_count(data) > 0 && iso15693_3_get_block_size(data) > 0) {
        return OpenPrintTagReadOk;
    }

    if(app->read_retries >= OPENPRINTTAG_READ_RETRIES) return OpenPrintTagReadFailed;
    app->read_retries++;

    // The poller was stopped by its callback, it still has to be stopped once before it is freed
    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);
    furi_delay_ms(OPENPRINTTAG_READ_RETRY_DELAY_MS);

    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    nfc_poller_start(app->nfc_poller, callback, app);
    return OpenPrintTagReadRetried;
}
