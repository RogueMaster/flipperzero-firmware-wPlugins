#include "view_write.h"

static void uhf_reader_seed_ap_buffer(uint8_t* buffer, uint32_t access_pwd) {
    buffer[0] = (access_pwd >> 24) & 0xFF;
    buffer[1] = (access_pwd >> 16) & 0xFF;
    buffer[2] = (access_pwd >> 8) & 0xFF;
    buffer[3] = access_pwd & 0xFF;
}

static void uhf_reader_update_ap_prompt_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    char tempBuffer[9];
    snprintf(
        tempBuffer,
        sizeof(tempBuffer),
        "%02X%02X%02X%02X",
        App->ApTempBuffer[0],
        App->ApTempBuffer[1],
        App->ApTempBuffer[2],
        App->ApTempBuffer[3]);

    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);
    if(App->DefaultLockAccessPwdStr != NULL && App->SettingLockApPwdItem != NULL) {
        furi_string_set_str(App->DefaultLockAccessPwdStr, tempBuffer);
        variable_item_set_current_value_text(
            App->SettingLockApPwdItem, furi_string_get_cstr(App->DefaultLockAccessPwdStr));
    }

    with_view_model(
        App->ViewWrite, UHFReaderWriteModel * Model, { Model->WriteApPromptDone = true; }, false);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewWrite);
}

/**
 * @brief      Callback for returning to write submenu screen.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_write_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewWrite;
}

/**
 * @brief      Callback for the epc value text input screen.
 * @details    This function saves the current tag selected with all its info
 * @param      context  The UHFReaderApp - Used to change app variables.
*/
void uhf_reader_epc_value_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool redraw = true;
    with_view_model(
        App->ViewWrite,

        //Keep track of the new epc value
        UHFReaderWriteModel * Model,
        {
            furi_string_set_str(Model->NewEpcValue, App->TempSaveBuffer);
            //A value has been entered for the chosen bank: reveal the Write button.
            Model->BankChosen = true;
        },

        redraw);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewWrite);
}

/**
 * @brief      Callback for write timer elapsed.
 * @details    This function is called when the timer is elapsed for the write screen and is currently not used for much...
 * @param      context  The context - The UHFReaderApp
*/
void uhf_reader_view_write_timer_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    view_dispatcher_send_custom_event(App->ViewDispatcher, UHFReaderEventIdRedrawScreen);
}

/**
 * @brief      Write enter callback function.
 * @details    This function is called when the view transitions to the write screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_write_enter_callback(void* context) {
    //Grab the period for the timer
    uint32_t Period = furi_ms_to_ticks(200);
    UHFReaderApp* App = (UHFReaderApp*)context;

    //Populate the write model from the live scanned tag (ActionFromLive) or the
    //saved file (ActionFromSaved). This is NULL-safe and never leaks a handle.
    uhf_reader_fetch_selected_tag(App);

    //Determine per-bank availability. For a live (unsaved) tag we only allow the
    //non-EPC banks once they've actually been deep-read; EPC is always available
    //from the scan. For a saved tag every bank is selectable.
    bool TidAvail = true;
    bool UserAvail = true;
    bool ResAvail = true;
    bool UpdateMode = (App->ActionContext == ActionFromLive);
    if(UpdateMode) {
        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * Live,
            {
                TidAvail = Live->TidBankRead;
                UserAvail = Live->UserBankRead;
                ResAvail = Live->ResBankRead;
            },
            false);
    }
    with_view_model(
        App->ViewWrite,
        UHFReaderWriteModel * Model,
        {
            Model->IsUpdateMode = UpdateMode;
            Model->TidAvail = TidAvail;
            Model->UserAvail = UserAvail;
            Model->ResAvail = ResAvail;
            //Only clear the chosen bank on a fresh entry from the action menu.
            //Returning from the value-entry keyboard must keep the selection so
            //the Write/Update button stays visible.
            if(App->WriteMenuFreshEntry) {
                Model->BankChosen = false;
                Model->WriteApPromptDone = false;
                furi_string_set_str(Model->WriteFunction, WRITE_SELECT_BANK);
            }
        },
        false);

    if(UpdateMode) {
        bool prompt_ap = false;
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            { prompt_ap = !Model->WriteApPromptDone; },
            false);
        if(prompt_ap) {
            App->WriteMenuFreshEntry = false;
            uhf_reader_seed_ap_buffer(App->ApTempBuffer, App->YRM100XWorker->DefaultAP);
            byte_input_set_header_text(App->ApInput, "Access Password");
            byte_input_set_result_callback(
                App->ApInput,
                uhf_reader_update_ap_prompt_updated,
                NULL,
                App,
                App->ApTempBuffer,
                App->ApInputBufferSize);
            view_set_previous_callback(
                byte_input_get_view(App->ApInput), uhf_reader_navigation_lock_exit_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetReadAp);
            return;
        }
    } else {
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            { Model->WriteApPromptDone = false; },
            false);
    }
    App->WriteMenuFreshEntry = false;

    //Start the timer
    if(App->Timer == NULL) {
        App->Timer =
            furi_timer_alloc(uhf_reader_view_write_timer_callback, FuriTimerTypePeriodic, context);
    }
    furi_timer_start(App->Timer, Period);

    //Setting default reading states
    App->IsWriting = false;
}

/**
 * @brief      Callback when the user exits the write screen.
 * @details    This function is called when the user exits the write screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_write_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    // IsWriting is updated by both the worker and GUI threads, so it is not a
    // reliable lifetime test. stop() is a safe no-op when already stopped and
    // otherwise joins the thread before this view can be left.
    if(App->YRM100XWorker) {
        UHF_I("UI", "WRITE view exit -> worker stop/join");
        uhf_worker_stop(App->YRM100XWorker);
        App->IsWriting = false;
    }
    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
}

/**
 * @brief      Callback for the uhf worker for writing.
 * @details    This function is called when the uhf worker is started for writing.
 * @param      event  The UHFWorkerEvent - UHFReaderApp object.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_write_tag_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;

    // Back/teardown owns the UI while it is joining this worker. Do not post a
    // dispatcher event from the worker thread after a Stop request.
    if(uhf_worker_stop_requested(App->YRM100XWorker)) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false;
        return;
    }

    if(event == UHFWorkerEventSuccess) {
        dolphin_deed(DolphinDeedNfcReadSuccess);
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_success(App);

        //Reset booleans tracking if the kill or access password were set for writing.
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false;

        //If the save on write option is toggled, then update the fields for the saved tag
        if(App->UHFSaveType == YES_SAVE_ON_WRITE) {
            if(!flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
                FURI_LOG_E(TAG, "Failed to open file");
            }
            FuriString* NumEpcs = furi_string_alloc();
            FuriString* EpcAndName = furi_string_alloc();
            FuriString* TempTid = furi_string_alloc();
            FuriString* TempRes = furi_string_alloc();
            FuriString* TempMem = furi_string_alloc();
            FuriString* TempEpc = furi_string_alloc();
            FuriString* TempPc = furi_string_alloc();
            FuriString* TempCrc = furi_string_alloc();

            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    if(furi_string_equal(Model->WriteFunction, WRITE_EPC_VAL)) {
                        furi_string_set(TempTid, Model->TidValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->MemValue);
                        furi_string_set(TempEpc, Model->NewEpcValue);

                    } else if(furi_string_equal(Model->WriteFunction, WRITE_USR_MEM)) {
                        furi_string_set(TempTid, Model->TidValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->NewEpcValue);
                        furi_string_set(TempEpc, Model->EpcValue);

                    } else if(furi_string_equal(Model->WriteFunction, WRITE_TID_MEM)) {
                        furi_string_set(TempTid, Model->NewEpcValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->MemValue);
                        furi_string_set(TempEpc, Model->EpcValue);

                    } else if(furi_string_equal(Model->WriteFunction, WRITE_RES_MEM)) {
                        furi_string_set(TempTid, Model->TidValue);
                        furi_string_set(TempRes, Model->NewEpcValue);
                        furi_string_set(TempMem, Model->MemValue);
                        furi_string_set(TempEpc, Model->EpcValue);
                    }
                    furi_string_set(TempPc, Model->Pc);
                    furi_string_set(TempCrc, Model->Crc);
                },
                Redraw);

            //Get the selected tag index and save all tag fields
            furi_string_printf(NumEpcs, "Tag%ld", App->SelectedTagIndex);
            furi_string_printf(
                EpcAndName,
                "%s:%s:%s:%s:%s:%s:%s",
                furi_string_get_cstr(App->EpcName),
                furi_string_get_cstr(TempEpc),
                furi_string_get_cstr(TempTid),
                furi_string_get_cstr(TempRes),
                furi_string_get_cstr(TempMem),
                furi_string_get_cstr(TempPc),
                furi_string_get_cstr(TempCrc));

            if(!flipper_format_update_string_cstr(
                   App->EpcFile, furi_string_get_cstr(NumEpcs), furi_string_get_cstr(EpcAndName))) {
                FURI_LOG_E(TAG, "Failed to write to file");
            }

            flipper_format_file_close(App->EpcFile);
            furi_string_free(NumEpcs);
            furi_string_free(EpcAndName);
            furi_string_free(TempEpc);
            furi_string_free(TempPc);
            furi_string_free(TempCrc);
            furi_string_free(TempTid);
            furi_string_free(TempRes);
            furi_string_free(TempMem);
            dolphin_deed(DolphinDeedRfidAdd);
        }
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
    } else if(event == UHFWorkerEventAccessDenied) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false;
        view_dispatcher_send_custom_event(
            App->ViewDispatcher, UHFCustomEventWorkerExitAccessDenied);
    } else if(event == UHFWorkerEventWrongPassword) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false;
        view_dispatcher_send_custom_event(
            App->ViewDispatcher, UHFCustomEventWorkerExitWrongPassword);
    } else if(event == UHFWorkerEventAborted) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false;
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExitAborted);
    }
}

/**
 * @brief      Callback for custom write events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - UHFReaderAppEventId value.
 * @param      context  The context - UHFReaderApp object.
 * @return     true if the event was handled, false otherwise.
*/
bool uhf_reader_view_write_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(event) {
    // Redraw screen by passing true to last parameter of with_view_model.
    case UHFReaderEventIdRedrawScreen: {
        bool redraw = true;
        with_view_model(App->ViewWrite, UHFReaderWriteModel * _Model, { UNUSED(_Model); }, redraw);
        return true;
    }
    //Indicate a success with a message on the screen!
    case UHFCustomEventWorkerExit: {
        bool Redraw = true;
        App->IsWriting = false;

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                /*
                 * Show the bank that was ACTUALLY written.
                 * Alpha12 always displayed "EPC Written!" even after TID/User/
                 * Reserved writes, which was only a UI-label bug.
                 */
                if(furi_string_equal(Model->WriteFunction, WRITE_RES_MEM)) {
                    // ResBytes layout: [0..3] = kill pwd, [4..7] = access pwd
                    App->YRM100XWorker->DefaultKP = bytes_to_uint32(App->ResBytes, 4);
                    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ResBytes + 4, 4);
                    furi_string_set_str(Model->WriteFunction, WRITE_RES_OK);
                } else if(furi_string_equal(Model->WriteFunction, WRITE_TID_MEM)) {
                    furi_string_set_str(Model->WriteFunction, WRITE_TID_OK);
                } else if(furi_string_equal(Model->WriteFunction, WRITE_USR_MEM)) {
                    furi_string_set_str(Model->WriteFunction, WRITE_USR_OK);
                } else {
                    furi_string_set_str(Model->WriteFunction, WRITE_EPC_OK);
                }

                Model->IsWriting = false;
            },
            Redraw);

        return true;
    }
    //Indicate a failure :(
    case UHFCustomEventWorkerExitAborted: {
        bool Redraw = true;
        App->IsWriting = false;

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                furi_string_set(Model->WriteFunction, WRITE_EPC_CANCELED);
                Model->IsWriting = false;
            },
            Redraw);

        return true;
    }
    //Write rejected by tag: wrong access password / memory locked.
    case UHFCustomEventWorkerExitAccessDenied: {
        bool Redraw = true;
        App->IsWriting = false;

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                furi_string_set_str(Model->WriteFunction, "Memory Lock Error");
                Model->IsWriting = false;
            },
            Redraw);

        return true;
    }
    //Write rejected: access password was wrong.
    case UHFCustomEventWorkerExitWrongPassword: {
        bool Redraw = true;
        App->IsWriting = false;

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                furi_string_set_str(Model->WriteFunction, "Wrong Password");
                Model->IsWriting = false;
            },
            Redraw);

        return true;
    }
    //The ok button was pressed to trigger a write
    case UHFReaderEventIdOkPressed: {
        bool redraw = true;
        dolphin_deed(DolphinDeedNfcRead);

        //If the user presses the ok button while the app is writing (to cancel the operation if the tag is inactive) then the worker is stopped
        if(App->IsWriting) {
            uhf_worker_stop(App->YRM100XWorker);
            App->IsWriting = false;

            return true;
        }

        //No bank has been selected yet - the Write button is hidden, so ignore OK.
        bool BankChosen = false;
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            { BankChosen = Model->BankChosen; },
            false);
        if(!BankChosen) {
            return true;
        }

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                Model->IsWriting = true;
                // YRM100X-only write path

                // Never carry a bank selection across writes. Apart from
                // protecting retries, this also makes a partially rejected
                // write unable to arm a later operation accidentally.
                m100_disable_write_mask(App->YRM100XWorker->module, WRITE_EPC);
                m100_disable_write_mask(App->YRM100XWorker->module, WRITE_USER);
                m100_disable_write_mask(App->YRM100XWorker->module, WRITE_TID);
                m100_disable_write_mask(App->YRM100XWorker->module, WRITE_RFU);

                //Set true if the requested EPC write is rejected as invalid
                //(empty, misaligned, or over 96 bits) so we never brick the tag.
                bool epc_write_invalid = false;
                //Set true if the Reserved entry is not exactly 16 hex chars (8 bytes).
                bool res_write_invalid = false;
                //Set when requested bank text is invalid hex or exceeds capacity.
                bool data_write_invalid = false;

                // Resetting the dynamically allocated arrays to zero
                uhf_reader_fetch_selected_tag(App);
                memset(App->EpcBytes, 0, EPC_MAX_BANK_SIZE * sizeof(uint8_t));
                memset(App->ResBytes, 0, RESERVED_MAX_BANK_SIZE * sizeof(uint8_t));
                memset(App->TidBytes, 0, TID_MAX_BANK_SIZE * sizeof(uint8_t));
                memset(App->UserBytes, 0, USER_MAX_BANK_SIZE * sizeof(uint8_t));
                memset(App->PcBytes, 0, 2 * sizeof(uint16_t));
                memset(App->CrcBytes, 0, 2 * sizeof(uint16_t));

                // Resetting the size_t variables to zero
                App->EpcBytesLen = 0;
                App->ResBytesLen = 0;
                App->TidBytesLen = 0;
                App->UserBytesLen = 0;
                App->PcBytesLen = 0;
                App->CrcBytesLen = 0;

                uhf_tag_reset(App->YRM100XWorker->NewTag);

                hex_string_to_uint16(
                    furi_string_get_cstr(Model->Pc), App->PcBytes, 2U, &App->PcBytesLen);
                hex_string_to_uint16(
                    furi_string_get_cstr(Model->Crc), App->CrcBytes, 2U, &App->CrcBytesLen);

                uint16_t combinedPc = App->PcBytesLen ? App->PcBytes[0] : 0U;
                uint16_t combinedCrc = App->CrcBytesLen ? App->CrcBytes[0] : 0U;

                if(furi_string_equal(Model->WriteFunction, WRITE_EPC_VAL) &&
                   Model->NewEpcValue != NULL) {
                    //EPC memory is word-organized, so the entry must be a whole
                    //number of 16-bit words (a multiple of 4 hex chars). Auto-pad
                    //with leading zeros to the next word boundary so a short entry
                    //still writes cleanly (e.g. "123" -> "0123", "12345" ->
                    //"00012345"). Leading zeros preserve the numeric value.
                    size_t epc_rem = furi_string_size(Model->NewEpcValue) % 4;
                    if(epc_rem != 0) {
                        FuriString* padded = furi_string_alloc();
                        for(size_t i = 0; i < (4 - epc_rem); i++) {
                            furi_string_push_back(padded, '0');
                        }
                        furi_string_cat(padded, Model->NewEpcValue);
                        furi_string_set(Model->NewEpcValue, padded);
                        furi_string_free(padded);
                    }

                    const bool epc_parsed = hex_string_to_bytes(
                        furi_string_get_cstr(Model->NewEpcValue),
                        App->EpcBytes,
                        EPC_MAX_BANK_SIZE,
                        &App->EpcBytesLen);

                    //A valid EPC is a whole number of 16-bit words (even byte
                    //count), non-empty, and at most 96 bits (12 bytes). Writing a
                    //zero-length or misaligned EPC corrupts the tag (empty EPC,
                    //undetectable), so refuse it instead of writing.
                    if(!epc_parsed || App->EpcBytesLen == 0 || (App->EpcBytesLen % 2) != 0 ||
                       App->EpcBytesLen > 12) {
                        epc_write_invalid = true;
                    } else {
                        uhf_tag_set_epc(
                            App->YRM100XWorker->NewTag,
                            (uint8_t*)App->EpcBytes,
                            App->EpcBytesLen * sizeof(uint8_t));

                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_EPC);
                    }
                } else {
                    hex_string_to_bytes(
                        furi_string_get_cstr(Model->EpcValue),
                        App->EpcBytes,
                        EPC_MAX_BANK_SIZE,
                        &App->EpcBytesLen);
                    uhf_tag_set_epc(
                        App->YRM100XWorker->NewTag,
                        (uint8_t*)App->EpcBytes,
                        App->EpcBytesLen * sizeof(uint8_t));
                }

                if(furi_string_equal(Model->WriteFunction, WRITE_USR_MEM) &&
                   Model->NewEpcValue != NULL) {
                    //User bank is word-organised (16-bit words). Right-pad with trailing
                    //zeros to the next word boundary so partial entries write cleanly
                    //(e.g. "ABCDE" -> "ABCDE0").
                    size_t usr_rem = furi_string_size(Model->NewEpcValue) % 4;
                    if(usr_rem != 0) {
                        for(size_t i = 0; i < (4 - usr_rem); i++) {
                            furi_string_push_back(Model->NewEpcValue, '0');
                        }
                    }
                    const bool user_parsed = hex_string_to_bytes(
                        furi_string_get_cstr(Model->NewEpcValue),
                        App->UserBytes,
                        USER_MAX_BANK_SIZE,
                        &App->UserBytesLen);
                    if(!user_parsed || App->UserBytesLen == 0U) {
                        data_write_invalid = true;
                    } else {
                        uhf_tag_set_user(
                            App->YRM100XWorker->NewTag,
                            (uint8_t*)App->UserBytes,
                            App->UserBytesLen * sizeof(uint8_t));
                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_USER);
                    }

                } else {
                    hex_string_to_bytes(
                        furi_string_get_cstr(Model->MemValue),
                        App->UserBytes,
                        USER_MAX_BANK_SIZE,
                        &App->UserBytesLen);
                    uhf_tag_set_user(
                        App->YRM100XWorker->NewTag,
                        (uint8_t*)App->UserBytes,
                        App->UserBytesLen * sizeof(uint8_t));
                }
                if(furi_string_equal(Model->WriteFunction, WRITE_TID_MEM) &&
                   Model->NewEpcValue != NULL) {
                    //TID bank is word-organised (16-bit words). Right-pad with trailing
                    //zeros to the next word boundary.
                    size_t tid_rem = furi_string_size(Model->NewEpcValue) % 4;
                    if(tid_rem != 0) {
                        for(size_t i = 0; i < (4 - tid_rem); i++) {
                            furi_string_push_back(Model->NewEpcValue, '0');
                        }
                    }
                    const bool tid_parsed = hex_string_to_bytes(
                        furi_string_get_cstr(Model->NewEpcValue),
                        App->TidBytes,
                        TID_MAX_BANK_SIZE,
                        &App->TidBytesLen);
                    if(!tid_parsed || App->TidBytesLen == 0U) {
                        data_write_invalid = true;
                    } else {
                        uhf_tag_set_tid(
                            App->YRM100XWorker->NewTag,
                            (uint8_t*)App->TidBytes,
                            App->TidBytesLen * sizeof(uint8_t));
                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_TID);
                    }
                } else {
                    hex_string_to_bytes(
                        furi_string_get_cstr(Model->TidValue),
                        App->TidBytes,
                        TID_MAX_BANK_SIZE,
                        &App->TidBytesLen);
                    uhf_tag_set_tid(
                        App->YRM100XWorker->NewTag,
                        (uint8_t*)App->TidBytes,
                        App->TidBytesLen * sizeof(uint8_t));
                }
                if(furi_string_equal(Model->WriteFunction, WRITE_RES_MEM) &&
                   Model->NewEpcValue != NULL) {
                    //Reserved bank is exactly kill pwd (words 0-1) + access pwd (words 2-3)
                    //= 8 bytes = 16 hex chars. Reject any other length — a partial write
                    //would corrupt the tag's access control irreversibly.
                    if(furi_string_size(Model->NewEpcValue) != 16) {
                        res_write_invalid = true;
                    } else {
                        const bool reserved_parsed = hex_string_to_bytes(
                            furi_string_get_cstr(Model->NewEpcValue),
                            App->ResBytes,
                            RESERVED_MAX_BANK_SIZE,
                            &App->ResBytesLen);
                        if(!reserved_parsed || App->ResBytesLen != 8U) {
                            res_write_invalid = true;
                        } else {
                            uhf_tag_set_kill_pwd(
                                App->YRM100XWorker->NewTag, App->ResBytes, App->ResBytesLen);
                            uhf_tag_set_access_pwd(
                                App->YRM100XWorker->NewTag, App->ResBytes, App->ResBytesLen);
                            m100_enable_write_mask(App->YRM100XWorker->module, WRITE_RFU);
                        }
                    }
                } else {
                    hex_string_to_bytes(
                        furi_string_get_cstr(Model->ResValue),
                        App->ResBytes,
                        RESERVED_MAX_BANK_SIZE,
                        &App->ResBytesLen);
                }

                uhf_tag_set_epc_pc(App->YRM100XWorker->NewTag, combinedPc);
                uhf_tag_set_epc_crc(App->YRM100XWorker->NewTag, combinedCrc);
                uhf_tag_set_epc_size(
                    App->YRM100XWorker->NewTag, App->EpcBytesLen * sizeof(uint8_t));
                uhf_tag_set_user_size(
                    App->YRM100XWorker->NewTag, App->UserBytesLen * sizeof(uint8_t));
                uhf_tag_set_tid_size(
                    App->YRM100XWorker->NewTag, App->TidBytesLen * sizeof(uint8_t));

                App->YRM100XWorker->KillPwd = true;
                App->YRM100XWorker->AccessPwd = true;

                if(epc_write_invalid) {
                    //Reject the write outright so the tag is left untouched.
                    m100_disable_write_mask(App->YRM100XWorker->module, WRITE_EPC);
                    App->IsWriting = false;
                    Model->IsWriting = false;
                    furi_string_set_str(Model->WriteFunction, "Invalid EPC length");
                    uhf_notify_error(App);
                } else if(res_write_invalid) {
                    //Reserved must be exactly 16 hex chars (kill pwd + access pwd).
                    App->IsWriting = false;
                    Model->IsWriting = false;
                    furi_string_set_str(Model->WriteFunction, "Reserved must be 16 hex");
                    uhf_notify_error(App);
                } else if(data_write_invalid) {
                    App->IsWriting = false;
                    Model->IsWriting = false;
                    furi_string_set_str(Model->WriteFunction, "Invalid/too long data");
                    uhf_notify_error(App);
                } else {
                    // Guard: if no mask was enabled (e.g. WriteFunction holds an error
                    // string from a previous failure), there is nothing to write.
                    // Starting the worker with no masks active returns immediate Success
                    // without touching the tag, which is a silent false-positive.
                    bool any_mask =
                        m100_is_write_mask_enabled(App->YRM100XWorker->module, WRITE_EPC) ||
                        m100_is_write_mask_enabled(App->YRM100XWorker->module, WRITE_USER) ||
                        m100_is_write_mask_enabled(App->YRM100XWorker->module, WRITE_TID) ||
                        m100_is_write_mask_enabled(App->YRM100XWorker->module, WRITE_RFU);
                    if(!any_mask) {
                        App->IsWriting = false;
                        Model->IsWriting = false;
                        return true;
                    }
                    App->IsWriting = true;

                    //Target the specific scanned tag (live) or single-poll (saved).
                    uhf_reader_prepare_write_target(App);
                    uhf_worker_start(
                        App->YRM100XWorker,
                        UHFWorkerStateWriteSingle,
                        uhf_write_tag_worker_callback,
                        App);
                    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
                }
            },
            redraw);

        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief      Write Draw Callback.
 * @details    This function is called when the user selects write on the main submenu.
 * @param      canvas    The canvas - Canvas object for drawing the screen.
 * @param      model  The view model - model for the view with variables required for drawing.
*/
void uhf_reader_view_write_draw_callback(Canvas* canvas, void* model) {
    UHFReaderWriteModel* MyModel = (UHFReaderWriteModel*)model;

    //Clearing the canvas, setting the color, font and content displayed.
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    //D-pad bank picker rendered with the native black, arrow-glyph buttons:
    //  Up (top-left)  = EPC        Down (top-right)  = Reserved
    //  Left (bot-left)= TID        Right (bot-right) = User
    //Banks that were never read on a live tag are simply not offered.
    elements_button_up(canvas, "EPC");
    if(MyModel->ResAvail) elements_button_down(canvas, "Res");
    if(MyModel->TidAvail) elements_button_left(canvas, "TID");
    if(MyModel->UserAvail) elements_button_right(canvas, "User");

    //Center of the screen: title and the currently selected bank (or a prompt).
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 26, AlignCenter, AlignBottom, MyModel->IsUpdateMode ? "Update" : "Write");
    canvas_set_font(canvas, FontSecondary);
    if(MyModel->BankChosen) {
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignBottom, furi_string_get_cstr(MyModel->WriteFunction));
    } else {
        //Live/Update mode targets the specific scanned tag, so the "1st detected
        //tag" wording only applies to the saved (single-poll) write flow.
        if(!MyModel->IsUpdateMode) {
            canvas_draw_str_aligned(
                canvas, 64, 36, AlignCenter, AlignBottom, "Write to the 1st detected tag");
        }
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignBottom, "Pick a memory bank");
    }

    //Display the action button only once a bank has been chosen (Cancel while writing).
    if(MyModel->IsWriting) {
        elements_button_center(canvas, "Cancel");
    } else if(MyModel->BankChosen) {
        elements_button_center(canvas, MyModel->IsUpdateMode ? "Update" : "Write");
    }
}

/**
 * @brief      Callback for write screen input.
 * @details    This function is called when the user presses a button while on the write screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - UHFReaderApp object.
 * @return     true if the event was handled, false otherwise.
*/
bool uhf_reader_view_write_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    //Handle the short input types
    if(event->type == InputTypeShort) {
        //Read the current per-bank availability so greyed-out banks can't be edited.
        bool TidAvail = true, UserAvail = true, ResAvail = true;
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                TidAvail = Model->TidAvail;
                UserAvail = Model->UserAvail;
                ResAvail = Model->ResAvail;
            },
            false);

        //Up -> EPC bank (always available from the scan)
        if(event->key == InputKeyUp && !App->IsWriting) {
            text_input_set_header_text(App->EpcWrite, "EPC Value");

            bool redraw = false;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->EpcValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_EPC_VAL);
                },
                redraw);

            bool clear_previous_text = false;
            text_input_set_result_callback(
                App->EpcWrite,
                uhf_reader_epc_value_text_updated,
                App,
                App->TempSaveBuffer,
                App->TempBufferSaveSize,
                clear_previous_text);
            view_set_previous_callback(
                text_input_get_view(App->EpcWrite), uhf_reader_navigation_write_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
            return true;
        }

        //Left -> TID bank (greyed until the bank has been read on a live tag)
        else if(event->key == InputKeyLeft && !App->IsWriting) {
            if(!TidAvail) {
                uhf_notify_error(App);
                return true;
            }
            text_input_set_header_text(App->EpcWrite, "TID Value");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->TidValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_TID_MEM);
                },
                redraw);

            bool clear_previous_text = false;
            text_input_set_result_callback(
                App->EpcWrite,
                uhf_reader_epc_value_text_updated,
                App,
                App->TempSaveBuffer,
                App->TempBufferSaveSize,
                clear_previous_text);
            view_set_previous_callback(
                text_input_get_view(App->EpcWrite), uhf_reader_navigation_write_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
            return true;
        }

        //Right -> User bank (greyed until the bank has been read on a live tag)
        else if(event->key == InputKeyRight && !App->IsWriting) {
            if(!UserAvail) {
                uhf_notify_error(App);
                return true;
            }
            text_input_set_header_text(App->EpcWrite, "User Memory Bank");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->MemValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_USR_MEM);
                },
                redraw);

            bool clear_previous_text = false;
            text_input_set_result_callback(
                App->EpcWrite,
                uhf_reader_epc_value_text_updated,
                App,
                App->TempSaveBuffer,
                App->TempBufferSaveSize,
                clear_previous_text);
            view_set_previous_callback(
                text_input_get_view(App->EpcWrite), uhf_reader_navigation_write_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
            return true;
        }

        //Down -> Reserved bank (greyed until the bank has been read on a live tag)
        else if(event->key == InputKeyDown && !App->IsWriting) {
            if(!ResAvail) {
                uhf_notify_error(App);
                return true;
            }
            text_input_set_header_text(App->EpcWrite, "Reserved Memory Bank");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->ResValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_RES_MEM);
                },
                redraw);

            bool clear_previous_text = false;
            text_input_set_result_callback(
                App->EpcWrite,
                uhf_reader_epc_value_text_updated,
                App,
                App->TempSaveBuffer,
                App->TempBufferSaveSize,
                clear_previous_text);
            view_set_previous_callback(
                text_input_get_view(App->EpcWrite), uhf_reader_navigation_write_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
            return true;
        }
    } else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            //Handle the OK button event

            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFReaderEventIdOkPressed);
            return true;
        }
    }
    return false;
}

/**
 * @brief      Callback when the user exits the write screen.
 * @details    This function is called when the user exits the write screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_write_exit_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

/**
 * @brief      Allocates the write view.
 * @details    This function allocates all variables for the write view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_write_alloc(UHFReaderApp* App) {
    //Allocating the view and setting all callback functions
    App->ViewWrite = view_alloc();
    view_set_draw_callback(App->ViewWrite, uhf_reader_view_write_draw_callback);
    view_set_input_callback(App->ViewWrite, uhf_reader_view_write_input_callback);
    view_set_previous_callback(App->ViewWrite, uhf_reader_navigation_write_exit_callback);
    view_set_enter_callback(App->ViewWrite, uhf_reader_view_write_enter_callback);
    view_set_exit_callback(App->ViewWrite, uhf_reader_view_write_exit_callback);
    view_set_context(App->ViewWrite, App);
    view_set_custom_callback(App->ViewWrite, uhf_reader_view_write_custom_event_callback);

    //Allocating the view model
    view_allocate_model(App->ViewWrite, ViewModelTypeLockFree, sizeof(UHFReaderWriteModel));
    UHFReaderWriteModel* ModelWrite = view_get_model(App->ViewWrite);
    FuriString* EpcNameWriteDefault = furi_string_alloc();

    App->EpcBytesLen = 0;
    App->ResBytesLen = 0;
    App->TidBytesLen = 0;
    App->UserBytesLen = 0;
    App->PcBytesLen = 0;
    App->CrcBytesLen = 0;
    App->EpcBytes = ModelWrite->EpcBuffer;
    App->ResBytes = ModelWrite->ResBuffer;
    App->TidBytes = ModelWrite->TidBuffer;
    App->UserBytes = ModelWrite->UserBuffer;
    App->PcBytes = ModelWrite->PcBuffer;
    App->CrcBytes = ModelWrite->CrcBuffer;
    memset(App->EpcBytes, 0, sizeof(ModelWrite->EpcBuffer));
    memset(App->ResBytes, 0, sizeof(ModelWrite->ResBuffer));
    memset(App->TidBytes, 0, sizeof(ModelWrite->TidBuffer));
    memset(App->UserBytes, 0, sizeof(ModelWrite->UserBuffer));
    memset(App->PcBytes, 0, sizeof(ModelWrite->PcBuffer));
    memset(App->CrcBytes, 0, sizeof(ModelWrite->CrcBuffer));

    //Setting default values for the view model
    ModelWrite->Setting1Index = App->Setting1Index;
    ModelWrite->Setting2Power = App->Setting2PowerStr;

    UHF_I(
        "MEM",
        "LAZY Write inherited connection index=%u value=%s",
        (unsigned int)App->Setting1Index,
        App->Setting1Names[App->Setting1Index]);
    uhf_debug_flush();
    ModelWrite->SettingKillPwd =
        furi_string_alloc_set(App->DefaultKillPassword ? App->DefaultKillPassword : "00000000");
    ModelWrite->EpcName = EpcNameWriteDefault;
    ModelWrite->Setting1Value = furi_string_alloc_set(App->Setting1Names[App->Setting1Index]);
    ModelWrite->Pc = furi_string_alloc_set("XXXX");
    ModelWrite->Crc = furi_string_alloc_set("XXXX");
    FuriString* EpcValueWriteDefault = furi_string_alloc();
    furi_string_set_str(EpcValueWriteDefault, "Press Write");
    ModelWrite->EpcValue = EpcValueWriteDefault;
    FuriString* EpcValueWriteStatus = furi_string_alloc();
    furi_string_set_str(EpcValueWriteStatus, "Press Write");
    ModelWrite->WriteStatus = EpcValueWriteStatus;
    FuriString* WriteDefaultEpc = furi_string_alloc();
    ModelWrite->NewEpcValue = WriteDefaultEpc;
    FuriString* DefaultWriteFunction = furi_string_alloc();
    furi_string_set_str(DefaultWriteFunction, "Press Arrow Keys");
    ModelWrite->WriteFunction = DefaultWriteFunction;
    FuriString* DefaultWriteTid = furi_string_alloc();
    furi_string_set_str(DefaultWriteTid, "TID HERE");
    ModelWrite->TidValue = DefaultWriteTid;
    FuriString* DefaultWriteTidNew = furi_string_alloc();
    furi_string_set_str(DefaultWriteTidNew, "NEW TID HERE");
    ModelWrite->NewTidValue = DefaultWriteTidNew;
    FuriString* DefaultWriteRes = furi_string_alloc();
    furi_string_set_str(DefaultWriteRes, "RES HERE");
    ModelWrite->ResValue = DefaultWriteRes;
    FuriString* DefaultWriteResNew = furi_string_alloc();
    furi_string_set_str(DefaultWriteResNew, "NEW RES HERE");
    ModelWrite->NewResValue = DefaultWriteResNew;
    FuriString* DefaultWriteMem = furi_string_alloc();
    furi_string_set_str(DefaultWriteMem, "MEM HERE");
    ModelWrite->MemValue = DefaultWriteMem;
    FuriString* DefaultWriteMemNew = furi_string_alloc();
    furi_string_set_str(DefaultWriteMemNew, "NEW MEM HERE");
    ModelWrite->NewMemValue = DefaultWriteMemNew;
    App->EpcName = furi_string_alloc_set("Enter Name");
    App->EpcToWrite = furi_string_alloc_set("Enter Name");
    App->EpcWrite = text_input_alloc();
    ModelWrite->WriteApPromptDone = false;
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewEpcWriteInput, text_input_get_view(App->EpcWrite));

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewWrite, App->ViewWrite);
}

/**
 * @brief      Frees the write view.
 * @details    This function frees all variables for the write view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_write_free(UHFReaderApp* App) {
    if(!App || !App->ViewWrite) return;

    App->EpcBytes = NULL;
    App->ResBytes = NULL;
    App->TidBytes = NULL;
    App->UserBytes = NULL;
    App->PcBytes = NULL;
    App->CrcBytes = NULL;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
    text_input_free(App->EpcWrite);
    App->EpcWrite = NULL;

    if(App->ViewWrite) {
        UHFReaderWriteModel* model = view_get_model(App->ViewWrite);
        if(model) {
            FuriString** owned[] = {
                &model->EpcName,
                &model->WriteFunction,
                &model->EpcValue,
                &model->WriteStatus,
                &model->NewEpcValue,
                &model->TidValue,
                &model->NewTidValue,
                &model->ResValue,
                &model->NewResValue,
                &model->MemValue,
                &model->NewMemValue,
                &model->Setting1Value,
                &model->Crc,
                &model->Pc,
                &model->SettingKillPwd,
            };

            for(size_t i = 0; i < COUNT_OF(owned); i++) {
                if(*owned[i]) {
                    furi_string_free(*owned[i]);
                    *owned[i] = NULL;
                }
            }

            // Setting2Power is borrowed from App and is not owned by this model.
        }

        view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewWrite);
        view_free(App->ViewWrite);
        App->ViewWrite = NULL;
    }

    if(App->EpcName) {
        furi_string_free(App->EpcName);
        App->EpcName = NULL;
    }
    if(App->EpcToWrite) {
        furi_string_free(App->EpcToWrite);
        App->EpcToWrite = NULL;
    }
}
