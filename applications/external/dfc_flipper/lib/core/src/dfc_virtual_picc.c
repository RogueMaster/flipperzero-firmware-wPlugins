#include "dfc_virtual_picc.h"
#include "dfc_ev2.h"

#define TAG "DfcVirtualPicc"

static const uint8_t DfcVirtualPiccProtocol[] = {0x02, 0x02};
// The activation a credential carrying no override of its own is presented with,
// used by cards that do not carry explicit activation overrides.
// anything recording what a credential presents records the same, so a stored
// record and the card a reader meets cannot describe different cards.
// The ATS is a genuine EV1's: TL=06 T0=75 (FSC=64) TA=77 TB=81 (FWI=8) TC=02
// hist=80. Readers gate on it. TA=77 invites PPS, which the transport answers.
static const uint8_t DfcVirtualPiccAts[] = {0x05, 0x65, 0x81, 0x02, 0x80};
static const uint8_t DfcVirtualPiccAtqa[] = {0x03, 0x44};
static const uint8_t DfcVirtualPiccRfDetail[] = {0x01, 0x51, 0x57};
#define DFC_VIRTUAL_PICC_SAK 0x20

#define DFC_ISO_DEP_RATS       0xE0
#define DFC_ISO_DEP_I_BLOCK    0x00
#define DFC_ISO_DEP_R_BLOCK    0x80
#define DFC_ISO_DEP_S_BLOCK    0xC0
#define DFC_ISO_DEP_BLOCK_MASK 0xC0
#define DFC_ISO_DEP_CID        0x08
#define DFC_ISO_DEP_NAD        0x04
#define DFC_ISO_DEP_MORE       0x10
#define DFC_ISO_DEP_R_NAK      0x10
#define DFC_ISO_DEP_MAX_INF    48

// The ATS this credential answers RATS with: its own if it carries one, the
// built-in default otherwise. A stored ATS reaches this point only after the
// admission check has confirmed its length octet describes it, so nothing here
// can emit a malformed answer.
static const uint8_t* picc_ats(const DfcCredential* credential, size_t* len) {
    if(credential->picc_ats_len > 0) {
        *len = credential->picc_ats_len;
        return credential->picc_ats;
    }
    *len = sizeof(DfcVirtualPiccAts);
    return DfcVirtualPiccAts;
}

static bool write_response(
    const uint8_t* data,
    size_t data_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(data_len > response_capacity) {
        return false;
    }

    memcpy(response, data, data_len);
    *response_len = data_len;
    return true;
}

static DfcVirtualPiccStatus write_status_word(
    uint8_t sw1,
    uint8_t sw2,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    const uint8_t data[] = {sw1, sw2};
    return write_response(data, sizeof(data), response, response_capacity, response_len) ?
               DfcVirtualPiccStatusOk :
               DfcVirtualPiccStatusBufferTooSmall;
}

#if DFC_ENABLE_VIRTUAL_CARD
enum {
    DfcVirtualCardSelectHeaderLength = 5,
    DfcVirtualCardSelectDataOffset = 5,
    DfcVirtualCardSelectLengthOffset = 4,
    DfcVirtualCardFciTag = 0x6F,
    DfcVirtualCardFciLength = 0x22,
    DfcVirtualCardDataTag = 0x85,
    DfcVirtualCardDataLength = 0x20,
    DfcVirtualCardClearInformationOffset = 0,
    DfcVirtualCardClearCapabilityOffset = 1,
    DfcVirtualCardClearUidOffset = 3,
    DfcVirtualCardExternalAuthenticateLength = 13,
    DfcVirtualCardExternalAuthenticateDataOffset = 5,
};

static DfcVirtualPiccStatus handle_virtual_card_select(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcCredential* credential = session->credential;
    if(!credential->virtual_card_configured ||
       !credential->virtual_card_authentication_mandatory ||
       command_len < DfcVirtualCardSelectHeaderLength)
        return DfcVirtualPiccStatusUnsupportedProtocol;

    size_t identifier_len = command[DfcVirtualCardSelectLengthOffset];
    if(command_len < DfcVirtualCardSelectDataOffset + identifier_len ||
       identifier_len != credential->virtual_card_installation_id_len ||
       memcmp(
           command + DfcVirtualCardSelectDataOffset,
           credential->virtual_card_installation_id,
           identifier_len) != 0)
        return DfcVirtualPiccStatusUnsupportedProtocol;

    const size_t selection_len =
        DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH;
    const size_t fci_len = 4 + selection_len + DFC_ISO7816_STATUS_WORD_LENGTH;
    if(response_capacity < fci_len) return DfcVirtualPiccStatusBufferTooSmall;

    DfcEmulator* emulator = session->emulator;
    dfc_random_fill(
        emulator->virtual_card_challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memset(
        emulator->virtual_card_clear_data, 0, DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    emulator->virtual_card_clear_data[DfcVirtualCardClearInformationOffset] =
        credential->virtual_card_information;
    memcpy(
        emulator->virtual_card_clear_data + DfcVirtualCardClearCapabilityOffset,
        credential->virtual_card_capabilities,
        DFC_VIRTUAL_CARD_CAPABILITY_LENGTH);
    memcpy(
        emulator->virtual_card_clear_data + DfcVirtualCardClearUidOffset,
        credential->virtual_card_uid,
        credential->virtual_card_uid_len);

    uint8_t clear[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
    memcpy(clear, emulator->virtual_card_challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(
        clear + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH,
        emulator->virtual_card_clear_data,
        DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t encrypted[sizeof(clear)];
    dfc_worker_aes_cbc_encrypt(
        credential->virtual_card_select_encryption_key,
        DFC_AES_KEY_LENGTH,
        iv,
        sizeof(clear),
        clear,
        encrypted);

    response[0] = DfcVirtualCardFciTag;
    response[1] = DfcVirtualCardFciLength;
    response[2] = DfcVirtualCardDataTag;
    response[3] = DfcVirtualCardDataLength;
    memcpy(response + 4, encrypted, sizeof(encrypted));
    response[4 + sizeof(encrypted)] = DFC_ISO7816_SW_OK_HI;
    response[5 + sizeof(encrypted)] = DFC_ISO7816_SW_OK_LO;
    *response_len = fci_len;
    emulator->virtual_card_authentication_expected = true;
    emulator->virtual_card_selection_pending = true;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus handle_virtual_card_external_authenticate(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcEmulator* emulator = session->emulator;
    if(!emulator->virtual_card_authentication_expected)
        return write_status_word(
            DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_HI,
            DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_LO,
            response,
            response_capacity,
            response_len);
    if(command_len != DfcVirtualCardExternalAuthenticateLength)
        return write_status_word(
            DFC_ISO7816_SW_WRONG_LENGTH_HI,
            DFC_ISO7816_SW_WRONG_LENGTH_LO,
            response,
            response_capacity,
            response_len);

    uint8_t input[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
    memcpy(input, emulator->virtual_card_challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(
        input + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH,
        emulator->virtual_card_clear_data,
        DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    uint8_t full_mac[DFC_AES_CMAC_LENGTH];
    uint8_t wire_mac[DFC_WIRE_MAC_LENGTH];
    if(aes_cmac(
           session->credential->virtual_card_select_mac_key,
           DFC_AES_KEY_LENGTH,
           input,
           sizeof(input),
           full_mac)) {
        for(size_t i = 0; i < DFC_WIRE_MAC_LENGTH; i++) wire_mac[i] = full_mac[(i * 2) + 1];
        emulator->virtual_card_selected =
            emulator->virtual_card_selection_pending &&
            memcmp(
                wire_mac,
                command + DfcVirtualCardExternalAuthenticateDataOffset,
                DFC_WIRE_MAC_LENGTH) == 0;
    }
    emulator->virtual_card_authentication_expected = false;
    emulator->virtual_card_selection_pending = false;
    return write_status_word(
        DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);
}
#endif

DfcVirtualPiccSession* dfc_virtual_picc_session_alloc(DfcCredential* credential) {
    if(!credential) return NULL;

    DfcVirtualPiccSession* session =
        dfc_platform_alloc(sizeof(DfcVirtualPiccSession), DfcAllocSession);
    if(!session) return NULL;
    memset(session, 0, sizeof(DfcVirtualPiccSession));
    session->credential = credential;
    session->emulator = dfc_emulator_alloc(credential);
    if(!session->emulator) {
        dfc_platform_free(session);
        return NULL;
    }
    return session;
}

void dfc_virtual_picc_session_free(DfcVirtualPiccSession* session) {
    if(!session) return;
    if(session->emulator) {
        dfc_emulator_free(session->emulator);
    }
    dfc_platform_free(session);
}

DfcVirtualPiccStatus dfc_virtual_picc_reset_protocol(DfcVirtualPiccSession* session) {
    if(!session || !session->emulator) return DfcVirtualPiccStatusProtocolError;

    dfc_emulator_reset_activation(session->emulator);
    session->iso_dep_selected = false;
    session->expected_pcd_sequence = 0;
    session->picc_sequence = 0;
    session->pending_response_len = 0;
    session->pending_response_offset = 0;
    session->pending_response_prefix_len = 0;
    session->iso_file_selected = false;
    session->iso_file_index = 0;
    return DfcVirtualPiccStatusOk;
}

DfcVirtualPiccStatus dfc_virtual_picc_field_off(DfcVirtualPiccSession* session) {
    DfcVirtualPiccStatus status = dfc_virtual_picc_reset_protocol(session);
    if(status != DfcVirtualPiccStatusOk) return status;

    session->activated = false;
    // The next physical activation generates a new random ID.
    session->random_uid_valid = false;
    memset(session->random_uid, 0, sizeof(session->random_uid));
    return DfcVirtualPiccStatusOk;
}

void dfc_virtual_picc_anticollision(
    const DfcCredential* credential,
    DfcVirtualPiccActivation* activation) {
    if(!credential || !activation) return;

    memset(activation, 0, sizeof(DfcVirtualPiccActivation));
    memcpy(activation->protocol, DfcVirtualPiccProtocol, sizeof(DfcVirtualPiccProtocol));
    activation->protocol_len = sizeof(DfcVirtualPiccProtocol);

    if(credential->picc_random_id) {
        // A fresh identifier per activation. The stored UID stays where it is and
        // is reachable only through GetCardUID under an authenticated session.
        activation->uid[0] = DFC_RANDOM_UID_FIRST_BYTE;
        dfc_random_fill(activation->uid + 1, DFC_RANDOM_UID_LEN - 1);
        activation->uid_len = DFC_RANDOM_UID_LEN;
    } else {
        memcpy(activation->uid, credential->uid, credential->uid_len);
        activation->uid_len = credential->uid_len;
    }

    size_t ats_len = 0;
    const uint8_t* ats = picc_ats(credential, &ats_len);
    memcpy(activation->ats, ats, ats_len);
    activation->ats_len = ats_len;

    activation->sak = credential->picc_has_sak ? credential->picc_sak : DFC_VIRTUAL_PICC_SAK;
    if(credential->picc_has_atqa) {
        memcpy(activation->atqa, credential->picc_atqa, sizeof(credential->picc_atqa));
    } else {
        memcpy(activation->atqa, DfcVirtualPiccAtqa, sizeof(DfcVirtualPiccAtqa));
    }
    activation->atqa_len = sizeof(DfcVirtualPiccAtqa);
    memcpy(activation->rf_detail, DfcVirtualPiccRfDetail, sizeof(DfcVirtualPiccRfDetail));
    activation->rf_detail_len = sizeof(DfcVirtualPiccRfDetail);
}

DfcVirtualPiccStatus dfc_virtual_picc_scan_iso14443a(
    DfcVirtualPiccSession* session,
    DfcVirtualPiccActivation* activation) {
    if(!session || !activation) return DfcVirtualPiccStatusProtocolError;
    if(!dfc_credential_uid_is_detectable(session->credential)) {
        return DfcVirtualPiccStatusNoCard;
    }

    dfc_virtual_picc_anticollision(session->credential, activation);
    if(session->credential->picc_random_id) {
        memcpy(session->random_uid, activation->uid, DFC_RANDOM_UID_LEN);
        session->random_uid_valid = true;
    } else {
        session->random_uid_valid = false;
    }
    session->activated = true;
    return DfcVirtualPiccStatusOk;
}

static bool is_rats(const uint8_t* frame, size_t frame_len) {
    return frame_len >= 2 && (frame[0] & 0xF0) == DFC_ISO_DEP_RATS;
}

static bool is_i_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == DFC_ISO_DEP_I_BLOCK;
}

static bool is_r_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == DFC_ISO_DEP_R_BLOCK;
}

static bool is_s_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == DFC_ISO_DEP_S_BLOCK;
}

static DfcVirtualPiccStatus build_iso_dep_i_block(
    DfcVirtualPiccSession* session,
    const uint8_t* inf,
    size_t inf_len,
    bool more,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(response_capacity < inf_len + 1) return DfcVirtualPiccStatusBufferTooSmall;
    response[0] = (uint8_t)(DFC_ISO_DEP_I_BLOCK | (session->picc_sequence & 0x01));
    if(more) response[0] |= DFC_ISO_DEP_MORE;
    if(inf_len > 0) {
        memcpy(response + 1, inf, inf_len);
    }
    *response_len = inf_len + 1;
    session->picc_sequence ^= 0x01;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus build_iso_dep_r_block(
    uint8_t sequence,
    bool nak,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(response_capacity < 1) return DfcVirtualPiccStatusBufferTooSmall;
    response[0] = (uint8_t)(DFC_ISO_DEP_R_BLOCK | (sequence & 0x01));
    if(nak) response[0] |= DFC_ISO_DEP_R_NAK;
    *response_len = 1;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus write_next_iso_dep_response_block(
    DfcVirtualPiccSession* session,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(session->pending_response_prefix_len > 0) {
        if(session->pending_response_offset >= session->pending_response_len) {
            if(response_capacity < session->pending_response_prefix_len)
                return DfcVirtualPiccStatusBufferTooSmall;
            memcpy(
                response, session->pending_response_prefix, session->pending_response_prefix_len);
            *response_len = session->pending_response_prefix_len;
            session->pending_response_prefix_len = 0;
            return DfcVirtualPiccStatusOk;
        }

        size_t remaining = session->pending_response_len - session->pending_response_offset;
        size_t chunk_len = remaining > DFC_ISO_DEP_MAX_INF ? DFC_ISO_DEP_MAX_INF : remaining;
        if(response_capacity < session->pending_response_prefix_len + chunk_len)
            return DfcVirtualPiccStatusBufferTooSmall;

        memcpy(response, session->pending_response_prefix, session->pending_response_prefix_len);
        memcpy(
            response + session->pending_response_prefix_len,
            session->pending_response + session->pending_response_offset,
            chunk_len);
        *response_len = session->pending_response_prefix_len + chunk_len;
        session->pending_response_offset += chunk_len;
        if(session->pending_response_offset >= session->pending_response_len) {
            session->pending_response_len = 0;
            session->pending_response_offset = 0;
            session->pending_response_prefix_len = 0;
        }
        return DfcVirtualPiccStatusOk;
    }

    if(session->pending_response_offset >= session->pending_response_len) {
        return build_iso_dep_i_block(
            session, NULL, 0, false, response, response_capacity, response_len);
    }

    size_t remaining = session->pending_response_len - session->pending_response_offset;
    size_t chunk_len = remaining > DFC_ISO_DEP_MAX_INF ? DFC_ISO_DEP_MAX_INF : remaining;
    bool more = (session->pending_response_offset + chunk_len) < session->pending_response_len;
    DfcVirtualPiccStatus status = build_iso_dep_i_block(
        session,
        session->pending_response + session->pending_response_offset,
        chunk_len,
        more,
        response,
        response_capacity,
        response_len);
    if(status != DfcVirtualPiccStatusOk) return status;
    session->pending_response_offset += chunk_len;
    if(!more) {
        session->pending_response_len = 0;
        session->pending_response_offset = 0;
    }
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus handle_iso_select(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    uint8_t p1 = command[2];
    uint8_t lc = (command_len > 4) ? command[4] : 0;
    const uint8_t* data = (lc > 0 && command_len >= (size_t)(5 + lc)) ? &command[5] : NULL;

    if(p1 == DFC_ISO7816_SELECT_PARENT && lc == 0) {
        session->iso_file_selected = false;
        return write_status_word(
            DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);
    }
    if((p1 == DFC_ISO7816_SELECT_PATH_FROM_MF || p1 == DFC_ISO7816_SELECT_PATH_FROM_DF)) {
        return write_status_word(
            DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
            DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
            response,
            response_capacity,
            response_len);
    }
    if(!data) {
        return write_status_word(
            DFC_ISO7816_SW_NOT_FOUND_HI,
            DFC_ISO7816_SW_NOT_FOUND_LO,
            response,
            response_capacity,
            response_len);
    }

    if(p1 == DFC_ISO7816_SELECT_BY_DF_NAME) {
        for(size_t i = 0; i < session->credential->num_apps; i++) {
            DfcApplication* app = &session->credential->apps[i];
            if(app->iso_aid_len > 0 && lc == app->iso_aid_len &&
               memcmp(data, app->iso_aid, lc) == 0) {
                dfc_emulator_reset_session(session->emulator);
                session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
                session->emulator->selected_app_index = i;
                session->iso_file_selected = false;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }

        if(lc == sizeof(DFC_ISO_AID) && memcmp(data, DFC_ISO_AID, lc) == 0) {
            dfc_emulator_reset_session(session->emulator);
            session->emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
            session->emulator->selected_app_index = 0;
            session->iso_file_selected = false;
            return write_status_word(
                DFC_ISO7816_SW_OK_HI,
                DFC_ISO7816_SW_OK_LO,
                response,
                response_capacity,
                response_len);
        }
    }

    if((p1 == DFC_ISO7816_SELECT_BY_FILE_ID || p1 == DFC_ISO7816_SELECT_CHILD_DF) &&
       lc == DFC_ISO7816_STATUS_WORD_LENGTH) {
        uint16_t file_id = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        if(file_id == DFC_ISO7816_MASTER_FILE_ID) {
            dfc_emulator_reset_session(session->emulator);
            session->emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
            session->emulator->selected_app_index = 0;
            session->iso_file_selected = false;
            return write_status_word(
                DFC_ISO7816_SW_OK_HI,
                DFC_ISO7816_SW_OK_LO,
                response,
                response_capacity,
                response_len);
        }
        for(size_t i = 0; i < session->credential->num_apps; i++) {
            DfcApplication* app = &session->credential->apps[i];
            if(app->has_iso_file_id && app->iso_file_id == file_id) {
                dfc_emulator_reset_session(session->emulator);
                session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
                session->emulator->selected_app_index = i;
                session->iso_file_selected = false;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }
    }

    if((p1 == DFC_ISO7816_SELECT_BY_FILE_ID || p1 == DFC_ISO7816_SELECT_CHILD_EF) &&
       lc == DFC_ISO7816_STATUS_WORD_LENGTH &&
       session->emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
        uint16_t file_id = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        for(size_t i = 0; i < session->credential->num_files; i++) {
            DfcFile* file = &session->credential->files[i];
            if(file->app_index == session->emulator->selected_app_index && file->has_iso_file_id &&
               file->iso_file_id == file_id) {
                session->iso_file_selected = true;
                session->iso_file_index = i;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }
    }

    return write_status_word(
        DFC_ISO7816_SW_NOT_FOUND_HI,
        DFC_ISO7816_SW_NOT_FOUND_LO,
        response,
        response_capacity,
        response_len);
}

static DfcVirtualPiccStatus handle_iso_file_command(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(!session->iso_file_selected || session->iso_file_index >= session->credential->num_files) {
        return write_status_word(
            DFC_ISO7816_SW_NOT_FOUND_HI,
            DFC_ISO7816_SW_NOT_FOUND_LO,
            response,
            response_capacity,
            response_len);
    }
    DfcFile* file = &session->credential->files[session->iso_file_index];
    uint8_t* data = dfc_file_data(session->credential, file);
#if DFC_ENABLE_SDM
    if(command[1] == DFC_ISO7816_INS_READ_BINARY && file->sdm_enabled) {
        if(!dfc_emulator_render_sdm_read(session->emulator, file)) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        data = session->emulator->sdm_read_cache;
    }
#endif

    if(command[1] == DFC_ISO7816_INS_READ_BINARY && command_len >= 5 &&
       (file->type == DFC_FILE_TYPE_STANDARD_DATA || file->type == DFC_FILE_TYPE_BACKUP_DATA)) {
        size_t offset = ((size_t)command[2] << 8) | command[3];
        if(offset > file->data_len) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        size_t requested = command[4] == 0 ? file->data_len - offset : command[4];
        if(requested > file->data_len - offset ||
           requested + DFC_ISO7816_STATUS_WORD_LENGTH > response_capacity) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        memcpy(response, data + offset, requested);
        response[requested] = DFC_ISO7816_SW_OK_HI;
        response[requested + 1] = DFC_ISO7816_SW_OK_LO;
        *response_len = requested + DFC_ISO7816_STATUS_WORD_LENGTH;
        return DfcVirtualPiccStatusOk;
    }

    if(command[1] == DFC_ISO7816_INS_UPDATE_BINARY && command_len >= 5 &&
       (file->type == DFC_FILE_TYPE_STANDARD_DATA || file->type == DFC_FILE_TYPE_BACKUP_DATA)) {
        size_t offset = ((size_t)command[2] << 8) | command[3];
        size_t write_len = command[4];
        if(command_len != 5 + write_len || offset > file->data_len ||
           write_len > file->data_len - offset) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        memcpy(data + offset, command + 5, write_len);
        dfc_credential_mark_dirty(session->credential);
        return write_status_word(
            DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);
    }

    if(command[1] == DFC_ISO7816_INS_READ_RECORD && command_len >= 5 &&
       (file->type == DFC_FILE_TYPE_LINEAR_RECORD || file->type == DFC_FILE_TYPE_CYCLIC_RECORD)) {
        size_t record_number = command[2];
        if(record_number == 0 || record_number > file->record_count ||
           file->record_size + DFC_ISO7816_STATUS_WORD_LENGTH > response_capacity) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        memcpy(response, data + (record_number - 1) * file->record_size, file->record_size);
        response[file->record_size] = DFC_ISO7816_SW_OK_HI;
        response[file->record_size + 1] = DFC_ISO7816_SW_OK_LO;
        *response_len = file->record_size + DFC_ISO7816_STATUS_WORD_LENGTH;
        return DfcVirtualPiccStatusOk;
    }

    return write_status_word(
        DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI,
        DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO,
        response,
        response_capacity,
        response_len);
}

static DfcVirtualPiccStatus handle_native_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    bool iso_wrapped,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    uint8_t clear_command[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t clear_command_len = 0;
    DfcEv2CommandSecurity security = dfc_ev2_prepare_command(
        session->emulator,
        command,
        command_len,
        clear_command,
        sizeof(clear_command),
        &clear_command_len);
    if(security == DfcEv2CommandInvalid) {
        const uint8_t integrity_error[] = {DFC_STATUS_INTEGRITY_ERROR};
        if(!iso_wrapped)
            return write_response(
                       integrity_error,
                       sizeof(integrity_error),
                       response,
                       response_capacity,
                       response_len) ?
                       DfcVirtualPiccStatusOk :
                       DfcVirtualPiccStatusBufferTooSmall;
        return dfc_wrap_native_response_as_iso7816(
                   integrity_error,
                   sizeof(integrity_error),
                   0,
                   response,
                   response_capacity,
                   response_len) ?
                   DfcVirtualPiccStatusOk :
                   DfcVirtualPiccStatusBufferTooSmall;
    }

    DfcByteBuf* tx = dfc_bytebuf_alloc(DFC_WORKER_MAX_BUFFER_SIZE);
    bool handled = dfc_emulator_handle_command(
        session->emulator, clear_command, clear_command_len, tx, NULL);
    DFC_UNUSED(handled);

    const uint8_t* tx_data = dfc_bytebuf_get_data(tx);
    size_t tx_len = dfc_bytebuf_get_size_bytes(tx);
    if(tx_len == 0) {
        dfc_bytebuf_free(tx);
        return DfcVirtualPiccStatusProtocolError;
    }

    uint8_t secured_response[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t secured_response_len = 0;
    bool protect_ev2_response = security == DfcEv2CommandSecured;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    protect_ev2_response = protect_ev2_response && !session->emulator->ev2_response_plain;
#endif
    if(protect_ev2_response) {
        if(!dfc_ev2_protect_response(
               session->emulator,
               clear_command,
               clear_command_len,
               tx_data,
               tx_len,
               secured_response,
               sizeof(secured_response),
               &secured_response_len)) {
            dfc_bytebuf_free(tx);
            return DfcVirtualPiccStatusProtocolError;
        }
        tx_data = secured_response;
        tx_len = secured_response_len;
    }

    DfcVirtualPiccStatus status = DfcVirtualPiccStatusOk;
    if(!iso_wrapped) {
        status = write_response(tx_data, tx_len, response, response_capacity, response_len) ?
                     DfcVirtualPiccStatusOk :
                     DfcVirtualPiccStatusBufferTooSmall;
    } else {
        status = dfc_wrap_native_response_as_iso7816(
                     tx_data, tx_len, 0, response, response_capacity, response_len) ?
                     DfcVirtualPiccStatusOk :
                     DfcVirtualPiccStatusBufferTooSmall;
    }

    dfc_bytebuf_free(tx);
    return status;
}

DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(!session || !command || !response || !response_len)
        return DfcVirtualPiccStatusProtocolError;
    if(!session->activated) return DfcVirtualPiccStatusNoCard;
    *response_len = 0;

    if(command_len >= 4 && command[0] == DFC_ISO7816_CLA_STANDARD) {
        if(command[1] == DFC_ISO7816_INS_SELECT) {
#if DFC_ENABLE_VIRTUAL_CARD
            DfcVirtualPiccStatus virtual_card_status = handle_virtual_card_select(
                session, command, command_len, response, response_capacity, response_len);
            if(virtual_card_status != DfcVirtualPiccStatusUnsupportedProtocol)
                return virtual_card_status;
#endif
            return handle_iso_select(
                session, command, command_len, response, response_capacity, response_len);
        }
#if DFC_ENABLE_VIRTUAL_CARD
        if(command[1] == DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE)
            return handle_virtual_card_external_authenticate(
                session, command, command_len, response, response_capacity, response_len);
#endif

        if(command[1] == DFC_ISO7816_INS_READ_BINARY ||
           command[1] == DFC_ISO7816_INS_UPDATE_BINARY ||
           command[1] == DFC_ISO7816_INS_READ_RECORD) {
            return handle_iso_file_command(
                session, command, command_len, response, response_capacity, response_len);
        }

        return write_status_word(
            DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI,
            DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO,
            response,
            response_capacity,
            response_len);
    }

    if(command_len >= 5 && command[0] == DFC_ISO7816_CLA_WRAPPER) {
        uint8_t lc = command[4];
        // Declared Lc must fit in the remaining CAPDU bytes (optional Le may follow).
        if(command_len < (size_t)(5 + lc)) {
            if(response_capacity < 2) return DfcVirtualPiccStatusBufferTooSmall;
            response[0] = 0x91;
            response[1] = DFC_STATUS_LENGTH_ERROR;
            *response_len = 2;
            return DfcVirtualPiccStatusOk;
        }
        uint8_t native_command[DFC_WORKER_MAX_BUFFER_SIZE];
        size_t native_len = 0;
        native_command[native_len++] = command[1];
        if(lc > 0) {
            if(native_len + lc > sizeof(native_command)) {
                return DfcVirtualPiccStatusBufferTooSmall;
            }
            memcpy(native_command + native_len, command + 5, lc);
            native_len += lc;
        }
        return handle_native_exchange(
            session, native_command, native_len, true, response, response_capacity, response_len);
    }

    return handle_native_exchange(
        session, command, command_len, false, response, response_capacity, response_len);
}

DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_frame_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(!session || !frame || !response || !response_len) return DfcVirtualPiccStatusProtocolError;
    if(!session->activated) return DfcVirtualPiccStatusNoCard;
    *response_len = 0;

    if(is_rats(frame, frame_len)) {
        size_t ats_len = 0;
        const uint8_t* ats = picc_ats(session->credential, &ats_len);
        if(response_capacity < ats_len) return DfcVirtualPiccStatusBufferTooSmall;
        memcpy(response, ats, ats_len);
        *response_len = ats_len;
        session->iso_dep_selected = true;
        session->expected_pcd_sequence = 0;
        session->picc_sequence = 0;
        session->pending_response_len = 0;
        session->pending_response_offset = 0;
        session->pending_response_prefix_len = 0;
        return DfcVirtualPiccStatusOk;
    }

    if(!session->iso_dep_selected || frame_len == 0) return DfcVirtualPiccStatusProtocolError;

    uint8_t pcb = frame[0];
    if(is_i_block(pcb)) {
        uint8_t sequence = (uint8_t)(pcb & 0x01);
        bool more = (pcb & DFC_ISO_DEP_MORE) != 0;
        size_t offset = 1;
        if(pcb & DFC_ISO_DEP_CID) offset++;
        if(pcb & DFC_ISO_DEP_NAD) offset++;
        if(frame_len < offset) return DfcVirtualPiccStatusProtocolError;

        const uint8_t* inf = frame + offset;
        size_t inf_len = frame_len - offset;

        if(sequence != session->expected_pcd_sequence) {
            return build_iso_dep_r_block(
                session->expected_pcd_sequence, true, response, response_capacity, response_len);
        }

        session->expected_pcd_sequence ^= 0x01;
        if(more) {
            return build_iso_dep_r_block(
                session->expected_pcd_sequence, false, response, response_capacity, response_len);
        }

        size_t apdu_response_len = 0;
        DfcVirtualPiccStatus status = dfc_virtual_picc_iso_dep_exchange(
            session,
            inf,
            inf_len,
            session->pending_response,
            sizeof(session->pending_response),
            &apdu_response_len);
        if(status != DfcVirtualPiccStatusOk) return status;
        session->pending_response_len = apdu_response_len;
        session->pending_response_offset = 0;
        if(offset > 1) {
            memcpy(session->pending_response_prefix, frame, offset);
            session->pending_response_prefix_len = offset;
        } else {
            session->pending_response_prefix_len = 0;
        }
        return write_next_iso_dep_response_block(
            session, response, response_capacity, response_len);
    }

    if(is_r_block(pcb)) {
        return write_next_iso_dep_response_block(
            session, response, response_capacity, response_len);
    }

    if(is_s_block(pcb)) {
        if(response_capacity < frame_len) return DfcVirtualPiccStatusBufferTooSmall;
        memcpy(response, frame, frame_len);
        if(response[0] == 0xC2) {
            response[0] = 0xE2;
            session->iso_dep_selected = false;
        } else {
            response[0] = (uint8_t)(response[0] | 0x20);
        }
        *response_len = frame_len;
        return DfcVirtualPiccStatusOk;
    }

    return DfcVirtualPiccStatusProtocolError;
}
