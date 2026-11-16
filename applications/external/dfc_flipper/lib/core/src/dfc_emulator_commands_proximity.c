#include "dfc_emulator_commands_i.h"

#if DFC_ENABLE_EMULATOR && DFC_ENABLE_PROXIMITY_CHECK

static void proximity_abort(DfcEmulator* emulator, DfcByteBuf* tx_buffer, uint8_t status) {
    emulator->proximity_active = false;
    emulator->proximity_offset = 0;
    dfc_secure_wipe(emulator->proximity_random, sizeof(emulator->proximity_random));
    dfc_secure_wipe(emulator->proximity_transcript, sizeof(emulator->proximity_transcript));
    emulator->proximity_transcript_len = 0;
    dfc_bytebuf_append_byte(tx_buffer, status);
}

static bool proximity_wire_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    uint8_t prefix,
    const DfcEmulator* emulator,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    if(!key || !output) return false;
    memset(output, 0, DFC_WIRE_MAC_LENGTH);
    uint8_t input[1 + sizeof(emulator->proximity_published) + DFC_PROXIMITY_TRANSCRIPT_MAX] = {0};
    size_t input_len = 0;
    input[input_len++] = prefix;
    memcpy(input + input_len, emulator->proximity_published, emulator->proximity_published_len);
    input_len += emulator->proximity_published_len;
    memcpy(input + input_len, emulator->proximity_transcript, emulator->proximity_transcript_len);
    input_len += emulator->proximity_transcript_len;

    bool ok = dfc_aes_wire_mac(key, input, input_len, output);
    dfc_secure_wipe(input, sizeof(input));
    return ok;
}

bool dfc_emulator_command_prepare_proximity_check(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    if(!credential->picc_has_proximity_key) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PROXIMITY_KEY_DISABLED);
        return true;
    }

    uint8_t proximity_random[DFC_PROXIMITY_RANDOM_LENGTH] = {0};
    if(!dfc_random_fill(proximity_random, sizeof(proximity_random))) {
        dfc_secure_wipe(proximity_random, sizeof(proximity_random));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        return false;
    }
    memcpy(emulator->proximity_random, proximity_random, sizeof(proximity_random));
    emulator->proximity_offset = 0;
    emulator->proximity_transcript_len = 0;
    emulator->proximity_published[0] = credential->picc_proximity_option;
    emulator->proximity_published[1] =
        (uint8_t)(credential->picc_proximity_published_response_time >> 8);
    emulator->proximity_published[2] = (uint8_t)credential->picc_proximity_published_response_time;
    emulator->proximity_published_len = 1 + DFC_PROXIMITY_PUBLISHED_TIME_LENGTH;
    if(credential->picc_has_proximity_bitrate) {
        emulator->proximity_published[emulator->proximity_published_len++] =
            credential->picc_proximity_bitrate;
    }
    emulator->proximity_active = true;
    emulator->proximity_verified = false;

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_SPECIAL_SUCCESS);
    dfc_bytebuf_append_bytes(
        tx_buffer, emulator->proximity_published, emulator->proximity_published_len);
    dfc_secure_wipe(proximity_random, sizeof(proximity_random));
    return true;
}

void dfc_emulator_command_proximity_check(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(!emulator->proximity_active) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }
    if(buffer_len < 2) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    size_t round_len = buffer[1];
    if(round_len == 0 || round_len > DFC_PROXIMITY_RANDOM_LENGTH || buffer_len != round_len + 2 ||
       emulator->proximity_offset + round_len > DFC_PROXIMITY_RANDOM_LENGTH ||
       emulator->proximity_transcript_len + round_len * 2 >
           sizeof(emulator->proximity_transcript)) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    const uint8_t* response = emulator->proximity_random + emulator->proximity_offset;
    memcpy(
        emulator->proximity_transcript + emulator->proximity_transcript_len, response, round_len);
    emulator->proximity_transcript_len += round_len;
    memcpy(
        emulator->proximity_transcript + emulator->proximity_transcript_len,
        buffer + 2,
        round_len);
    emulator->proximity_transcript_len += round_len;
    emulator->proximity_offset += round_len;

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_SPECIAL_SUCCESS);
    dfc_bytebuf_append_bytes(tx_buffer, response, round_len);
}

void dfc_emulator_command_verify_proximity_check(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(!emulator->credential->picc_has_proximity_key) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PROXIMITY_KEY_DISABLED);
        return;
    }
    if(!emulator->proximity_active) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }
    if(emulator->proximity_offset != DFC_PROXIMITY_RANDOM_LENGTH ||
       buffer_len != 1 + DFC_WIRE_MAC_LENGTH) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    uint8_t expected[DFC_WIRE_MAC_LENGTH] = {0};
    if(!proximity_wire_mac(
           emulator->credential->picc_proximity_key,
           DFC_CMD_VERIFY_PROXIMITY_CHECK,
           emulator,
           expected)) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }
    if(!dfc_ct_equal(expected, buffer + 1, sizeof(expected))) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }

    uint8_t response[DFC_WIRE_MAC_LENGTH] = {0};
    if(!proximity_wire_mac(
           emulator->credential->picc_proximity_key,
           DFC_STATUS_SPECIAL_SUCCESS,
           emulator,
           response)) {
        proximity_abort(emulator, tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup_response;
    }
    emulator->proximity_active = false;
    emulator->proximity_verified = true;
    emulator->proximity_offset = 0;
    emulator->proximity_transcript_len = 0;
    dfc_secure_wipe(emulator->proximity_random, sizeof(emulator->proximity_random));
    dfc_secure_wipe(emulator->proximity_transcript, sizeof(emulator->proximity_transcript));
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_SPECIAL_SUCCESS);
    dfc_bytebuf_append_bytes(tx_buffer, response, sizeof(response));
cleanup_response:
    dfc_secure_wipe(response, sizeof(response));
cleanup:
    dfc_secure_wipe(expected, sizeof(expected));
}

#endif
