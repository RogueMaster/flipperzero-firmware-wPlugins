#include "view_lock.h"
#include "view_kill.h"
/**
 * @brief      Callback when the user exits the lock screen.
 * @details    This function is called when the user exits the lock screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_lock_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewLock;
}

/**
 * @brief      Callback for the uhf worker for locking.
 * @details    This function is called when the uhf worker is started for locking.
 * @param      event  The UHFWorkerEvent - UHFReaderApp object.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_access_tag_worker_callback(UHFWorkerEvent event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(event == UHFWorkerEventSuccess) {
        dolphin_deed(DolphinDeedNfcReadSuccess);
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        notification_message(App->Notifications, &sequence_success);
        App->YRM100XWorker->AccessPwd = false;
        popup_reset(App->LockPopup);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLock);
    } else if(event == UHFWorkerEventAborted) {
        App->YRM100XWorker->AccessPwd = false;
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        notification_message(App->Notifications, &sequence_error);
        popup_reset(App->LockPopup);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLock);
    }
}
/**
 * @brief      Allocates the access password input view 
 * @details    Allocates the set access password input view 
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/
void access_password_menu_alloc(UHFReaderApp* App) {
    App->SetApInput = byte_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSetAccessPwd, byte_input_get_view(App->SetApInput));
    App->SetPwdTempBuffer = (uint8_t*)malloc(4);
}
/**
 * @brief      Handles the set access password view.
 * @details    Used when the access password is set through the GUI
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/
void uhf_reader_access_password_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;
    // Temporary buffer to hold the converted string
    char* tempBuffer = (char*)malloc(8);

    snprintf(tempBuffer, 8, "%s", convert_to_hex_string(App->SetPwdTempBuffer, 4));

    if(App->UHFModuleType != YRM100X_MODULE) {
        //TODO: ADD SUPPORT FOR M6E and M7E
        uart_helper_send(App->UartHelper, "SETPWD\n", 7);

    } else {
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                furi_string_set(Model->ResValue, tempBuffer);

                if(App->ReaderConnected) {
                    //This is where we should write...
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

                    //Set the reserved bank value
                    bool redraw = true;
                    with_view_model(
                        App->ViewWrite,
                        UHFReaderWriteModel * Model,
                        {
                            hex_string_to_bytes(
                                furi_string_get_cstr(Model->ResValue),
                                App->ResBytes,
                                &App->ResBytesLen);
                            uhf_tag_set_kill_pwd(TempTag, App->ResBytes, 4);
                        },
                        redraw);

                    uhf_tag_set_access_pwd(TempTag, App->SetPwdTempBuffer, 4);

                    m100_enable_write_mask(App->YRM100XWorker->module, WRITE_RFU);

                    uhf_tag_set_epc_pc(TempTag, combinedPc);
                    uhf_tag_set_epc_crc(TempTag, combinedCrc);
                    App->YRM100XWorker->AccessPwd = true;

                    uhf_worker_start(
                        App->YRM100XWorker,
                        UHFWorkerStateWriteSingle,
                        uhf_access_tag_worker_callback,
                        App);
                    notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
                }
            },
            Redraw);
    }
    //Switch to the popup
    Popup* PopupLock = App->LockPopup;
    popup_set_header(PopupLock, "Setting\nAccess\nPassword", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(PopupLock, 0, 3, &I_RFIDDolphinReceive_97x61);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
}

/**
 * @brief      Callback when the user exits the lock screen.
 * @details    This function is called when the user exits the lock screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_lock_exit_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

/**
 * @brief      Maps lock type to string 
 * @details    This function is used to change the lock type value for the lock popups.
 * @param      type LockType - The locktype 
 * @return     the string representing the lock type action
*/
const char* get_lock_bank_type_string(LockType type) {
    switch(type) {
    case Lock:
        return "Locking";
    case Unlock:
        return "Unlocking";
    case PermaUnlock:
        return "Perma Unlocking";
    case PermaLock:
        return "Perma Locking";
    default:
        return "Unknown";
    }
}

/**
 * @brief      Maps bank type to string 
 * @details    This function is used to change the bank type value for the lock popups.
 * @param      bank BankType - The bank type 
 * @return     the string representing the bank type
*/
const char* get_memory_bank_string(BankType bank) {
    switch(bank) {
    case ReservedBank:
        return "Reserved\nBank";
    case EPCBank:
        return "EPC\nBank";
    case TIDBank:
        return "TID\nBank";
    case UserBank:
        return "User\nBank";
    case KillPwd:
        return "Kill\nPassword";
    case AccessPwd:
        return "Access\nPassword";
    case FileZero:
        return "User\nBank";
    default:
        return "Unknown\nBank";
    }
}
/**
 * @brief      Handles the lock items clicked
 * @details    Handles hanldes the lock actions 
 * @param      context, index - context used for UHFReaderApp, index used for state check.
*/
void uhf_reader_lock_item_clicked(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    Popup* PopupLock = App->LockPopup;
    uint32_t returnResponse = 0;
    char* header = malloc(68 * sizeof(char));
    index++;

    //Check if the set AP menu is being selected
    if(index == 1) {
        // Header to display on the access password input screen.
        byte_input_set_header_text(App->SetApInput, App->SetAccessPasswordPlaceHolder);
        byte_input_set_result_callback(
            App->SetApInput,
            uhf_reader_access_password_updated,
            NULL,
            App,
            App->SetPwdTempBuffer,
            App->KillPwdInputBufferSize);
        view_set_previous_callback(
            byte_input_get_view(App->SetApInput), uhf_reader_navigation_lock_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetAccessPwd);
    } else if(index == 4) {
        // Create a dynamic string for the popup header
        snprintf(
            header,
            68,
            "%s\n%s",
            get_lock_bank_type_string(App->DefaultLockType),
            get_memory_bank_string(App->DefaultLockBank));

        // Set the popup header with the dynamic values and switch to it
        popup_set_header(PopupLock, header, 68, 30, AlignLeft, AlignTop);
        popup_set_icon(PopupLock, 0, 3, &I_RFIDDolphinReceive_97x61);
        notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);

        while(true) {
            returnResponse = m100_lock_label_data(
                App->YRM100XWorker->module,
                App->DefaultLockBank,
                App->YRM100XWorker->DefaultAP,
                App->DefaultLockType);
            if(returnResponse == M100SuccessResponse) {
                notification_message(App->Notifications, &sequence_success);
                dolphin_deed(DolphinDeedNfcReadSuccess);
                break;
            } else if(returnResponse == M100APWrong) {
                notification_message(App->Notifications, &sequence_error);
                break;
            }
            continue;
        }
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        popup_reset(App->LockPopup);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLock);
        free(header);
    }
}

/**
 * @brief      Handles the UHF Bank Selection
 * @details    This function handles switching between different memory banks for locking the UHF RFID Tag
 * @param      item  VariableItem - the current selection for the bank.
*/
void uhf_reader_setting_lock_bank_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(Index == 1) {
        App->DefaultLockBank = AccessPwd;
    } else if(Index == 2) {
        App->DefaultLockBank = EPCBank;
    } else if(Index == 3) {
        App->DefaultLockBank = TIDBank;
    } else if(Index == 4) {
        App->DefaultLockBank = FileZero;
    } else {
        App->DefaultLockBank = KillPwd;
    }
    variable_item_set_current_value_text(Item, App->SettingLockBankNames[Index]);
}

/**
 * @brief      Handles the UHF Lock Action Selection
 * @details    This function handles switching between different supported lock actions for the selected UHF RFID Memory Bank.
 * @param      item  VariableItem - the current selection for the lock action
*/
void uhf_reader_setting_lock_action_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);
    if(Index == 1) {
        App->DefaultLockType = PermaUnlock;
    } else if(Index == 2) {
        App->DefaultLockType = Lock;
    } else if(Index == 3) {
        App->DefaultLockType = PermaLock;
    } else {
        App->DefaultLockType = Unlock;
    }
    variable_item_set_current_value_text(Item, App->SettingLockActionNames[Index]);
}
/**
 * @brief      Allocates the lock view
 * @details    This function allocates all variables for the lock view.
 * @param      app  The UHFReaderApp object.
*/
void view_lock_alloc(UHFReaderApp* App) {
    //Setting variables
    App->SettingApLabel = "Set AP";
    App->SetAccessPasswordPlaceHolder = strdup("Enter Access Password!");
    App->SettingApDefaultPassword = strdup("00000000");

    //Options for different banks to lock following the Gen2 Protocol Standard https://www.gs1.org/sites/default/files/docs/epc/Gen2_Protocol_Standard.pdf
    App->SettingLockBankConfigLabel = "Memory Bank";
    App->SettingLockBankValues[0] = 1;
    App->SettingLockBankValues[1] = 2;
    App->SettingLockBankValues[2] = 3;
    App->SettingLockBankValues[3] = 4;
    App->SettingLockBankValues[4] = 5;
    App->SettingLockBankNames[0] = "Kill";
    App->SettingLockBankNames[1] = "AP";
    App->SettingLockBankNames[2] = "EPC";
    App->SettingLockBankNames[3] = "TID";
    App->SettingLockBankNames[4] = "User";
    App->DefaultLockBank = KillPwd;

    //Options for the lock mode
    App->SettingLockActionConfigLabel = "Lock Mode";
    App->SettingLockActionValues[0] = 1;
    App->SettingLockActionValues[1] = 2;
    App->SettingLockActionValues[2] = 3;
    App->SettingLockActionValues[3] = 4;
    App->SettingLockActionNames[0] = "Unlock";
    App->SettingLockActionNames[1] = "Perm-U";
    App->SettingLockActionNames[2] = "Lock";
    App->SettingLockActionNames[3] = "Perm-L";
    App->DefaultLockType = Unlock;
    //The Button for executing the desired lock command and storing the output
    App->SettingLockExecuteConfigLabel = "Execute";
    App->SettingLockExecuteResult = strdup("Press Me!");

    //Allocating the set access password menu
    access_password_menu_alloc(App);

    //Allocating the popup shown when locking the tags
    App->LockPopup = popup_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewLockPopup, popup_get_view(App->LockPopup));

    //Creating the variable item list for the lock menu options
    App->VariableItemListLock = variable_item_list_alloc();
    variable_item_list_reset(App->VariableItemListLock);

    //The set access password menu
    App->DefaultLockAccessPwdStr = furi_string_alloc_set(App->SettingApDefaultPassword);
    App->SettingLockApPwdItem =
        variable_item_list_add(App->VariableItemListLock, App->SettingApLabel, 1, NULL, NULL);
    variable_item_set_current_value_text(
        App->SettingLockApPwdItem, furi_string_get_cstr(App->DefaultLockAccessPwdStr));
    variable_item_list_set_enter_callback(
        App->VariableItemListLock, uhf_reader_lock_item_clicked, App);

    VariableItem* Item = variable_item_list_add(
        App->VariableItemListLock,
        App->SettingLockBankConfigLabel,
        COUNT_OF(App->SettingLockBankValues),
        uhf_reader_setting_lock_bank_change,
        App);

    //Creating the default index for setting one which is the connection status
    App->SettingLockBankIndex = 0;
    variable_item_set_current_value_index(Item, App->SettingLockBankIndex);
    variable_item_set_current_value_text(
        Item, App->SettingLockBankNames[App->SettingLockBankIndex]);

    VariableItem* ItemLock = variable_item_list_add(
        App->VariableItemListLock,
        App->SettingLockActionConfigLabel,
        COUNT_OF(App->SettingLockActionValues),
        uhf_reader_setting_lock_action_change,
        App);

    //Creating the default index for setting one which is the connection status
    App->SettingLockActionIndex = 0;
    variable_item_set_current_value_index(ItemLock, App->SettingLockActionIndex);
    variable_item_set_current_value_text(
        ItemLock, App->SettingLockActionNames[App->SettingLockActionIndex]);

    //The execute button and result
    App->DefaultLockResultStr = furi_string_alloc_set(App->SettingLockExecuteResult);
    App->SettingLockResultItem = variable_item_list_add(
        App->VariableItemListLock, App->SettingLockExecuteConfigLabel, 1, NULL, NULL);
    variable_item_set_current_value_text(
        App->SettingLockResultItem, furi_string_get_cstr(App->DefaultLockResultStr));
    variable_item_list_set_enter_callback(
        App->VariableItemListLock, uhf_reader_lock_item_clicked, App);
    view_set_previous_callback(
        variable_item_list_get_view(App->VariableItemListLock),
        uhf_reader_navigation_lock_exit_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher,
        UHFReaderViewLock,
        variable_item_list_get_view(App->VariableItemListLock));
}

/**
 * @brief      Frees the tag action view.
 * @details    This function frees all variables for the tag actions view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_lock_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewLockPopup);
    popup_free(App->LockPopup);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSetAccessPwd);
    byte_input_free(App->SetApInput);
    free(App->SetPwdTempBuffer);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewLock);
    variable_item_list_free(App->VariableItemListLock);
}
