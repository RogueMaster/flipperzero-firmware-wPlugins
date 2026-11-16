/*
 * Native command routing registry.
 *
 * DFC_NATIVE_COMMAND expands directly to a switch case in the dispatcher.
 * Keeping every command label here makes duplicate values a compile error and
 * leaves validation and direct handler calls together for review.
 */
#if DFC_ENABLE_PROXIMITY_CHECK
DFC_NATIVE_COMMAND(DFC_CMD_PREPARE_PROXIMITY_CHECK) {
        uint8_t prepare_candidate_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
        bool prepare_has_candidate = false;
        if(!prepare_ev1_command_update(
               emulator,
               cmd,
               buffer + 1,
               buffer_len - 1,
               prepare_candidate_iv,
               &prepare_has_candidate)) {
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return true;
        }
        if(buffer_len != 1) {
            commit_ev1_command_update(emulator, prepare_candidate_iv, prepare_has_candidate);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(dfc_emulator_command_prepare_proximity_check(emulator, tx_buffer)) {
            commit_ev1_command_update(emulator, prepare_candidate_iv, prepare_has_candidate);
        }
        dfc_secure_wipe(prepare_candidate_iv, sizeof(prepare_candidate_iv));
        return true;
    }
DFC_NATIVE_COMMAND(DFC_CMD_PROXIMITY_CHECK)
        dfc_emulator_command_proximity_check(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_VERIFY_PROXIMITY_CHECK)
        dfc_emulator_command_verify_proximity_check(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif
#if DFC_ENABLE_STATIC_SIGNATURE
DFC_NATIVE_COMMAND(DFC_CMD_READ_SIGNATURE)
        dfc_emulator_command_read_signature(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif

/* Card metadata and key management. */
DFC_NATIVE_COMMAND(DFC_CMD_CHANGE_KEY_SETTINGS)
        dfc_emulator_command_change_key_settings(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_VERSION)
        if(buffer_len != 1) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        dfc_emulator_command_get_version(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_CARD_UID)
        if(buffer_len != 1) {
            // EV1 and EV2 terminal errors retire their chained state before
            // the plain error is emitted. Legacy sessions keep their old rule.
            bool retires_session = dfc_emulator_native_authentication_active(emulator) &&
                                   emulator->secure_messaging->cipher !=
                                       DFC_CMD_AUTHENTICATE_LEGACY;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
            retires_session = retires_session || emulator->ev2_session_active;
#endif
            if(retires_session) dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        handle_get_card_uid(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_SELECT_APPLICATION)
        if(buffer_len < 4) return false;
        dfc_emulator_command_select_application(emulator, buffer, tx_buffer, dfc);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_KEY_SETTINGS)
        dfc_emulator_command_get_key_settings(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_KEY_VERSION)
        if(buffer_len < 2) return false;
        dfc_emulator_command_get_key_version(emulator, buffer, buffer_len, tx_buffer);
        return true;
#if DFC_ENABLE_KEY_SETS
DFC_NATIVE_COMMAND(DFC_CMD_INITIALIZE_KEY_SET)
        dfc_emulator_command_initialize_key_set(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_FINALIZE_KEY_SET)
        dfc_emulator_command_finalize_key_set(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_ROLL_KEY_SET)
        dfc_emulator_command_roll_key_set(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CHANGE_KEY_EV2)
        dfc_emulator_command_change_key_ev2(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif

/* Application and file lifecycle. */
#if DFC_ENABLE_DELEGATED_APPLICATIONS
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_DELEGATED_APPLICATION)
        handle_create_delegated_application(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_DELEGATED_INFO)
        handle_get_delegated_info(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif
DFC_NATIVE_COMMAND(DFC_CMD_GET_APPLICATION_IDS)
        dfc_emulator_command_get_application_ids(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_DF_NAMES)
        dfc_emulator_command_get_df_names(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_FREE_MEM)
        dfc_emulator_command_free_mem(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_APPLICATION)
        if(buffer_len < 6) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        dfc_emulator_command_create_application(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_DELETE_APPLICATION)
        if(buffer_len < 4) return false;
        dfc_emulator_command_delete_application(emulator, buffer, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_FORMAT_PICC)
        dfc_emulator_command_format_picc(emulator, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_ISO_FILE_IDS)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_get_iso_file_ids(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_FILE_IDS)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_get_file_ids(emulator, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_GET_FILE_SETTINGS)
        if(buffer_len < 2) return false;
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_get_file_settings(emulator, buffer, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CHANGE_FILE_SETTINGS)
        if(buffer_len < 2) return false;
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_change_file_settings(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_STD_DATA_FILE)
        if(buffer_len < 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_create_std_data_file(
            emulator, buffer, buffer_len, DFC_FILE_TYPE_STANDARD_DATA, tx_buffer);
        return true;
#if DFC_ENABLE_BACKUP_FILES
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_BACKUP_DATA_FILE)
        if(buffer_len < 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_create_std_data_file(
            emulator, buffer, buffer_len, DFC_FILE_TYPE_BACKUP_DATA, tx_buffer);
        return true;
#endif
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_VALUE_FILE)
        if(buffer_len < 18) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_create_value_file(emulator, buffer, tx_buffer);
        return true;
#if DFC_ENABLE_RECORD_FILES
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_LINEAR_RECORD_FILE)
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_CYCLIC_RECORD_FILE)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_create_record_file(
            emulator,
            buffer,
            buffer_len,
            cmd == DFC_CMD_CREATE_LINEAR_RECORD_FILE ? DFC_FILE_TYPE_LINEAR_RECORD :
                                                       DFC_FILE_TYPE_CYCLIC_RECORD,
            tx_buffer);
        return true;
#endif
DFC_NATIVE_COMMAND(DFC_CMD_DELETE_FILE)
        if(buffer_len != 2) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_delete_file(emulator, buffer, tx_buffer);
        return true;

/* Value files and transactions. */
DFC_NATIVE_COMMAND(DFC_CMD_GET_VALUE)
        if(buffer_len < 2) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_get_value(emulator, buffer, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CREDIT)
DFC_NATIVE_COMMAND(DFC_CMD_LIMITED_CREDIT)
DFC_NATIVE_COMMAND(DFC_CMD_DEBIT)
        if(buffer_len < 6) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_value_delta(
            emulator,
            buffer,
            buffer_len,
            tx_buffer,
            cmd,
            cmd == DFC_CMD_DEBIT,
            cmd == DFC_CMD_LIMITED_CREDIT);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_COMMIT_TRANSACTION)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        if(buffer_len > 2 ||
           (buffer_len == 2 && buffer[1] != DFC_TRANSACTION_COMMIT_RETURN_MAC_OPTION)) {
            dfc_emulator_command_clear_selected_app_pending_values(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        dfc_emulator_command_commit_transaction(
            emulator,
            buffer_len == 2 && buffer[1] == DFC_TRANSACTION_COMMIT_RETURN_MAC_OPTION,
            tx_buffer);
        return true;
#if DFC_ENABLE_TRANSACTION_MAC
DFC_NATIVE_COMMAND(DFC_CMD_CREATE_TRANSACTION_MAC_FILE)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_create_transaction_mac_file(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_COMMIT_READER_ID)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_commit_reader_id(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_NOTIFY_TRANSACTION_SUCCESS)
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return true;
#endif
DFC_NATIVE_COMMAND(DFC_CMD_ABORT_TRANSACTION)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        if(buffer_len != 1) {
            dfc_emulator_command_clear_selected_app_pending_values(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        dfc_emulator_command_abort_transaction(emulator, tx_buffer);
        return true;

/* Authentication and session continuation. */
DFC_NATIVE_COMMAND(DFC_CMD_CHANGE_KEY)
        dfc_emulator_command_change_key(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_AUTHENTICATE_LEGACY)
DFC_NATIVE_COMMAND(DFC_CMD_AUTHENTICATE_ISO)
DFC_NATIVE_COMMAND(DFC_CMD_AUTHENTICATE_AES)
        if(buffer_len < 2) return false;
        dfc_emulator_command_authenticate_step1(emulator, cmd, buffer, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_SET_CONFIGURATION)
        dfc_emulator_command_set_configuration(emulator, buffer, buffer_len, tx_buffer);
        return true;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
DFC_NATIVE_COMMAND(DFC_CMD_AUTHENTICATE_EV2_FIRST)
DFC_NATIVE_COMMAND(DFC_CMD_AUTHENTICATE_EV2_NON_FIRST) {
        uint8_t auth_candidate_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
        bool auth_has_candidate = false;
        if(!prepare_ev1_command_update(
               emulator, cmd, buffer + 1, buffer_len - 1, auth_candidate_iv, &auth_has_candidate)) {
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return true;
        }
        bool non_first = cmd == DFC_CMD_AUTHENTICATE_EV2_NON_FIRST;
        if(dfc_emulator_command_authenticate_ev2_start(
               emulator, buffer, buffer_len, non_first, tx_buffer)) {
            commit_ev1_command_update(emulator, auth_candidate_iv, auth_has_candidate);
        }
        dfc_secure_wipe(auth_candidate_iv, sizeof(auth_candidate_iv));
        return true;
    }
#endif
DFC_NATIVE_COMMAND(DFC_CMD_ADDITIONAL_FRAME)
        if(
#if DFC_ENABLE_DELEGATED_APPLICATIONS
            emulator->delegated_creation_pending) {
            handle_create_delegated_application_continuation(
                emulator, buffer, buffer_len, tx_buffer);
        } else if(
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
            emulator->ev2_authentication_pending) {
            dfc_emulator_command_authenticate_ev2_continuation(
                emulator, buffer, buffer_len, tx_buffer);
        } else if(
#endif
            emulator->awaiting_step2) {
            dfc_emulator_command_authenticate_step2(emulator, buffer, buffer_len, tx_buffer, dfc);
        } else if(emulator->get_version_frame != 0) {
            if(dfc_emulator_native_authentication_active(emulator)) {
                if(!dfc_secure_messaging_update_ev1_command(
                       emulator->secure_messaging, cmd, buffer + 1, buffer_len - 1)) {
                    dfc_emulator_command_clear_pending_chain(emulator);
                    dfc_emulator_reset_session(emulator);
                    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
                    return true;
                }
            }
            if(buffer_len != 1) {
                dfc_emulator_command_clear_pending_chain(emulator);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            } else {
                dfc_emulator_command_get_version_continuation(emulator, tx_buffer);
            }
            dfc_emulator_command_apply_ev1_response_secure_messaging(
                emulator, DFC_CMD_GET_VERSION, tx_buffer);
        } else if(emulator->df_names_pending) {
            dfc_emulator_command_df_names_continuation(emulator, tx_buffer);
        } else if(emulator->pending_chain_len > emulator->pending_chain_offset) {
            handle_pending_chain_continuation(emulator, tx_buffer);
        } else {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        }
        return true;

/* Data and record operations. */
DFC_NATIVE_COMMAND(DFC_CMD_READ_DATA)
#if DFC_ENABLE_SDM
    {
        uint8_t read_original_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE] = {0};
        uint8_t read_candidate_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
        bool read_has_candidate = false;
        if(dfc_emulator_native_authentication_active(emulator)) {
            memcpy(read_original_iv, emulator->secure_messaging->iv, sizeof(read_original_iv));
        }
        if(!prepare_ev1_command_update(
               emulator, cmd, buffer + 1, buffer_len - 1, read_candidate_iv, &read_has_candidate)) {
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return true;
        }
        commit_ev1_command_update(emulator, read_candidate_iv, read_has_candidate);
        if(buffer_len < 8) {
            dfc_secure_wipe(read_original_iv, sizeof(read_original_iv));
            dfc_secure_wipe(read_candidate_iv, sizeof(read_candidate_iv));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) {
            dfc_secure_wipe(read_original_iv, sizeof(read_original_iv));
            dfc_secure_wipe(read_candidate_iv, sizeof(read_candidate_iv));
            return true;
        }
        bool handled =
            dfc_emulator_command_read_data(emulator, buffer, buffer_len, tx_buffer, dfc);
        if(dfc_random_failure_is_retryable() && read_has_candidate &&
           dfc_emulator_native_authentication_active(emulator)) {
            memcpy(emulator->secure_messaging->iv, read_original_iv, sizeof(read_original_iv));
        }
        dfc_secure_wipe(read_original_iv, sizeof(read_original_iv));
        dfc_secure_wipe(read_candidate_iv, sizeof(read_candidate_iv));
        return handled;
    }
#else
        if(buffer_len < 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        return dfc_emulator_command_read_data(emulator, buffer, buffer_len, tx_buffer, dfc);
#endif
#if DFC_ENABLE_SDM
DFC_NATIVE_COMMAND(DFC_CMD_GET_FILE_COUNTERS)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_get_file_counters(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif
DFC_NATIVE_COMMAND(DFC_CMD_WRITE_DATA)
        if(buffer_len < 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_write_data(emulator, buffer, buffer_len, tx_buffer);
        return true;
#if DFC_ENABLE_RECORD_FILES
DFC_NATIVE_COMMAND(DFC_CMD_WRITE_RECORD)
        if(buffer_len < 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_write_record(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_READ_RECORDS)
        if(buffer_len != 8) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        handle_read_records(emulator, buffer, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_UPDATE_RECORD)
DFC_NATIVE_COMMAND(DFC_CMD_UPDATE_RECORD_ISO)
        if(emulator->credential->card.generation == DfcGenerationEv1) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
            return true;
        }
        if(buffer_len < DFC_UPDATE_RECORD_HEADER_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_update_record(emulator, buffer, buffer_len, tx_buffer);
        return true;
DFC_NATIVE_COMMAND(DFC_CMD_CLEAR_RECORD_FILE)
        if(!require_selected_application(emulator, tx_buffer)) return true;
        dfc_emulator_command_clear_record_file(emulator, buffer, buffer_len, tx_buffer);
        return true;
#endif
