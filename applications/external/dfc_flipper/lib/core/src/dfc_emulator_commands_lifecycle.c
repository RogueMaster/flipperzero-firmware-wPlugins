#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"

#if DFC_ENABLE_EMULATOR

#define TAG                  DFC_EMULATOR_TAG
#define emulator_current_app dfc_emulator_current_app
#define emulator_num_keys    dfc_emulator_num_keys
#define read_uint24_le       dfc_emulator_read_uint24_le

#if DFC_ENABLE_SDM
enum {
    DfcSdmFieldUidOffset,
    DfcSdmFieldCounterOffset,
    DfcSdmFieldPiccDataOffset,
    DfcSdmFieldMacInputOffset,
    DfcSdmFieldEncryptedFileOffset,
    DfcSdmFieldEncryptedFileLength,
    DfcSdmFieldMacOffset,
    DfcSdmFieldCounterLimit,
    DfcSdmFieldCount,
};

static void set_sdm_field(DfcFile* file, size_t field, uint32_t value) {
    DfcSdmSettings* sdm = &file->state.data.sdm;
    switch(field) {
    case DfcSdmFieldUidOffset:
        sdm->uid_offset = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagUidOffset, true);
        break;
    case DfcSdmFieldCounterOffset:
        sdm->counter_offset = value;
        dfc_file_sdm_flag_set(
            file, DfcSdmFlagCounterOffset, value != DFC_SDM_HIDDEN_COUNTER_OFFSET);
        break;
    case DfcSdmFieldPiccDataOffset:
        sdm->picc_data_offset = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagPiccDataOffset, true);
        break;
    case DfcSdmFieldMacInputOffset:
        sdm->mac_input_offset = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagMacInputOffset, true);
        break;
    case DfcSdmFieldEncryptedFileOffset:
        sdm->encrypted_file_offset = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagEncryptedFile, true);
        break;
    case DfcSdmFieldEncryptedFileLength:
        sdm->encrypted_file_length = value;
        break;
    case DfcSdmFieldMacOffset:
        sdm->mac_offset = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagMacOffset, true);
        break;
    case DfcSdmFieldCounterLimit:
        sdm->counter_limit = value;
        dfc_file_sdm_flag_set(file, DfcSdmFlagCounterLimit, true);
        break;
    default:
        DFC_ASSERT(false);
        break;
    }
}

#endif

static bool require_create_delete_access(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    uint8_t settings = dfc_emulator_command_current_key_settings_1(emulator);
    if((settings & DFC_KS1_FREE_CREATE_DELETE) != 0 ||
       dfc_emulator_command_is_authenticated_as_master(emulator)) {
        return true;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
    return false;
}

void dfc_emulator_command_select_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer,
    Dfc* dfc) {
    const uint8_t* req_aid = apdu + 1;
    DfcApplication* app =
        dfc_credential_find_application_desfire_order(emulator->credential, req_aid);

    bool is_picc = req_aid[0] == 0x00 && req_aid[1] == 0x00 && req_aid[2] == 0x00;
    bool is_app = app != NULL;

    if(is_picc || is_app) {
        dfc_emulator_select_application(
            emulator,
            is_app ? DfcEmulatorSelectedApplicationApp : DfcEmulatorSelectedApplicationPicc,
            is_app ? dfc_credential_application_index(emulator->credential, app) : 0);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        if(dfc && is_app) {
            dfc_port_notify(dfc, DfcEventApplicationSelected);
        }
    } else {
        DFC_LOG_I(TAG, "SelectApplication: unknown AID");
        dfc_emulator_reset_session(emulator);
        emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
        emulator->selected_app_index = 0;
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_APPLICATION_NOT_FOUND);
    }
}

static bool require_picc_authentication(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc &&
       dfc_emulator_command_is_authenticated_as_master(emulator)) {
        return true;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
    return false;
}

static size_t allocated_file_bytes(const DfcCredential* credential) {
    size_t used = 0;
    for(size_t i = 0; i < credential->num_files; i++) {
        const DfcFile* file = &credential->files[i];
        if(file->type == DFC_FILE_TYPE_STANDARD_DATA || file->type == DFC_FILE_TYPE_BACKUP_DATA) {
            uint32_t declared_size;
            if(!dfc_file_get_declared_size(file, &declared_size)) return SIZE_MAX;
            used += declared_size;
        } else if(
            file->type == DFC_FILE_TYPE_LINEAR_RECORD ||
            file->type == DFC_FILE_TYPE_CYCLIC_RECORD) {
            used += (size_t)file->state.record.size * file->state.record.max_records;
        }
    }
    return used;
}

static size_t card_file_capacity_bytes(const DfcCard* card) {
    if(card->generation == DfcGenerationEv3 && card->storage == DfcStorage4KByteCount)
        return DFC_EV3_4K_FREE_MEMORY_BYTES;
    // A zeroed card has no configured storage. Keep the legacy EV1 fallback.
    return card->storage ? card->storage : DFC_EV1_PICC_STORAGE_BYTES;
}

static bool has_card_file_capacity(const DfcCredential* credential, size_t allocation) {
    size_t capacity = card_file_capacity_bytes(&credential->card);
    size_t used = allocated_file_bytes(credential);
    return used <= capacity && allocation <= capacity - used;
}

void dfc_emulator_command_free_mem(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    // Report the same logical capacity used to admit file allocations.
    size_t used = allocated_file_bytes(emulator->credential);
    size_t advertised_capacity = card_file_capacity_bytes(&emulator->credential->card);
    uint32_t free_bytes = 0;
    if(used >= advertised_capacity) {
        free_bytes = 0;
    } else {
        free_bytes = (uint32_t)(advertised_capacity - used);
    }
    uint8_t free_mem[3] = {
        (uint8_t)(free_bytes & 0xFF),
        (uint8_t)((free_bytes >> 8) & 0xFF),
        (uint8_t)((free_bytes >> 16) & 0xFF),
    };
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_bytes(tx_buffer, free_mem, sizeof(free_mem));
}

void dfc_emulator_command_df_names_continuation(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    const DfcCredential* credential = emulator->credential;
    size_t index = emulator->df_names_next;
    while(index < credential->num_apps && !credential->apps[index].has_iso_file_id)
        index++;
    if(index >= credential->num_apps) {
        dfc_emulator_command_clear_pending_chain(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        return;
    }
    size_t next = index + 1;
    while(next < credential->num_apps && !credential->apps[next].has_iso_file_id)
        next++;
    bool more = next < credential->num_apps;
    const DfcApplication* app = &credential->apps[index];
    dfc_bytebuf_append_byte(tx_buffer, more ? DFC_CMD_ADDITIONAL_FRAME : DFC_STATUS_OK);
    dfc_bytebuf_append_byte(tx_buffer, app->aid[2]);
    dfc_bytebuf_append_byte(tx_buffer, app->aid[1]);
    dfc_bytebuf_append_byte(tx_buffer, app->aid[0]);
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)(app->iso_file_id & 0xFF));
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)(app->iso_file_id >> 8));
    dfc_bytebuf_append_bytes(tx_buffer, app->iso_aid, app->iso_aid_len);
    emulator->df_names_pending = more;
    emulator->df_names_next = next;
}

void dfc_emulator_command_get_df_names(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    bool ev1 = dfc_emulator_native_authentication_active(emulator) &&
               dfc_secure_messaging_applies_ev1(emulator->secure_messaging, DFC_CMD_GET_DF_NAMES);
    // D40 does not add a response CMAC here. Every application still occupies
    // one frame, whether or not authentication is active.
    if(!ev1) {
        dfc_emulator_command_clear_pending_chain(emulator);
        emulator->df_names_next = 0;
        dfc_emulator_command_df_names_continuation(emulator, tx_buffer);
        return;
    }

    // Records contain the AID, the little-endian ISO file ID, and the DF name.
    // An application is listed when it carries an ISO file ID; the DF name is
    // whatever it has, and an application without one is listed all the same.
    uint8_t* records = emulator->pending_chain;
    uint8_t record_lengths[DFC_MAX_APPS];
    size_t record_count = 0;
    size_t records_len = 0;
    for(size_t i = 0; i < emulator->credential->num_apps; i++) {
        const DfcApplication* app = &emulator->credential->apps[i];
        if(!app->has_iso_file_id) continue;
        if(records_len + 5 + app->iso_aid_len + DFC_WORKER_CMAC_SIZE >
           sizeof(emulator->pending_chain))
            break;
        records[records_len++] = app->aid[2];
        records[records_len++] = app->aid[1];
        records[records_len++] = app->aid[0];
        records[records_len++] = (uint8_t)(app->iso_file_id & 0xFF);
        records[records_len++] = (uint8_t)(app->iso_file_id >> 8);
        memcpy(records + records_len, app->iso_aid, app->iso_aid_len);
        records_len += app->iso_aid_len;
        record_lengths[record_count++] = (uint8_t)(5 + app->iso_aid_len);
    }

    // Secure the complete logical response, then retain each application's
    // frame boundary. The final application's frame also carries the CMAC.
    size_t secured_len = dfc_secure_messaging_generate_ev1_response_in_place(
        emulator->secure_messaging,
        DFC_STATUS_OK,
        records,
        records_len,
        sizeof(emulator->pending_chain));
    if(secured_len == 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    emulator->response_secured = true;
    if(record_count <= 1) {
        dfc_emulator_command_append_status_payload(tx_buffer, DFC_STATUS_OK, records, secured_len);
        return;
    }

    dfc_emulator_command_clear_pending_chain(emulator);
    emulator->response_secured = true;
    size_t first_len = record_lengths[0];
    emulator->pending_chain_len = secured_len;
    emulator->pending_chain_offset = first_len;
    emulator->pending_chain_chunk_count = (uint8_t)(record_count - 1);
    emulator->pending_chain_chunk_index = 0;
    for(size_t index = 1; index < record_count; index++) {
        size_t chunk = record_lengths[index];
        if(index + 1 == record_count) chunk += secured_len - records_len;
        emulator->pending_chain_chunks[index - 1] = (uint8_t)chunk;
    }
    dfc_emulator_command_append_status_payload(
        tx_buffer, DFC_CMD_ADDITIONAL_FRAME, records, first_len);
}

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static void format_selected_application(DfcEmulator* emulator) {
    DfcCredential* credential = emulator->credential;
    size_t app_index = emulator->selected_app_index;

    // Delete backwards because releasing a file compacts both descriptor and
    // data pools. The application record and its key pool slice stay intact.
    for(size_t i = credential->num_files; i > 0; i--) {
        DfcFile* file = &credential->files[i - 1];
        if(file->app_index == app_index) {
            (void)dfc_credential_delete_file(credential, app_index, file->number);
        }
    }
}
#endif

void dfc_emulator_command_format_picc(
    DfcEmulator* emulator,
    size_t command_length,
    DfcByteBuf* tx_buffer) {
    if(command_length != 1) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    if(emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        DfcApplication* app = dfc_emulator_current_app(emulator);
        if(!app || !app->delegated) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        if(!dfc_emulator_command_is_authenticated_as_master(emulator)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
            return;
        }
        if(app->delegated_format_disabled) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }

        // Restore pending transactional writes while file offsets still match
        // the snapshot. Formatting can then compact storage safely.
        (void)dfc_emulator_abort_transaction(emulator);
        format_selected_application(emulator);
        uint16_t overhead = dfc_emulator_command_delegated_application_overhead_blocks(
            (uint8_t)app->num_keys, app->key_len);
        app->delegated_free_blocks =
            app->delegated_quota_limit >= overhead ? app->delegated_quota_limit - overhead : 0;
        dfc_credential_mark_dirty(emulator->credential);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
#else
        // Application-scoped formatting is unavailable without delegated
        // application support; ordinary applications always reject it.
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
#endif
        return;
    }

    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }

    if(!require_picc_authentication(emulator, tx_buffer)) return;
    if(emulator->credential->picc_format_disabled) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }

    (void)dfc_emulator_abort_transaction(emulator);
    dfc_credential_format_picc(emulator->credential);
    dfc_credential_mark_dirty(emulator->credential);
    dfc_emulator_retire_authentication(emulator);
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
    emulator->selected_app_index = 0;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_get_file_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    DfcCredential* credential = emulator->credential;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    for(size_t i = 0; i < credential->num_files; i++) {
        if(credential->files[i].app_index == emulator->selected_app_index) {
            dfc_bytebuf_append_byte(tx_buffer, credential->files[i].number);
        }
    }
}

void dfc_emulator_command_get_iso_file_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    DfcCredential* credential = emulator->credential;
    uint8_t fids[DFC_MAX_FILES * 2];
    size_t fids_len = 0;
    for(size_t i = 0; i < credential->num_files; i++) {
        DfcFile* file = &credential->files[i];
        if(file->app_index == emulator->selected_app_index && file->has_iso_file_id) {
            if(fids_len + 2 > sizeof(fids)) break;
            // LE 16-bit ISO file IDs (matches CreateStdDataFile optional FID).
            fids[fids_len++] = (uint8_t)(file->iso_file_id & 0xFF);
            fids[fids_len++] = (uint8_t)(file->iso_file_id >> 8);
        }
    }
    dfc_emulator_command_emit_payload_with_chaining(
        emulator, tx_buffer, DFC_CMD_GET_ISO_FILE_IDS, fids, fids_len, 2);
}

void dfc_emulator_command_get_file_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    uint8_t file_no = apdu[1];
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, file_no);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    if(file->type == 0x02) {
        dfc_bytebuf_append_byte(tx_buffer, file->type);
        dfc_bytebuf_append_byte(tx_buffer, file->comm_settings);
        uint8_t rights_wire[2];
        dfc_emulator_command_access_rights_to_wire(file->access_rights, rights_wire);
        dfc_bytebuf_append_bytes(tx_buffer, rights_wire, sizeof(rights_wire));
        dfc_emulator_command_append_int32_le(tx_buffer, file->state.value.lower_limit);
        dfc_emulator_command_append_int32_le(tx_buffer, file->state.value.upper_limit);
        // File settings expose the committed reusable limit, not the balance
        // and not a debit that is still pending in the current transaction.
        dfc_emulator_command_append_int32_le(tx_buffer, file->state.value.limited_credit_value);
        dfc_bytebuf_append_byte(tx_buffer, file->state.value.limited_credit);
    } else {
        uint8_t settings[7];
        dfc_encode_standard_data_file_settings(
            file->type, file->comm_settings, file->access_rights, file->data_len, settings);
        dfc_bytebuf_append_bytes(tx_buffer, settings, sizeof(settings));
    }
}

static void apply_change_file_settings(
    DfcEmulator* emulator,
    DfcFile* file,
    const uint8_t* body,
    size_t body_len,
    DfcByteBuf* tx_buffer) {
    size_t key_count = emulator_num_keys(emulator);

    if(body_len != 3) {
#if DFC_ENABLE_SDM
        if(body_len == 0 || (body[0] & DFC_SDM_ENABLED_MASK) == 0) {
#endif
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
#if DFC_ENABLE_SDM
        }
#endif
    }
    uint8_t comm_settings = body[0];
    uint16_t access_rights = dfc_emulator_command_access_rights_from_wire(body[1], body[2]);
#if DFC_ENABLE_SDM
    const uint8_t* settings = NULL;
    uint8_t sdm_options = 0;
    uint8_t sdm_fields = 0;
    uint16_t sdm_access_rights = 0;
    if((comm_settings & DFC_SDM_ENABLED_MASK) != 0) {
        if(body_len < 3 + DFC_SDM_BASE_SETTINGS_LENGTH) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        settings = body + 3;
        sdm_options = settings[DFC_SDM_OPTIONS_OFFSET];
        sdm_access_rights =
            (uint16_t)(settings[DFC_SDM_ACCESS_RIGHTS_OFFSET] |
                       ((uint16_t)settings[DFC_SDM_ACCESS_RIGHTS_OFFSET + 1] << 8));
        uint8_t meta_read = (uint8_t)(sdm_access_rights >> DFC_SDM_META_READ_SHIFT);
        uint8_t sdm_file_read =
            (uint8_t)((sdm_access_rights >> DFC_SDM_FILE_READ_SHIFT) & DFC_SDM_ACCESS_MASK);
        if(meta_read == DFC_SDM_FREE_ACCESS) {
            if((sdm_options & DFC_SDM_UID_MIRROR_MASK) != 0)
                sdm_fields |= 1u << DfcSdmFieldUidOffset;
            if((sdm_options & DFC_SDM_COUNTER_MIRROR_MASK) != 0)
                sdm_fields |= 1u << DfcSdmFieldCounterOffset;
        } else if(meta_read < DFC_SDM_FREE_ACCESS) {
            sdm_fields |= 1u << DfcSdmFieldPiccDataOffset;
        }
        if(sdm_file_read != DFC_SDM_DENIED_ACCESS) {
            sdm_fields |= (1u << DfcSdmFieldMacInputOffset) | (1u << DfcSdmFieldMacOffset);
            if((sdm_options & DFC_SDM_ENCRYPTED_FILE_MASK) != 0)
                sdm_fields |= (1u << DfcSdmFieldEncryptedFileOffset) |
                              (1u << DfcSdmFieldEncryptedFileLength);
        }
        if((sdm_options & DFC_SDM_COUNTER_LIMIT_MASK) != 0)
            sdm_fields |= 1u << DfcSdmFieldCounterLimit;
        size_t expected_len = 3 + DFC_SDM_BASE_SETTINGS_LENGTH;
        for(uint8_t fields = sdm_fields; fields != 0; fields >>= 1) {
            if((fields & 1u) != 0) expected_len += DFC_SDM_OFFSET_LENGTH;
        }
        if(body_len != expected_len) {
            dfc_bytebuf_append_byte(
                tx_buffer,
                body_len < expected_len ? DFC_STATUS_LENGTH_ERROR : DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        if((sdm_access_rights & 0x00F0u) != 0x00F0u) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        if(((sdm_options & (DFC_SDM_UID_MIRROR_MASK | DFC_SDM_COUNTER_MIRROR_MASK)) == 0 &&
            meta_read != DFC_SDM_DENIED_ACCESS) ||
           ((sdm_options & DFC_SDM_UID_MIRROR_MASK) != 0 && meta_read == DFC_SDM_DENIED_ACCESS)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        if((sdm_options & DFC_SDM_ASCII_ENCODING_MASK) == 0) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        if((sdm_options & DFC_SDM_COUNTER_MIRROR_MASK) == 0 &&
           (sdm_access_rights & DFC_SDM_ACCESS_MASK) != DFC_SDM_DENIED_ACCESS) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        if((sdm_fields & (1u << DfcSdmFieldMacInputOffset)) != 0 && sdm_file_read >= key_count) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
    }
#endif
    uint32_t key_references = access_rights;
#if DFC_ENABLE_SDM
    if(settings) {
        key_references |= (uint32_t)(sdm_access_rights & 0xF000u) << 4;
        key_references |= (uint32_t)(sdm_access_rights & DFC_SDM_ACCESS_MASK) << 20;
    }
    const size_t reference_count = 6;
#else
    const size_t reference_count = 4;
#endif
    if(!dfc_emulator_command_key_references_exist(key_references, reference_count, key_count)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    file->comm_settings = comm_settings;
    file->access_rights = access_rights;
#if DFC_ENABLE_SDM
    memset(&file->state.data.sdm, 0, sizeof(file->state.data.sdm));
    if(settings) {
        file->state.data.sdm.options = sdm_options;
        file->state.data.sdm.access_rights = sdm_access_rights;
        const uint8_t* cursor = settings + DFC_SDM_BASE_SETTINGS_LENGTH;
        for(size_t i = 0; i < DfcSdmFieldCount; i++, sdm_fields >>= 1) {
            if((sdm_fields & 1u) == 0) continue;
            uint32_t value = read_uint24_le(cursor);
            cursor += DFC_SDM_OFFSET_LENGTH;
            set_sdm_field(file, i, value);
        }
        dfc_file_sdm_flag_set(file, DfcSdmFlagEnabled, true);
        file->state.data.sdm_read_counter = 0;
    }
    emulator->sdm_read_cache_valid = false;
#endif
    dfc_credential_mark_dirty(emulator->credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_change_file_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }

    uint8_t change_key = dfc_emulator_command_access_change_nibble(file->access_rights);
    if(change_key == DFC_ACCESS_DENY) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_access_nibble_allows(emulator, change_key)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }

    const uint8_t* body = apdu + 2;
    size_t body_len = apdu_len - 2;
    size_t clear_len = 0;
    size_t scratch_len = 0;
    bool ev2_active = false;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    ev2_active = emulator->ev2_session_active;
#endif
    if(dfc_emulator_native_authentication_active(emulator) && !ev2_active) {
        // Free access stays plain; key-gated settings use the secure-write Full path.
        uint8_t protection = change_key == DFC_ACCESS_FREE ? DFC_COMM_PLAIN : DFC_COMM_ENCIPHERED;
        clear_len = dfc_emulator_command_unwrap_write_payload(
            emulator,
            apdu,
            apdu_len,
            1,
            protection,
            emulator->pending_chain,
            sizeof(emulator->pending_chain),
            &body,
            &scratch_len);
        if(clear_len == SIZE_MAX) {
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        body_len = clear_len;
    }

    apply_change_file_settings(emulator, file, body, body_len, tx_buffer);
    dfc_secure_wipe(emulator->pending_chain, scratch_len);
}

void dfc_emulator_command_get_application_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    uint8_t aids[DFC_MAX_APPS * 3];
    size_t aids_len = 0;
    for(size_t i = 0; i < emulator->credential->num_apps; i++) {
        const DfcApplication* app = &emulator->credential->apps[i];
        aids[aids_len++] = app->aid[2];
        aids[aids_len++] = app->aid[1];
        aids[aids_len++] = app->aid[0];
    }
    dfc_emulator_command_emit_payload_with_chaining(
        emulator, tx_buffer, DFC_CMD_GET_APPLICATION_IDS, aids, aids_len, 3);
}

static bool iso_file_id_in_use(const DfcCredential* credential, uint16_t iso_file_id) {
    for(size_t i = 0; i < credential->num_apps; i++) {
        const DfcApplication* app = &credential->apps[i];
        if(app->has_iso_file_id && app->iso_file_id == iso_file_id) return true;
    }
    return false;
}

static bool iso_df_name_in_use(
    const DfcCredential* credential,
    const uint8_t* df_name,
    size_t df_name_len) {
    if(df_name_len == 0) return false;
    for(size_t i = 0; i < credential->num_apps; i++) {
        const DfcApplication* app = &credential->apps[i];
        if(app->iso_aid_len == df_name_len && memcmp(app->iso_aid, df_name, df_name_len) == 0) {
            return true;
        }
    }
    return false;
}

void dfc_emulator_command_create_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    // Native payload: AID(3) + KS1 + KS2 [+ ISO FID(2) [+ DF name]]
    if(apdu_len < 6) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    size_t payload_len = apdu_len - 1;
    if(payload_len != 5 && payload_len < 7) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    DfcCredential* credential = emulator->credential;
    const uint8_t* aid = apdu + 1;
    if(aid[0] == 0x00 && aid[1] == 0x00 && aid[2] == 0x00) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(dfc_credential_find_application_desfire_order(credential, aid)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_DUPLICATE_ERROR);
        return;
    }

    size_t key_count = apdu[5] & DFC_NUM_KEYS_MASK;
    if(key_count == 0 || key_count > DFC_EV1_MAX_KEYS_PER_APP) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(credential->num_apps >= DFC_MAX_APPS || credential->num_apps >= DFC_EV1_MAX_APPLICATIONS) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_COUNT_ERROR);
        return;
    }

    size_t optional_offset = 6;
#if DFC_ENABLE_KEY_SETS
    bool has_key_sets = false;
    uint8_t active_key_set_version = DFC_KEY_SET_INITIAL_VERSION;
    uint8_t key_set_count = 1;
    uint8_t max_key_size =
        (uint8_t)dfc_credential_stored_key_length(dfc_credential_key_length(apdu[5]));
    uint8_t key_set_settings = 0;
#endif
    if((apdu[5] & DFC_KS2_EXTENDED_SETTINGS) != 0) {
        if(apdu_len <= optional_offset) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        uint8_t extended_settings = apdu[optional_offset++];
#if DFC_ENABLE_KEY_SETS
        has_key_sets = (extended_settings & DFC_EXTENDED_SETTINGS_KEY_SETS) != 0;
        if(has_key_sets) {
            const size_t key_set_parameter_count = DFC_CREATE_APPLICATION_KEY_SET_PARAMETER_COUNT;
            if(apdu_len < optional_offset + key_set_parameter_count) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                return;
            }
            active_key_set_version = apdu[optional_offset++];
            key_set_count = apdu[optional_offset++];
            max_key_size = apdu[optional_offset++];
            key_set_settings = apdu[optional_offset++];
            size_t active_key_size =
                dfc_credential_stored_key_length(dfc_credential_key_length(apdu[5]));
            if(key_set_count < DFC_KEY_SET_MINIMUM_COUNT || key_set_count > DFC_MAX_KEY_SETS ||
               (max_key_size != DFC_KEY_SET_MAXIMUM_16_BYTE &&
                max_key_size != DFC_KEY_SET_MAXIMUM_24_BYTE) ||
               max_key_size < active_key_size) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
                return;
            }
        }
#else
        if((extended_settings & DFC_EXTENDED_SETTINGS_KEY_SETS) != 0) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
            return;
        }
#endif
    }
    bool has_iso_file_id = apdu_len >= optional_offset + 2;
    uint16_t iso_file_id = 0;
    const uint8_t* df_name = NULL;
    size_t df_name_len = 0;
    if(has_iso_file_id) {
        iso_file_id =
            (uint16_t)(apdu[optional_offset] | ((uint16_t)apdu[optional_offset + 1] << 8));
        if(iso_file_id_in_use(credential, iso_file_id)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_DUPLICATE_ERROR);
            return;
        }
        if(apdu_len > optional_offset + 2) {
            df_name = apdu + optional_offset + 2;
            df_name_len = apdu_len - (optional_offset + 2);
            if(df_name_len > sizeof(((DfcApplication*)0)->iso_aid)) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
                return;
            }
            if(iso_df_name_in_use(credential, df_name, df_name_len)) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_DUPLICATE_ERROR);
                return;
            }
        }
    }

    uint8_t key_settings_2 = apdu[5];
    // Presence of app ISO FID implies file ISO FID support (KS2 bit 5).
    if(has_iso_file_id) {
        key_settings_2 = (uint8_t)(key_settings_2 | DFC_KS2_ISO_FILE_IDS);
    }
    DfcApplication* app =
        dfc_credential_create_application_desfire_order(credential, aid, apdu[4], key_settings_2);
    if(!app) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    app->has_auth_commands = true;
    app->auth_commands =
        dfc_credential_default_auth_commands(key_settings_2, credential->card.generation) |
        (DFC_ENABLE_ISO7816_AUTH ? DFC_AUTH_COMMAND_ISO7816 : 0);
#if DFC_ENABLE_KEY_SETS
    if(has_key_sets) {
        if(!dfc_credential_key_sets_resize(
               credential,
               app,
               key_set_count,
               key_count,
               dfc_credential_key_length(key_settings_2),
               max_key_size)) {
            size_t app_index = dfc_credential_application_index(credential, app);
            dfc_credential_delete_application_at(credential, app_index);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
            return;
        }
        app->key_set_settings = key_set_settings;
        uint8_t* active_version = dfc_credential_key_set_version(credential, app, 0);
        uint8_t active_type =
            (uint8_t)((key_settings_2 & DFC_KEY_TYPE_MASK) >> DFC_KEY_TYPE_SHIFT);
        if(!active_version) {
            size_t app_index = dfc_credential_application_index(credential, app);
            dfc_credential_delete_application_at(credential, app_index);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
            return;
        }
        *active_version = active_key_set_version;
        for(size_t index = 0; index < app->num_key_sets; index++) {
            uint8_t* type = dfc_credential_key_set_type(credential, app, index);
            if(!type) {
                size_t app_index = dfc_credential_application_index(credential, app);
                dfc_credential_delete_application_at(credential, app_index);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
                return;
            }
            *type = active_type;
        }
    }
#endif
    if(has_iso_file_id) {
        app->has_iso_file_id = true;
        app->iso_file_id = iso_file_id;
    }
    if(df_name_len > 0) {
        memcpy(app->iso_aid, df_name, df_name_len);
        app->iso_aid_len = (uint8_t)df_name_len;
    }
    dfc_credential_mark_dirty(credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_delete_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    DfcApplication* app = dfc_credential_find_application_desfire_order(credential, apdu + 1);
    if(!app) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_APPLICATION_NOT_FOUND);
        return;
    }

    size_t app_index = dfc_credential_application_index(credential, app);
    bool authorized = false;
    if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc) {
        authorized = dfc_emulator_command_is_authenticated_as_master(emulator);
    } else if(
        emulator->selected_application == DfcEmulatorSelectedApplicationApp &&
        emulator->selected_app_index == app_index) {
        authorized = dfc_emulator_command_is_authenticated_as_master(emulator);
    }
    if(!authorized) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }

    dfc_emulator_select_application(emulator, DfcEmulatorSelectedApplicationPicc, 0);
    dfc_credential_delete_application_at(credential, app_index);
    dfc_credential_mark_dirty(credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

static bool file_iso_id_in_use(const DfcCredential* credential, uint16_t iso_file_id) {
    for(size_t i = 0; i < credential->num_files; i++) {
        const DfcFile* file = &credential->files[i];
        if(file->has_iso_file_id && file->iso_file_id == iso_file_id) return true;
    }
    return false;
}

static bool new_file_number_is_available(
    DfcEmulator* emulator,
    uint8_t file_number,
    uint8_t duplicate_status,
    DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    if(file_number > DFC_EV1_MAX_FILE_NUMBER) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return false;
    }
    if(dfc_credential_find_file_in_app(credential, emulator->selected_app_index, file_number)) {
        dfc_bytebuf_append_byte(tx_buffer, duplicate_status);
        return false;
    }
    return true;
}

static bool new_file_count_is_available(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    if(dfc_credential_count_files_in_app(credential, emulator->selected_app_index) >=
           DFC_EV1_MAX_FILES_PER_APP ||
       credential->num_files >= DFC_MAX_FILES) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_COUNT_ERROR);
        return false;
    }
    return true;
}

static void publish_created_file(DfcEmulator* emulator, DfcFile* file, DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    if(!dfc_emulator_transaction_snapshot_insert_file(
           emulator, (size_t)(file - credential->files))) {
        (void)dfc_credential_delete_file(credential, emulator->selected_app_index, file->number);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    dfc_emulator_transaction_snapshot_note_persistent_change(emulator);
    dfc_credential_mark_dirty(credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_create_std_data_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t file_type,
    DfcByteBuf* tx_buffer) {
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    DfcCredential* credential = emulator->credential;
    // Native payload: FileNo + [ISO FID LE 2] + Comm + AR(2) + Size(3)
    // Without ISO FID: 7 bytes; with ISO FID: 9 bytes.
    size_t payload_len = apdu_len - 1;
    bool has_iso_file_id = payload_len == 9;
    if(payload_len != 7 && payload_len != 9) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    uint8_t file_no = apdu[1];
    size_t settings_offset = has_iso_file_id ? 4 : 2;
    uint16_t iso_file_id = 0;
    if(has_iso_file_id) {
        DfcApplication* app = emulator_current_app(emulator);
        if(!app || (app->key_settings_2 & DFC_KS2_ISO_FILE_IDS) == 0) {
            // ISO FID only when KS2 bit 5 enabled for the application.
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        iso_file_id = (uint16_t)(apdu[2] | ((uint16_t)apdu[3] << 8));
        if(file_iso_id_in_use(credential, iso_file_id)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_DUPLICATE_ERROR);
            return;
        }
    }

    if(!new_file_number_is_available(emulator, file_no, DFC_STATUS_DUPLICATE_ERROR, tx_buffer) ||
       !new_file_count_is_available(emulator, tx_buffer))
        return;

    uint8_t comm_settings = apdu[settings_offset];
    uint16_t access_rights = dfc_emulator_command_access_rights_from_wire(
        apdu[settings_offset + 1], apdu[settings_offset + 2]);
    uint32_t file_size = read_uint24_le(apdu + settings_offset + 3);
    if(file_size > DFC_MAX_FILE_DATA || file_size > DFC_FILE_POOL_SIZE) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(file_size > dfc_credential_file_pool_free(credential) ||
       !has_card_file_capacity(credential, file_size)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }

    DfcFile* file = dfc_credential_create_file(credential, emulator->selected_app_index, file_no);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    file->type = file_type;
    file->comm_settings = comm_settings;
    file->access_rights = access_rights;
    if(has_iso_file_id) {
        file->has_iso_file_id = true;
        file->iso_file_id = iso_file_id;
    }
    if(!dfc_file_set_data_size(credential, file, file_size)) {
        (void)dfc_credential_delete_file(credential, emulator->selected_app_index, file_no);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    publish_created_file(emulator, file, tx_buffer);
}

#if DFC_ENABLE_RECORD_FILES
void dfc_emulator_command_create_record_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t file_type,
    DfcByteBuf* tx_buffer) {
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    size_t payload_len = apdu_len - 1;
    bool has_iso_file_id = payload_len == 12;
    if(payload_len != 10 && payload_len != 12) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    DfcCredential* credential = emulator->credential;
    uint8_t file_no = apdu[1];
    size_t settings_offset = has_iso_file_id ? 4 : 2;
    if(!new_file_number_is_available(emulator, file_no, DFC_STATUS_DUPLICATE_ERROR, tx_buffer))
        return;

    uint16_t iso_file_id = 0;
    if(has_iso_file_id) {
        DfcApplication* app = emulator_current_app(emulator);
        if(!app || (app->key_settings_2 & DFC_KS2_ISO_FILE_IDS) == 0) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
            return;
        }
        iso_file_id = (uint16_t)(apdu[2] | ((uint16_t)apdu[3] << 8));
        if(file_iso_id_in_use(credential, iso_file_id)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_DUPLICATE_ERROR);
            return;
        }
    }
    uint32_t record_size = read_uint24_le(apdu + settings_offset + 3);
    uint32_t max_records = read_uint24_le(apdu + settings_offset + 6);
    uint64_t allocation = (uint64_t)record_size * max_records;
    if(file_type == DFC_FILE_TYPE_CYCLIC_RECORD && max_records == 1) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(record_size == 0 || max_records == 0 || allocation > DFC_MAX_FILE_DATA ||
       allocation > dfc_credential_file_pool_free(credential) ||
       !has_card_file_capacity(credential, (size_t)allocation)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(!new_file_count_is_available(emulator, tx_buffer)) return;

    DfcFile* file = dfc_credential_create_file(credential, emulator->selected_app_index, file_no);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_COUNT_ERROR);
        return;
    }
    file->type = file_type;
    file->comm_settings = apdu[settings_offset];
    file->access_rights = dfc_emulator_command_access_rights_from_wire(
        apdu[settings_offset + 1], apdu[settings_offset + 2]);
    file->state.record.size = record_size;
    file->state.record.max_records = max_records;
    file->state.record.count = 0;
    if(has_iso_file_id) {
        file->has_iso_file_id = true;
        file->iso_file_id = iso_file_id;
    }
    if(!dfc_file_resize(credential, file, (size_t)allocation)) {
        (void)dfc_credential_delete_file(credential, emulator->selected_app_index, file_no);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    publish_created_file(emulator, file, tx_buffer);
}
#endif

void dfc_emulator_command_create_value_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    DfcCredential* credential = emulator->credential;
    uint8_t file_no = apdu[1];
    if(!new_file_number_is_available(emulator, file_no, DFC_STATUS_DUPLICATE_ERROR, tx_buffer) ||
       !new_file_count_is_available(emulator, tx_buffer))
        return;

    int32_t lower_limit = dfc_emulator_command_read_int32_le(apdu + 5);
    int32_t upper_limit = dfc_emulator_command_read_int32_le(apdu + 9);
    int32_t value = dfc_emulator_command_read_int32_le(apdu + 13);
    if(lower_limit > upper_limit || value < lower_limit || value > upper_limit) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }

    DfcFile* file = dfc_credential_create_file(credential, emulator->selected_app_index, file_no);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    file->type = 0x02;
    file->comm_settings = apdu[2];
    file->access_rights = dfc_emulator_command_access_rights_from_wire(apdu[3], apdu[4]);
    file->state.value.lower_limit = lower_limit;
    file->state.value.upper_limit = upper_limit;
    file->state.value.current = value;
    file->state.value.limited_credit = apdu[17];
    file->state.value.limited_credit_value = 0;
    file->state.value.limited_credit_pending = 0;
    file->state.value.pending = false;
    file->state.value.pending_delta = 0;
    file->state.value.pending_value = 0;
    file->state.value.pending_absolute = false;
    publish_created_file(emulator, file, tx_buffer);
}

void dfc_emulator_command_delete_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    uint8_t file_no = apdu[1];
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, file_no);
    bool deletes_transaction_mac = file && file->type == DFC_FILE_TYPE_TRANSACTION_MAC;
    if(deletes_transaction_mac) {
        (void)dfc_emulator_abort_transaction(emulator);
    } else if(file) {
        dfc_emulator_transaction_snapshot_delete_file(
            emulator, (size_t)(file - emulator->credential->files));
    }
    if(dfc_credential_delete_file(emulator->credential, emulator->selected_app_index, file_no)) {
        dfc_emulator_transaction_snapshot_note_persistent_change(emulator);
        dfc_credential_mark_dirty(emulator->credential);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        return;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
}

#if DFC_ENABLE_TRANSACTION_MAC
static bool transaction_mac_file_has_capacity(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    DfcCredential* credential = emulator->credential;
    if(credential->file_pool_used > DFC_FILE_POOL_SIZE ||
       DFC_TRANSACTION_MAC_FILE_LENGTH > DFC_FILE_POOL_SIZE - credential->file_pool_used) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return false;
    }
    return true;
}

void dfc_emulator_command_create_transaction_mac_file(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(!require_create_delete_access(emulator, tx_buffer)) return;

    const size_t command_length = 1 + 5 + DFC_AES_KEY_LENGTH + 1;
    if(buffer_len != command_length || dfc_credential_find_transaction_mac_file(
                                           emulator->credential, emulator->selected_app_index)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    const DfcApplication* app = &emulator->credential->apps[emulator->selected_app_index];
    uint16_t access_rights = dfc_emulator_command_access_rights_from_wire(buffer[3], buffer[4]);
    if(!dfc_communication_settings_is_valid(buffer[2]) ||
       !dfc_transaction_mac_access_rights_are_valid(access_rights, app->num_keys) ||
       !dfc_transaction_mac_key_type_is_valid(buffer[5])) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(!new_file_number_is_available(emulator, buffer[1], DFC_STATUS_PARAMETER_ERROR, tx_buffer) ||
       !new_file_count_is_available(emulator, tx_buffer) ||
       !transaction_mac_file_has_capacity(emulator, tx_buffer))
        return;

    /* No fallible operation remains after the transaction boundary moves. */
    (void)dfc_emulator_abort_transaction(emulator);
    DfcFile* file =
        dfc_credential_create_file(emulator->credential, emulator->selected_app_index, buffer[1]);
    DFC_ASSERT(file);
    bool resized = dfc_file_resize(emulator->credential, file, DFC_TRANSACTION_MAC_FILE_LENGTH);
    DFC_ASSERT(resized);
    DFC_UNUSED(resized);
    file->number = buffer[1];
    file->type = DFC_FILE_TYPE_TRANSACTION_MAC;
    file->comm_settings = buffer[2];
    file->access_rights = access_rights;
    file->state.transaction_mac.key_type = buffer[5];
    file->contents_complete = true;
    memcpy(file->state.transaction_mac.key, buffer + 6, DFC_AES_KEY_LENGTH);
    file->state.transaction_mac.key_version = buffer[6 + DFC_AES_KEY_LENGTH];
    dfc_credential_mark_dirty(emulator->credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}
#endif

#endif
