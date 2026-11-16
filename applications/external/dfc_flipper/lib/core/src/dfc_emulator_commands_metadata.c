#include "dfc_emulator_commands_i.h"

#if DFC_ENABLE_EMULATOR

static bool storage_version_code(const DfcCard* card, uint8_t* code) {
    // Match the legacy 8 KiB capacity of a zeroed, not yet configured card.
    uint32_t storage = card->storage ? card->storage : DFC_EV1_PICC_STORAGE_BYTES;
    // EV3 cards expose only the supported 2, 4, and 8 KiB storage classes.
    if(card->generation == DfcGenerationEv3 && storage != 2048 && storage != 4096 &&
       storage != 8192) {
        return false;
    }
    switch(storage) {
    case 1024:
        *code = 0x14;
        return true;
    case 2048:
        *code = 0x16;
        return true;
    case 4096:
        *code = 0x18;
        return true;
    case 8192:
        *code = 0x1A;
        return true;
    case 16384:
        *code = 0x1C;
        return true;
    case 32768:
        *code = 0x1E;
        return true;
    case 65536:
        *code = 0x20;
        return true;
    case 131072:
        *code = 0x22;
        return true;
    default:
        return false;
    }
}

static bool build_default_version_frame(const DfcCard* card, bool software, uint8_t frame[7]) {
    static const uint8_t version[3][2][2] = {
        {{0x01, 0x00}, {0x01, 0x03}},
        {{0x12, 0x00}, {0x02, 0x01}},
        {{0x33, 0x00}, {0x03, 0x00}},
    };
    size_t generation = 0;
    if(card->generation == DfcGenerationEv2) generation = 1;
    if(card->generation == DfcGenerationEv3) generation = 2;
    frame[0] = 0x04;
    frame[1] = 0x01;
    frame[2] = 0x01;
    frame[3] = version[generation][software ? 1 : 0][0];
    frame[4] = version[generation][software ? 1 : 0][1];
    if(!storage_version_code(card, &frame[5])) return false;
    frame[6] = 0x05;
    return true;
}

void dfc_emulator_command_get_version(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    uint8_t default_version[7];
    const DfcCard* card = &emulator->credential->card;
    dfc_emulator_command_clear_pending_chain(emulator);
    emulator->get_version_frame = 0;
    if(!card->has_hardware_version && !build_default_version_frame(card, false, default_version)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    const uint8_t* hardware_version = card->has_hardware_version ? card->hardware_version :
                                                                   default_version;
    emulator->get_version_frame = 1;
    dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
    dfc_bytebuf_append_bytes(tx_buffer, hardware_version, sizeof(default_version));
}

void dfc_emulator_command_get_version_continuation(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    static const uint8_t production_legacy[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x24};
    static const uint8_t production_ev3[] = {0x00, 0x00, 0x00, '0', '0', 0x01, 0x24};

    if(emulator->get_version_frame == 1) {
        uint8_t default_version[7];
        const DfcCard* card = &emulator->credential->card;
        if(!card->has_software_version &&
           !build_default_version_frame(card, true, default_version)) {
            emulator->get_version_frame = 0;
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        const uint8_t* software_version = card->has_software_version ? card->software_version :
                                                                       default_version;
        emulator->get_version_frame = 2;
        dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
        dfc_bytebuf_append_bytes(tx_buffer, software_version, sizeof(default_version));
        return;
    }

    if(emulator->get_version_frame == 2) {
        emulator->get_version_frame = 0;
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        if(emulator->credential->picc_random_id) {
            // Random-ID mode hides the real UID until authenticated GetCardUID.
            static const uint8_t zero_uid[DFC_DESFIRE_UID_LEN] = {0};
            dfc_bytebuf_append_bytes(tx_buffer, zero_uid, DFC_DESFIRE_UID_LEN);
        } else {
            dfc_bytebuf_append_bytes(tx_buffer, emulator->credential->uid, DFC_DESFIRE_UID_LEN);
        }
        const uint8_t* production = emulator->credential->card.generation == DfcGenerationEv3 ?
                                        production_ev3 :
                                        production_legacy;
        dfc_bytebuf_append_bytes(tx_buffer, production, sizeof(production_legacy));
        return;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
}

#if DFC_ENABLE_STATIC_SIGNATURE
void dfc_emulator_command_read_signature(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(emulator->credential->card.generation < DfcGenerationEv2) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }
    if(buffer_len != 2) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    if(buffer[1] != 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(!emulator->credential->picc_has_static_signature) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_SPECIAL_SUCCESS);
    dfc_bytebuf_append_bytes(
        tx_buffer, emulator->credential->picc_static_signature, DFC_STATIC_SIGNATURE_LENGTH);
}
#endif

#endif
