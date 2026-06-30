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

/**
 * @brief      Fetch selected tag's info
 * @details    This function populates the app's tag variables based on the selected saved tag 
 * @param      context  The context - The App
*/
void uhf_reader_fetch_selected_tag(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

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
        } else {
            //Grab the saved uhf tag info from the saved epcs file
            const char* InputString = furi_string_get_cstr(TempTag);
            furi_string_set(App->EpcToWrite, extract_epc(InputString));
            furi_string_set(App->EpcName, extract_name(InputString));

            //Set the write model uhf tag values accordingly
            bool redraw = true;
            with_view_model(
                App->ViewWrite,
                UHFReaderWriteModel * Model,
                {
                    furi_string_set(Model->EpcValue, extract_epc(InputString));
                    furi_string_set(Model->TidValue, extract_tid(InputString));
                    furi_string_set(Model->ResValue, extract_res(InputString));
                    furi_string_set(Model->MemValue, extract_mem(InputString));
                    furi_string_set(Model->Pc, extract_pc(InputString));
                    furi_string_set(Model->Crc, extract_crc(InputString));
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
 * @brief      Callback for the uhf worker for writing.
 * @details    This function is called when the uhf worker is started for writing.
 * @param      event  The UHFWorkerEvent - UHFReaderApp object.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_kill_tag_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(event == UHFWorkerEventSuccess) {
        dolphin_deed(DolphinDeedNfcReadSuccess);

        //Stop blinking the led
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        notification_message(App->Notifications, &sequence_success);

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
        notification_message(App->Notifications, &sequence_error);

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
void uhf_reader_kill_password_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;
    // Temporary buffer to hold the converted string
    char* tempBuffer = (char*)malloc(12);

    snprintf(tempBuffer, 12, "%s", convert_to_hex_string(App->KillPwdTempBuffer, 4));

    //Changing the read screen's power value to the one set in the configuration menu
    if(App->UHFModuleType != YRM100X_MODULE) {
        //TODO: ADD SUPPORT FOR M6E and M7E

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                //Send the power command to the RPi Zero
                uart_helper_send(App->UartHelper, "SETKILLPWD\n", 11);

                //Set the current power determined by user
                furi_string_set(Model->SettingKillPwd, tempBuffer);

                //Send the power value to the RPi Zero
                uart_helper_send_string(App->UartHelper, Model->SettingKillPwd);
            },
            Redraw);

    } else {
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                //Set the current power determined by user
                furi_string_set(Model->SettingKillPwd, tempBuffer);

                if(App->ReaderConnected) {
                    uhf_reader_fetch_selected_tag(App);
                    memset(App->ResBytes, 0, 8 * sizeof(uint8_t));
                    memset(App->PcBytes, 0, 2 * sizeof(uint16_t));
                    memset(App->CrcBytes, 0, 2 * sizeof(uint16_t));

                    // Resetting the size_t variables to zero
                    App->ResBytesLen = 0;
                    App->PcBytesLen = 0;
                    App->CrcBytesLen = 0;
                    //uint16_t PcBytes[4];
                    //size_t PcBytesLen;
                    // uint16_t CrcBytes[4];
                    //size_t CrcBytesLen;
                    //uint8_t ResBytes
                    //  [4]; //Technically can be up to 96 bits in length for epc gen 2. We only care about the first 8....
                    //size_t ResBytesLen;

                    //uhf_reader_fetch_selected_tag(App);
                    UHFTag* TempTag = App->YRM100XWorker->NewTag;

                    uhf_tag_reset(TempTag);

                    hex_string_to_uint16(
                        furi_string_get_cstr(Model->Pc), App->PcBytes, &App->PcBytesLen);
                    hex_string_to_uint16(
                        furi_string_get_cstr(Model->Crc), App->CrcBytes, &App->CrcBytesLen);

                    uint16_t combinedPc = 0;
                    uint16_t combinedCrc = 0;

                    for(size_t i = 0; i < 4; i++) {
                        combinedPc |= App->PcBytes[i];
                    }

                    for(size_t i = 0; i < 4; i++) {
                        combinedCrc |= App->CrcBytes[i];
                    }
                    hex_string_to_bytes(tempBuffer, App->ResBytes, &App->ResBytesLen);
                    uhf_tag_set_kill_pwd(TempTag, App->KillPwdTempBuffer, 4);
                    uhf_tag_set_epc_pc(TempTag, combinedPc);
                    uhf_tag_set_epc_crc(TempTag, combinedCrc);
                    m100_enable_write_mask(App->YRM100XWorker->module, WRITE_RFU);
                    App->YRM100XWorker->KillPwd = true;

                    uhf_worker_start(
                        App->YRM100XWorker,
                        UHFWorkerStateWriteSingle,
                        uhf_kill_tag_worker_callback,
                        App);
                    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
                }
            },
            Redraw);
    }
    Popup* PopupLock = App->LockPopup;
    popup_set_header(PopupLock, "Setting\nKill\nPassword", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(PopupLock, 0, 3, &I_RFIDDolphinReceive_97x61);

    free(tempBuffer);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
}

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
    // Temporary buffer to hold the converted string
    char* tempBuffer = (char*)malloc(9);

    snprintf(tempBuffer, 9, "%s", convert_to_hex_string(App->KillConfirmPwdTempBuffer, 4));

    //Changing the read screen's power value to the one set in the configuration menu
    if(App->UHFModuleType != YRM100X_MODULE) {
        //TODO: ADD SUPPORT FOR M6E and M7E
        uart_helper_send(App->UartHelper, "KILLTAG\n", 11);
        // view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);

    } else {
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
                        furi_string_get_cstr(Model->Pc), App->PcBytes, &App->PcBytesLen);
                    hex_string_to_uint16(
                        furi_string_get_cstr(Model->Crc), App->CrcBytes, &App->CrcBytesLen);

                    uint16_t combinedPc = 0;
                    uint16_t combinedCrc = 0;

                    for(size_t i = 0; i < 4; i++) {
                        combinedPc |= App->PcBytes[i];
                    }

                    for(size_t i = 0; i < 4; i++) {
                        combinedCrc |= App->CrcBytes[i];
                    }
                    hex_string_to_bytes(tempBuffer, App->ResBytes, &App->ResBytesLen);

                    uhf_tag_set_kill_pwd(TempTag, App->ResBytes, 4);
                    uhf_tag_set_epc_pc(TempTag, combinedPc);
                    uhf_tag_set_epc_crc(TempTag, combinedCrc);

                    // Start worker
                    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
                    while(true) {
                        returnResponse = m100_kill_tag(
                            App->YRM100XWorker->module,
                            bytes_to_uint32(App->KillConfirmPwdTempBuffer, 4));
                        if(returnResponse == M100SuccessResponse) {
                            notification_message(App->Notifications, &sequence_success);
                            break;
                        } else if(returnResponse == M100APWrong) {
                            notification_message(App->Notifications, &sequence_error);
                            break;
                        }
                        continue;
                    }
                    notification_message(App->Notifications, &uhf_sequence_blink_stop);
                    popup_reset(PopupLock);
                    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
                }
            },
            Redraw);
    }

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
    //Case for setting the kill password
    case UHFReaderSubmenuIndexSetKillPwd:
        //Using a byte input type
        byte_input_set_header_text(App->KillInput, App->KillPasswordPlaceHolder);
        byte_input_set_result_callback(
            App->KillInput,
            uhf_reader_kill_password_updated,
            NULL,
            App,
            App->KillPwdTempBuffer,
            App->KillPwdInputBufferSize);
        view_set_previous_callback(
            byte_input_get_view(App->KillInput), uhf_reader_navigation_kill_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetKillPwd);
        break;

    //Case for the kill tag action
    case UHFReaderSubmenuIndexKillTag:

        byte_input_set_header_text(App->KillConfirmInput, App->KillConfirmPasswordPlaceHolder);
        byte_input_set_result_callback(
            App->KillConfirmInput,
            uhf_reader_kill_confirm_password_updated,
            NULL,
            App,
            App->KillConfirmPwdTempBuffer,
            App->KillPwdInputBufferSize);
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
 * @brief      Allocates kill input view 
 * @details    This function allocates the kill byte input view 
 * @param      context  The context - The App
*/
void kill_menu_alloc(UHFReaderApp* App) {
    App->KillInput = byte_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSetKillPwd, byte_input_get_view(App->KillInput));
    App->KillPwdInputBufferSize = 4;
    App->KillPwdTempBuffer = (uint8_t*)malloc(App->KillPwdInputBufferSize);
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

    //Allocate the kill input views
    kill_menu_alloc(App);
    kill_confirm_menu_alloc(App);
    submenu_add_item(
        App->SubmenuKillActions,
        "Set Kill Password",
        UHFReaderSubmenuIndexSetKillPwd,
        uhf_reader_submenu_kill_callback,
        App);
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
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSetKillPwd);
    byte_input_free(App->KillInput);
    free(App->KillPwdTempBuffer);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewKillConfirm);
    byte_input_free(App->KillConfirmInput);
    free(App->KillConfirmPwdTempBuffer);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewKill);
    submenu_free(App->SubmenuKillActions);
}
