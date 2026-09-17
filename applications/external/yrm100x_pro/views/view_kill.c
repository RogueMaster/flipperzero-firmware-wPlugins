#include "view_kill.h"
#include "../helpers/yrm100x_module.h"
/**
 * @brief      Callback for navigation on kill screen.
 * @details    This function is called when the user exits a screen on the kill screen
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_kill_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewKill;
}

//Safely set a FuriString from a (possibly-NULL) heap string returned by an
//extract_* helper, freeing the heap string. Sets empty on NULL to avoid a
//furi_string_set(str, NULL) crash.
static void uhf_set_from_extracted(FuriString* Dest, char* Extracted) {
    if(Extracted != NULL) {
        furi_string_set_str(Dest, Extracted);
        free(Extracted);
    } else {
        furi_string_set_str(Dest, "");
    }
}

/**
 * @brief      Populate the ViewWrite model from the in-memory live scanned tag.
 * @details    Used for the unsaved (ActionFromLive) path so Update/Lock/Kill
 *             operate on the specific tag currently displayed in the EPC dump,
 *             never touching Saved_EPCs.txt.
 * @param      App  The UHFReaderApp object.
*/
static void uhf_reader_fetch_live_tag(UHFReaderApp* App) {
    //Copy the live tag fields out of the EPC-dump model first, then populate the
    //write model. This avoids holding two view-model locks at once.
    FuriString* Epc = furi_string_alloc();
    FuriString* Tid = furi_string_alloc();
    FuriString* Res = furi_string_alloc();
    FuriString* Mem = furi_string_alloc();
    FuriString* Pc = furi_string_alloc();
    FuriString* Crc = furi_string_alloc();

    with_view_model(
        App->ViewEpc,
        UHFRFIDTagModel * Live,
        {
            furi_string_set(Epc, Live->Epc);
            furi_string_set(Tid, Live->Tid);
            furi_string_set(Res, Live->Reserved);
            furi_string_set(Mem, Live->User);
            furi_string_set(Pc, Live->Pc);
            furi_string_set(Crc, Live->Crc);
        },
        false);

    furi_string_set(App->EpcToWrite, Epc);
    //Unsaved tags have no name; build a readable placeholder from the EPC tail.
    const char* EpcStr = furi_string_get_cstr(Epc);
    size_t Len = strlen(EpcStr);
    const char* Tail = Len > 8 ? EpcStr + (Len - 8) : EpcStr;
    furi_string_printf(App->EpcName, "Tag_%s", Tail);

    with_view_model(
        App->ViewWrite,
        UHFReaderWriteModel * Model,
        {
            furi_string_set(Model->EpcName, App->EpcName);
            furi_string_set(Model->EpcValue, Epc);
            furi_string_set(Model->TidValue, Tid);
            furi_string_set(Model->ResValue, Res);
            furi_string_set(Model->MemValue, Mem);
            furi_string_set(Model->Pc, Pc);
            furi_string_set(Model->Crc, Crc);
        },
        true);

    furi_string_free(Epc);
    furi_string_free(Tid);
    furi_string_free(Res);
    furi_string_free(Mem);
    furi_string_free(Pc);
    furi_string_free(Crc);
}

/**
 * @brief      Fetch selected tag's info
 * @details    Populates the ViewWrite model. For ActionFromLive it reads the
 *             in-memory scanned tag; for ActionFromSaved it reads Saved_EPCs.txt
 *             by SelectedTagIndex. All file extracts are NULL-guarded.
 * @param      context  The context - The App
*/
void uhf_reader_fetch_selected_tag(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    //Unsaved live tag: use the in-memory model, never the saved file.
    if(App->ActionContext == ActionFromLive) {
        uhf_reader_fetch_live_tag(App);
        return;
    }

    //Allocate space for the FuriStrings used
    FuriString* TempStr = furi_string_alloc();
    FuriString* TempTag = furi_string_alloc();

    //Open the saved epcs file to extract the uhf tag info
    if(!flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FURI_LOG_E(TAG, "Failed to open Saved file");
        flipper_format_file_close(App->EpcFile);
    } else {
        furi_string_printf(TempStr, "Tag%ld", App->SelectedTagIndex);
        if(!flipper_format_read_string(App->EpcFile, furi_string_get_cstr(TempStr), TempTag)) {
            FURI_LOG_D(TAG, "Could not read tag %ld data", App->SelectedTagIndex);
            flipper_format_file_close(App->EpcFile);
        } else {
            //Grab the saved uhf tag info from the saved epcs file
            const char* InputString = furi_string_get_cstr(TempTag);
            uhf_set_from_extracted(App->EpcToWrite, extract_epc(InputString));
            uhf_set_from_extracted(App->EpcName, extract_name(InputString));

            //Set the write model uhf tag values accordingly
            bool redraw = true;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    uhf_set_from_extracted(Model->EpcValue, extract_epc(InputString));
                    uhf_set_from_extracted(Model->TidValue, extract_tid(InputString));
                    uhf_set_from_extracted(Model->ResValue, extract_res(InputString));
                    uhf_set_from_extracted(Model->MemValue, extract_mem(InputString));
                    uhf_set_from_extracted(Model->Pc, extract_pc(InputString));
                    uhf_set_from_extracted(Model->Crc, extract_crc(InputString));
                },
                redraw);
            //Close the file
            flipper_format_file_close(App->EpcFile);
        }
    }

    furi_string_free(TempTag);
    furi_string_free(TempStr);
}

/**
 * @brief      Configure the worker's write targeting for the current context.
 * @details    ActionFromLive -> Targeted: preload SelectedTag with the scanned
 *             tag's current EPC/PC/CRC so the write addresses that exact tag
 *             (and aborts after 10s if absent). ActionFromSaved -> single-poll.
 *             Call immediately before uhf_worker_start(...WriteSingle...).
 * @param      context  The context - The App
*/
void uhf_reader_prepare_write_target(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    if(App->ActionContext != ActionFromLive) {
        App->YRM100XWorker->Targeted = false;
        return;
    }

    App->YRM100XWorker->Targeted = true;
    UHFTag* Target = App->YRM100XWorker->SelectedTag;
    uhf_tag_reset(Target);

    uint8_t EpcBytes[EPC_MAX_BANK_SIZE] = {0};
    size_t EpcLen = 0;
    uint16_t PcArr[2] = {0};
    size_t PcLen = 0;
    uint16_t CrcArr[2] = {0};
    size_t CrcLen = 0;

    with_view_model(
        App->ViewWrite,
        UHFReaderWriteModel * Model,
        {
            hex_string_to_bytes(
                furi_string_get_cstr(Model->EpcValue), EpcBytes, sizeof(EpcBytes), &EpcLen);
            hex_string_to_uint16(furi_string_get_cstr(Model->Pc), PcArr, COUNT_OF(PcArr), &PcLen);
            hex_string_to_uint16(
                furi_string_get_cstr(Model->Crc), CrcArr, COUNT_OF(CrcArr), &CrcLen);
        },
        false);

    uhf_tag_set_epc(Target, EpcBytes, EpcLen * sizeof(uint8_t));
    uhf_tag_set_epc_pc(Target, PcArr[0]);
    uhf_tag_set_epc_crc(Target, CrcArr[0]);
}

/**
 * @brief      Callback for the uhf worker for killing.
 * @details    This function is called when the uhf worker is started for killing.
 * @param      event  The UHFWorkerEvent - UHFReaderApp object.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_kill_tag_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(event == UHFWorkerEventSuccess) {
        dolphin_deed(DolphinDeedNfcReadSuccess);

        //Stop blinking the led
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_success(App);

        //Set the boolean false that tracks which password is being used
        App->YRM100XWorker->KillPwd = false;

        //Reset the popup
        popup_reset(App->LockPopup);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
    } else if(event == UHFWorkerEventAborted) {
        //Set the boolean false that tracks which password is being used
        App->YRM100XWorker->KillPwd = false;
        //Stop blinking the led
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        uhf_notify_error(App);

        //Reset the popup
        popup_reset(App->LockPopup);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
    }
}
/**
 * @brief      Handles the kill menu.
 * @details    This function handles the kill password that is set from the kill screen
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/

/**
 * @brief      Handles the kill confirm menu.
 * @details    This function handles the password confirmation screen
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/
void uhf_reader_kill_confirm_password_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;
    Popup* PopupLock = App->LockPopup;
    popup_reset(PopupLock);
    popup_set_header(PopupLock, "Killing\nUHF\nTag!", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(PopupLock, 0, 3, &I_RFIDDolphinReceive_97x61);
    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
    // Temporary stack buffer; conversion helper returns heap memory that must be freed.
    char tempBuffer[9] = {0};
    char* converted = convert_to_hex_string(App->KillConfirmPwdTempBuffer, 4);
    if(converted) {
        snprintf(tempBuffer, sizeof(tempBuffer), "%s", converted);
        free(converted);
    }

    //Changing the read screen's power value to the one set in the configuration menu

    with_view_model(
        App->ViewWrite,
        UHFReaderWriteModel * Model,
        {
            furi_string_set(Model->ResValue, tempBuffer);

            if(App->ReaderConnected) {
                //This is where we should kill the tag.
                uint32_t returnResponse = 0;
                // uint16_t PcBytes[4];
                // size_t PcBytesLen;
                // uint16_t CrcBytes[4];
                // size_t CrcBytesLen;
                // uint8_t ResBytes
                //     [4]; //Technically can be up to 96 bits in length for epc gen 2. We only care about the first 8....
                // size_t ResBytesLen;

                //uhf_reader_fetch_selected_tag(App);
                uhf_reader_fetch_selected_tag(App);
                memset(App->ResBytes, 0, 8 * sizeof(uint8_t));
                memset(App->PcBytes, 0, 2 * sizeof(uint16_t));
                memset(App->CrcBytes, 0, 2 * sizeof(uint16_t));

                // Resetting the size_t variables to zero
                App->ResBytesLen = 0;
                App->PcBytesLen = 0;
                App->CrcBytesLen = 0;
                UHFTag* TempTag = App->YRM100XWorker->NewTag;

                uhf_tag_reset(TempTag);

                hex_string_to_uint16(
                    furi_string_get_cstr(Model->Pc), App->PcBytes, 2U, &App->PcBytesLen);
                hex_string_to_uint16(
                    furi_string_get_cstr(Model->Crc), App->CrcBytes, 2U, &App->CrcBytesLen);

                uint16_t combinedPc = App->PcBytesLen ? App->PcBytes[0] : 0U;
                uint16_t combinedCrc = App->CrcBytesLen ? App->CrcBytes[0] : 0U;
                hex_string_to_bytes(
                    tempBuffer, App->ResBytes, RESERVED_MAX_BANK_SIZE, &App->ResBytesLen);

                uhf_tag_set_kill_pwd(TempTag, App->ResBytes, 4);
                uhf_tag_set_epc_pc(TempTag, combinedPc);
                uhf_tag_set_epc_crc(TempTag, combinedCrc);

                // Set Select to this specific tag before killing (§2.11 requires it).
                // m100_set_select also sets Select mode to 0x02 automatically.
                // Load the EPC from the write model (already populated by
                // uhf_reader_fetch_selected_tag) into NewTag's epc bytes.
                {
                    uint8_t epc_buf[EPC_MAX_BANK_SIZE] = {0};
                    size_t epc_len = 0;
                    hex_string_to_bytes(
                        furi_string_get_cstr(Model->EpcValue), epc_buf, sizeof(epc_buf), &epc_len);
                    uhf_tag_set_epc(TempTag, epc_buf, epc_len);
                }
                // Kill (0x65), like Lock, does NOT re-send the Select itself —
                // it relies on the Select set immediately before it, and the tag
                // can brown out mid-handshake at range. So on every attempt we
                // re-issue the Select (§2.11) and then the Kill. Empty/checksum,
                // 0x13 "no response" and 0x12 "tag not found" are transient at
                // range, so retry them; only stop early on a definitive tag-level
                // outcome (success, wrong AP, kill pwd = 0).
                view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
                const int MAX_KILL_ATTEMPTS = 20;
                for(int kill_attempts = 0; kill_attempts < MAX_KILL_ATTEMPTS; kill_attempts++) {
                    // Re-Select this specific tag before each Kill attempt.
                    if(m100_set_select(App->YRM100XWorker->module, App->YRM100XWorker->NewTag) !=
                       M100SuccessResponse) {
                        returnResponse = M100NoTagResponse;
                        continue; // Select itself failed (RF transient) — retry.
                    }

                    returnResponse = m100_kill_tag(
                        App->YRM100XWorker->module,
                        bytes_to_uint32(App->KillConfirmPwdTempBuffer, 4));
                    if(returnResponse == M100SuccessResponse) {
                        uhf_notify_success(App);
                        break;
                    } else if(
                        returnResponse == M100EmptyResponse ||
                        returnResponse == M100ValidationFail ||
                        returnResponse == M100ChecksumFail ||
                        returnResponse == M100NoTagResponse) {
                        // Transient RF failure (brown-out at range) — retry.
                        continue;
                    } else {
                        // Definitive tag-level outcome (wrong AP, kill pwd = 0):
                        // stop retrying and report it.
                        uhf_notify_error(App);
                        break;
                    }
                }
                if(returnResponse != M100SuccessResponse && returnResponse != M100APWrong &&
                   returnResponse != M100MemoryOverrun) {
                    // Exhausted all retries on a transient failure.
                    uhf_notify_error(App);
                }
                notification_message(App->Notifications, &uhf_sequence_blink_stop);
                popup_reset(PopupLock);
                view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
            }
        },
        Redraw);

    //Switch back to the configuration view
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
}
/**
 * @brief      Kill Submenu Callback
 * @details    Handles the different submenu options for the kill tag menu.
 * @param      context    The UHFReaderApp - the app for working with variables
 * @param      index  The selected submenu index
*/
void uhf_reader_submenu_kill_callback(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(index) {
    //Case for the kill tag action — prompts the user to enter the kill password
    case UHFReaderSubmenuIndexKillTag:
        byte_input_set_header_text(App->KillConfirmInput, App->KillConfirmPasswordPlaceHolder);
        byte_input_set_result_callback(
            App->KillConfirmInput,
            uhf_reader_kill_confirm_password_updated,
            NULL,
            App,
            App->KillConfirmPwdTempBuffer,
            4);
        view_set_previous_callback(
            byte_input_get_view(App->KillConfirmInput), uhf_reader_navigation_kill_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKillConfirm);
        break;
    default:
        break;
    }
}

/**
 * @brief      Callback when the user exits the kill tag screen.
 * @details    This function is called when the user exits the kill tag screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_kill_exit_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

/**
 * @brief      Allocates kill confirm input view
 * @details    This function allocates the kill confirm byte input view
 * @param      context  The context - The App
*/
void kill_confirm_menu_alloc(UHFReaderApp* App) {
    App->KillConfirmInput = byte_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewKillConfirm, byte_input_get_view(App->KillConfirmInput));
    App->KillConfirmPwdTempBuffer = (uint8_t*)malloc(4);
}

/**
 * @brief      Allocates the kill tag view.
 * @details    This function allocates all variables for the kill tag view.
 * @param      app  The UHFReaderApp object.
*/
void view_kill_alloc(UHFReaderApp* App) {
    //Allocate the submenu for the kill menu
    App->SubmenuKillActions = submenu_alloc();
    submenu_set_header(App->SubmenuKillActions, "Kill Tag Options: ");

    //Creating placeholders for the views used for the kill feature
    App->KillPasswordPlaceHolder = strdup("Enter Kill Password!");
    App->KillConfirmPasswordPlaceHolder = strdup("Confirm Kill Password!");
    App->DefaultKillPassword = strdup("00000000");

    //Allocate the kill confirm input view
    kill_confirm_menu_alloc(App);
    submenu_add_item(
        App->SubmenuKillActions,
        "Kill Tag (Permanent)",
        UHFReaderSubmenuIndexKillTag,
        uhf_reader_submenu_kill_callback,
        App);
    view_set_previous_callback(
        submenu_get_view(App->SubmenuKillActions), uhf_reader_navigation_kill_exit_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewKill, submenu_get_view(App->SubmenuKillActions));
}

/**
 * @brief      Frees the kill tag view.
 * @details    This function frees all variables for the kill tag view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_kill_free(UHFReaderApp* App) {
    if(!App || !App->SubmenuKillActions) return;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewKillConfirm);
    byte_input_free(App->KillConfirmInput);
    App->KillConfirmInput = NULL;

    free(App->KillConfirmPwdTempBuffer);
    App->KillConfirmPwdTempBuffer = NULL;

    free(App->KillPasswordPlaceHolder);
    App->KillPasswordPlaceHolder = NULL;
    free(App->KillConfirmPasswordPlaceHolder);
    App->KillConfirmPasswordPlaceHolder = NULL;
    free(App->DefaultKillPassword);
    App->DefaultKillPassword = NULL;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewKill);
    submenu_free(App->SubmenuKillActions);
    App->SubmenuKillActions = NULL;
}
