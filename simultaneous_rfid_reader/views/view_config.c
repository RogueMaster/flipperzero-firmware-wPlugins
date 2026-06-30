#include "view_config.h"


/**
 * @brief      Callback for returning to the configuration screen.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
*/
static void locked_popup_back_callback(void* context) {
    UHFReaderApp* app = context;
    view_dispatcher_switch_to_view(app->ViewDispatcher, UHFReaderViewConfigure);
}

/**
 * @brief      Shows a notification when the reader is not connected
 * @details    This function shows a notification when the reader is not connected and the user tries to change a setting.
 * @param      App  The UHFReaderApp - used to allocate app variables and views.
*/
static void show_locked_notification(UHFReaderApp* App) {
    notification_message(App->Notifications, &sequence_error);
    
    Popup* popup = App->LockPopup;
    popup_reset(popup);
    popup_set_header(popup, "Connect\nTo Reader\nFirst!", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_WarningDolphin_45x42);
    
    // Set timeout for 2 seconds
    popup_enable_timeout(popup);
    popup_set_timeout(popup, 2000);
    popup_set_context(popup, App);
    popup_set_callback(popup, locked_popup_back_callback);
    
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_config_submenu_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

/**
 * @brief      Allocates the configuration menu
 * @details    This function allocates all views and variables related to the configuration menu.
 * @param      app  The UHFReaderApp - used to allocate app variables and views.
*/
void view_config_alloc(UHFReaderApp* App) {
    //Allocate the power input menu
    power_menu_alloc(App);
    ap_menu_alloc(App);
    
    //Creating the variable item list
    App->VariableItemListConfig = variable_item_list_alloc();
    variable_item_list_reset(App->VariableItemListConfig);

    //Initializing configuration setting variables
    App->Setting1Values[0] = 1;
    App->Setting1Values[1] = 2;
    App->Setting1Names[0] = "Disconnected";
    App->Setting1Names[1] = "Connected";
    App->ReaderConnected = false;
    App->Setting1ConfigLabel = "Connection";
    App->Setting2ConfigLabel = "Power Level";
    App->Setting2EntryText = "Enter Value In Range 0-2700";
    App->Setting2DefaultValue = "1500";
    App->Setting3Values[0] = 1;
    App->Setting3Values[1] = 2;
    App->Setting3Names[0] = "Internal";
    App->Setting3Names[1] = "External";
    App->Setting3ConfigLabel = "Antenna";

    //Baud rate values (just for YRM100 Module for now!)
    App->SettingBaudValues[0] = 1;
    App->SettingBaudValues[1] = 2;
    App->SettingBaudValues[2] = 3;
    App->SettingBaudNames[0] = "9600";
    App->SettingBaudNames[2] = "384000";
    App->SettingBaudNames[1] = "115200";
    App->SettingBaudConfigLabel = "Baud Rate";
    App->UHFBaudRate = 115200;

    //Setting the module values for the config menu
    App->SettingModuleValues[0] = 1;
    App->SettingModuleValues[1] = 2;
    App->SettingModuleValues[2] = 3;
    App->SettingModuleNames[0] = "M6e";
    App->SettingModuleNames[1] = "YRM100";
    App->SettingModuleNames[2] = "M7e";
    App->SettingModuleConfigLabel = "UHF Module";
    App->UHFModuleType = M6E_NANO_MODULE;
    
    App->SettingSavingValues[0] = 1;
    App->SettingSavingValues[1] = 2;
    App->SettingSavingNames[0] = "No";
    App->SettingSavingNames[1] = "Yes";
    App->SettingSavingConfigLabel = "Save on Write";
    App->UHFSaveType = NO_SAVE_ON_WRITE;
    
    //Setting the available regions
    App->SettingRegionValues[0] = 1;
    App->SettingRegionValues[1] = 2;
    App->SettingRegionValues[2] = 3;
    App->SettingRegionValues[3] = 4;
    App->SettingRegionValues[4] = 5;
    App->SettingRegionNames[0] = "USA";
    App->SettingRegionNames[1] = "EU";
    App->SettingRegionNames[2] = "Korea";
    App->SettingRegionNames[3] = "China 800";
    App->SettingRegionNames[4] = "China 900";
    App->SettingRegionConfigLabel = "Region";
    App->UHFRegionType = USA_REGION;

    //Setting the config menu labels for the default read access password
    App->ReadAccessPasswordLabel = strdup("Default AP");
    App->AccessPasswordPlaceHolder = strdup("Enter Access Password!");
    App->DefaultAccessPassword = strdup("00000000");

    // Add setting 1 to variable item list
    VariableItem* Item = variable_item_list_add(
        App->VariableItemListConfig,
        App->Setting1ConfigLabel,
        COUNT_OF(App->Setting1Values),
        uhf_reader_setting_1_change,
        App);

    //Creating the default index for setting one which is the connection status
    App->Setting1Index = 0;
    variable_item_set_current_value_index(Item, App->Setting1Index);
    variable_item_set_current_value_text(Item, App->Setting1Names[App->Setting1Index]);

    // Store module selection reference
    App->ModuleSelectionItem = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingModuleConfigLabel,
        COUNT_OF(App->SettingModuleValues),
        uhf_reader_module_setting_change,
        App);

    //Default index for the module selection option
    App->SettingModuleIndex = 0;
    variable_item_set_current_value_index(App->ModuleSelectionItem, App->SettingModuleIndex);
    variable_item_set_current_value_text(
        App->ModuleSelectionItem, App->SettingModuleNames[App->SettingModuleIndex]);
    
     // Add other items and track them for locking
     App->num_items = 5; // Adjust based on total number of lockable items
     App->item_locks = malloc(sizeof(VariableItemLock) *5);
     
     // Initialize all items as locked except module and save
     for(size_t i = 0; i < App->num_items; i++) {
         App->item_locks[i].locked = true;
     }

    //Creating the default power value
    App->Setting2PowerStr = furi_string_alloc_set(App->Setting2DefaultValue);
    App->Setting2Item = variable_item_list_add(
        App->VariableItemListConfig, App->Setting2ConfigLabel, 1, NULL, NULL);
    variable_item_list_set_enter_callback(
        App->VariableItemListConfig, uhf_reader_setting_item_clicked, App);
    variable_item_set_current_value_text(App->Setting2Item, "LOCKED");
    

    App->BaudSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingBaudConfigLabel,
        COUNT_OF(App->SettingBaudValues),
        uhf_reader_baud_setting_change,
        App);
    
    //Default index for the baud selection option
    App->SettingBaudIndex = 2;
    variable_item_set_current_value_index(App->BaudSelection, App->SettingBaudIndex);
    variable_item_set_current_value_text(App->BaudSelection, "LOCKED");

    
    App->RegionSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingRegionConfigLabel,
        COUNT_OF(App->SettingRegionValues),
        uhf_reader_region_setting_change,
        App);

    //Default index for the baud selection option
    App->SettingRegionIndex = 0;
    variable_item_set_current_value_index(App->RegionSelection, App->SettingRegionIndex);
    variable_item_set_current_value_text(App->RegionSelection, "LOCKED");
    //Default access password input for reading and writing to the tag, or locking
    App->DefaultAccessPwdStr = furi_string_alloc_set(App->DefaultAccessPassword);
    App->SettingApPwdItem = variable_item_list_add(
        App->VariableItemListConfig,  App->ReadAccessPasswordLabel, 1, NULL, NULL);
    variable_item_list_set_enter_callback(
        App->VariableItemListConfig, uhf_reader_setting_item_clicked, App);
    variable_item_set_current_value_text(App->SettingApPwdItem, "LOCKED");
    VariableItem* SavingSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingSavingConfigLabel,
        COUNT_OF(App->SettingSavingValues),
        uhf_reader_save_setting_change,
        App);

    //Default index for the module selection option
    App->SettingSavingIndex = 0;
    variable_item_set_current_value_index(SavingSelection, App->SettingSavingIndex);
    variable_item_set_current_value_text(
        SavingSelection, App->SettingSavingNames[App->SettingSavingIndex]);

    // Add setting 3 to variable item list
    App->AntennaSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->Setting3ConfigLabel,
        COUNT_OF(App->Setting3Values),
        uhf_reader_setting_3_change,
        App);

    //Default index for the antenna selection option
    App->Setting3Index = 0;
    variable_item_set_current_value_index(App->AntennaSelection, App->Setting3Index);
    variable_item_set_current_value_text(App->AntennaSelection, "LOCKED");
    //Setting previous callback
    view_set_previous_callback(
        variable_item_list_get_view(App->VariableItemListConfig),
        uhf_reader_navigation_config_submenu_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher,
        UHFReaderViewConfigure,
        variable_item_list_get_view(App->VariableItemListConfig));
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_configure_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewConfigure;
}

/**
 * @brief      Handles the connection setting
 * @details    Attempts to connect/disconnect from the reader.
 * @param      item  VariableItem - the current selection for connect values.
*/
void uhf_reader_setting_1_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);

    //Getting the index
    uint8_t Index = variable_item_get_current_value_index(Item);

    //Will eventually do some sort of check to confirm successful connection
    if(App->ReaderConnected == false) {
        if(App->UHFModuleType != YRM100X_MODULE){
            uart_helper_send(App->UartHelper, "C\n", 2);
        }
        
        App->ReaderConnected = true;
         // Unlock all items except module and save selection
         for(size_t i = 0; i < App->num_items; i++) {
            App->item_locks[i].locked = false;
        }

        // Update UI for all items to show unlocked state
        
        //Default index for the baud selection option
        App->SettingBaudIndex = 2;
        App->SettingRegionIndex = 0;
        App->Setting3Index = 0;
        variable_item_set_current_value_index(App->RegionSelection, App->SettingRegionIndex);
        variable_item_set_current_value_index(App->BaudSelection, App->SettingBaudIndex);
        variable_item_set_current_value_index(App->AntennaSelection, App->Setting3Index);
    
        variable_item_set_current_value_text(
            App->Setting2Item, furi_string_get_cstr(App->Setting2PowerStr));
        variable_item_list_set_enter_callback(
            App->VariableItemListConfig, uhf_reader_setting_item_clicked, App);
        
        variable_item_set_current_value_text(
            App->BaudSelection, App->SettingBaudNames[App->SettingBaudIndex]);
        variable_item_set_current_value_text(
            App->RegionSelection, App->SettingRegionNames[App->SettingRegionIndex]);
        
        variable_item_set_current_value_text(
            App->SettingApPwdItem, furi_string_get_cstr(App->DefaultAccessPwdStr));
        variable_item_list_set_enter_callback(
            App->VariableItemListConfig, uhf_reader_setting_item_clicked, App);
        
        variable_item_set_current_value_text(App->AntennaSelection, App->Setting3Names[App->Setting3Index]); 
       

    } else {
        if(App->UHFModuleType != YRM100X_MODULE){
            uart_helper_send(App->UartHelper, "D\n", 2);
        }
        App->ReaderConnected = false;
        // Lock all items except module and save
        for(size_t i = 0; i < App->num_items; i++) {
            App->item_locks[i].locked = true;
        }
        variable_item_set_current_value_text(App->Setting2Item, "LOCKED");
        variable_item_set_current_value_text(App->BaudSelection, "LOCKED");
        variable_item_set_current_value_text(App->RegionSelection, "LOCKED");
        variable_item_set_current_value_text(App->SettingApPwdItem, "LOCKED");
        variable_item_set_current_value_text(App->AntennaSelection, "LOCKED");
    }

    //Setting the current setting value for both the read and write screens
    variable_item_set_current_value_text(Item, App->Setting1Names[Index]);
    UHFReaderConfigModel* ModelRead = view_get_model(App->ViewRead);
    ModelRead->Setting1Index = Index;
    furi_string_set(ModelRead->Setting1Value, App->Setting1Names[Index]);
    UHFReaderWriteModel* ModelWrite = view_get_model(App->ViewWrite);
    ModelWrite->Setting1Index = Index;
    furi_string_set(ModelWrite->Setting1Value, App->Setting1Names[Index]);
}

/**
 * @brief      Handles the power menu.
 * @details    This function handles the power value that is set from the configuration screen and sends it to the RPi Zero via UART.
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/
void uhf_reader_setting_2_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;

    //Changing the read screen's power value to the one set in the configuration menu
    if(App->UHFModuleType != YRM100X_MODULE) {
        uint32_t power_value = (uint32_t)atoi(App->TempBuffer);

        // Validate power value is between 1 and 2700 inclusive
        if(power_value >= 1 && power_value <= 2700) {
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * Model,
                {
                    //Send the power command to the RPi Zero
                    uart_helper_send(App->UartHelper, "POWER\n", 6);

                    //Set the current power determined by user
                    furi_string_set(Model->Setting2Power, App->TempBuffer);

                    //Send the power value to the RPi Zero
                    uart_helper_send_string(App->UartHelper, Model->Setting2Power);

                    //Update the power value in the configuration screen
                    variable_item_set_current_value_text(
                        App->Setting2Item, furi_string_get_cstr(Model->Setting2Power));
                },
                Redraw);
           
        }
    } else {
        //Set the power of the YRM100X Here!!!!
        uint16_t power_value = (uint16_t)atoi(App->TempBuffer);

        // Validate power value is between 1 and 27 inclusive
        if(power_value >= 1 && power_value <= 27) {
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * Model,
                {
                    //Set the current power determined by user
                    furi_string_set(Model->Setting2Power, App->TempBuffer);

                    //Send the power value to the YRM100X
                    if(m100_set_transmitting_power(App->YRM100XWorker->module, power_value)) {
                        //Update the power value in the configuration screen
                        variable_item_set_current_value_text(
                            App->Setting2Item, furi_string_get_cstr(Model->Setting2Power));
                    }
                },
                Redraw);
            
        }
    }

    //Switch back to the configuration view
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewConfigure);
}

/**
 * @brief      Handles setting the default access password
 * @details    This function handles setting the default access password for reading, writing, and locking actions 
 * @param      context The UHFReaderApp app - used to allocate app variables and views.
*/
void uhf_reader_setting_6_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;
    
    // Temporary buffer to hold the converted string
    char* tempBuffer = (char*)malloc(24);
    snprintf(tempBuffer, 24, "%s", convert_to_hex_string(App->ApTempBuffer, 4));
    
    
    if(App->UHFModuleType != YRM100X_MODULE) {
        
        
        //TODO: ADD SUPPORT FOR M6E and M7E 
        
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * Model,
                {
                    //Send the set AP command to the RPi Zero
                    uart_helper_send(App->UartHelper, "SETPWD\n", 7);

                    //Set the current AP determined by user
                    furi_string_set(Model->SettingReadAp, tempBuffer);

                    //Send the AP value to the RPi Zero
                    uart_helper_send_string(App->UartHelper, Model->SettingReadAp);

                    //Update the AP value in the configuration screen
                    variable_item_set_current_value_text(
                        App->SettingApPwdItem, furi_string_get_cstr(Model->SettingReadAp));
                },
                Redraw);
            
    } else {
        
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * Model,
                {
                    //Set the current AP determined by user
                    furi_string_set(Model->SettingReadAp, tempBuffer);

                    //Send the AP value to the YRM100X
                    variable_item_set_current_value_text(
                        App->SettingApPwdItem, furi_string_get_cstr(Model->SettingReadAp));
                },
                Redraw);

            if(App->ReaderConnected){
                App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);
            }
            
    }
    free(tempBuffer);
    //Switch back to the configuration view
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewConfigure);
}


/**
 * @brief      Handles the Antenna Selection
 * @details    This function is a place holder for future functionality.
 * @param      item  VariableItem - the current selection for antenna values.
*/
void uhf_reader_setting_3_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);
    if(App->item_locks[4].locked) {
        show_locked_notification(App);
        return;
    }
    if(Index == 1) {
        uart_helper_send(App->UartHelper, "External\n", 9);
        //TODO: ADD SUPPORT FOR DIFFERENT ANTENNA TYPES AFTER HARDWARE DEVELOPED!
    } else {
        uart_helper_send(App->UartHelper, "Internal\n", 9);
    }

    //TODO: WAIT FOR ACK AND THEN SET TEXT VALUE
    variable_item_set_current_value_text(Item, App->Setting3Names[Index]);

    //Updating the antenna value for the read screen
    UHFReaderConfigModel* ModelRead = view_get_model(App->ViewRead);
    ModelRead->Setting3Index = Index;
    furi_string_set(ModelRead->Setting3Value, App->Setting3Names[Index]);

    //Updating the value of the antenna mode for the write screen
    UHFReaderWriteModel* ModelWrite = view_get_model(App->ViewWrite);
    ModelWrite->Setting3Index = Index;
    furi_string_set(ModelWrite->Setting3Value, App->Setting3Names[Index]);
}

/**
 * @brief      Handles the UHF Reader Module Selection
 * @details    This function handles switching between different supported UHF Readers like the YRM100X, M6e, and M7e Nano RFID modules.
 * @param      item  VariableItem - the current selection for the UHF Module.
*/
void uhf_reader_module_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    variable_item_set_current_value_text(Item, App->SettingModuleNames[Index]);

    if(Index == 1) {
        //Mark the YRM100X as being used
        App->UHFModuleType = YRM100X_MODULE;

        //Free regular uart worker
        uart_helper_free(App->UartHelper);

        //Create a UART worker for the YRM100X module
        App->YRM100XWorker = uhf_worker_alloc();
        UHFTagWrapper* WorkerTagWrapper = uhf_tag_wrapper_alloc();
        App->YRM100XWorker->uhf_tag_wrapper = WorkerTagWrapper;
        m100_disable_write_mask(App->YRM100XWorker->module, WRITE_EPC);

    } else if(Index == 2) {
        //Freeing the YRM100X uart helper
        if(App->UHFModuleType == YRM100X_MODULE) {
            //Free Tag Wrapper
            uhf_tag_wrapper_free(App->YRM100XWorker->uhf_tag_wrapper);

            //Freeing yrm100x worker
            uhf_worker_stop(App->YRM100XWorker);
            uhf_worker_free(App->YRM100XWorker);
        }
        //Mark the M7E as being used
        App->UartHelper = uart_helper_alloc();
        uart_helper_set_delimiter(App->UartHelper, LINE_DELIMITER, INCLUDE_LINE_DELIMITER);
        uart_helper_set_callback(App->UartHelper, uart_demo_process_line, App);
        App->UHFModuleType = M7E_HECTO_MODULE;
    } else {
        //Freeing the YRM100X uart helper
        if(App->UHFModuleType == YRM100X_MODULE) {
            //Free Tag Wrapper
            uhf_tag_wrapper_free(App->YRM100XWorker->uhf_tag_wrapper);

            //Freeing yrm100x worker
            uhf_worker_stop(App->YRM100XWorker);
            uhf_worker_free(App->YRM100XWorker);
        }

        //Mark the M6E as being used
        App->UartHelper = uart_helper_alloc();
        uart_helper_set_delimiter(App->UartHelper, LINE_DELIMITER, INCLUDE_LINE_DELIMITER);
        uart_helper_set_callback(App->UartHelper, uart_demo_process_line, App);
        App->UHFModuleType = M6E_NANO_MODULE;
    }
}
/**
 * @brief      Handles the App save settings
 * @details    This function toggles the saving mode after a write operation.
 * @param      item  VariableItem - the current selection for setting.
*/
void uhf_reader_save_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    variable_item_set_current_value_text(Item, App->SettingSavingNames[Index]);

    if(Index == 1) {
        App->UHFSaveType = YES_SAVE_ON_WRITE;
    } 
    else {
        App->UHFSaveType = NO_SAVE_ON_WRITE;
    }
}

/**
 * @brief      Handles the UHF Reader Baud Rate Selection
 * @details    This function handles switching between different supported baud rates for the UHF Readers like the YRM100X, M6e, and M7e Nano RFID modules.
 * @param      item  VariableItem - the current selection for the UHF baud rate.
*/
void uhf_reader_baud_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);
    // Check if locked
    if(App->item_locks[1].locked) {
        show_locked_notification(App);
        return;
    }
    variable_item_set_current_value_text(Item, App->SettingBaudNames[Index]);

    if(Index == 1) {
        //Use 115200 as baud rate
        App->UHFBaudRate = 115200;
    } else if(Index == 2) {
        //Use 19200 as baud rate
        App->UHFBaudRate = 384000;
    } else {
        //Use 9600 as baud rate
        App->UHFBaudRate = 9600;
    }

    //Setting the baudrate for each module
    if(App->UHFModuleType == YRM100X_MODULE && App->ReaderConnected) {
        m100_set_baudrate(App->YRM100XWorker->module, App->UHFBaudRate);
    } else {
        uart_helper_set_baud_rate(App->UartHelper, App->UHFBaudRate);
    }
}

/**
 * @brief      Handles the UHF Reader Region Selection
 * @details    This function handles switching between different supported regions for the selected UHF RFID Reader.
 * @param      item  VariableItem - the current selection for the Region.
*/
void uhf_reader_region_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);
    if(App->item_locks[2].locked) {
        show_locked_notification(App);
        return;
    }
    if(App->ReaderConnected) {
        if(Index == 1) {
            //Mark EU as being used
            App->UHFRegionType = EU_REGION;
        } else if(Index == 2) {
            //Mark Korea as being used
            App->UHFRegionType = KOREA_REGION;
        } else if(Index == 3) {
            //Mark Korea as being used
            App->UHFRegionType = CHINA_800_REGION;
        } else if(Index == 4) {
            //Mark Korea as being used
            App->UHFRegionType = CHINA_900_REGION;
        } else {
            //Mark the M6E as being used
            App->UHFRegionType = USA_REGION;
        }

        if(App->UHFModuleType == YRM100X_MODULE) {
            WorkingRegion region = WORKING_REGIONS[Index];
            if(m100_set_working_region(App->YRM100XWorker->module, region)) {
                variable_item_set_current_value_text(Item, App->SettingRegionNames[Index]);
            }

        } else {
            //PLACE COMMAND HERE TO CHANGE THE REGION FOR M6E and M7E
            uart_helper_send(App->UartHelper, "Region\n", 7);
            variable_item_set_current_value_text(Item, App->SettingRegionNames[Index]);
        }
    }
}
/**
 * @brief      Allocates the power text screen
 * @details    Allocates the text input object for the power screen.
 * @param      app  The UHFReaderApp - used for allocating variables and text input.
*/
void power_menu_alloc(UHFReaderApp* App) {
    //TODO: Add checks for reader connection and change power levels based on module used.
    App->TextInput = text_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSetPower, text_input_get_view(App->TextInput));
    App->TempBufferSize = 5;
    App->TempBuffer = (char*)malloc(App->TempBufferSize);

}

/**
 * @brief      Allocates the AP text screen
 * @details    Allocates the text input object for the AP screen.
 * @param      app  The UHFReaderApp - used for allocating variables and text input.
*/
void ap_menu_alloc(UHFReaderApp* App) {
    App->ApInput = byte_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSetReadAp, byte_input_get_view(App->ApInput));
    App->ApInputBufferSize = 4;
    App->ApTempBuffer = (uint8_t*)malloc(App->ApInputBufferSize);
}


/**
 * @brief      Handles the setting items clicked
 * @details    Handles the power value input by the user.
 * @param      context, index - context used for UHFReaderApp, index used for state check.
*/
void uhf_reader_setting_item_clicked(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context; 
    index++;
    //Check if the power menu is being selected
    if(index == 3) {
        if(App->item_locks[0].locked) {
            show_locked_notification(App);
            return;
        }
        // Header to display on the power value input screen.
        text_input_set_header_text(App->TextInput, App->Setting2EntryText);

        //Modify the value of the power for the read and write models
        bool Redraw = false;
        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * Model,
            {
                strncpy(
                    App->TempBuffer,
                    furi_string_get_cstr(Model->Setting2Power),
                    App->TempBufferSize);
            },
            Redraw);
        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                strncpy(
                    App->TempBuffer,
                    furi_string_get_cstr(Model->Setting2Power),
                    App->TempBufferSize);
            },
            Redraw);

        //Setting the power text input callback function
        bool ClearPreviousText = false;
        text_input_set_result_callback(
            App->TextInput,
            uhf_reader_setting_2_text_updated,
            App,
            App->TempBuffer,
            App->TempBufferSize,
            ClearPreviousText);
        view_set_previous_callback(
            text_input_get_view(App->TextInput), uhf_reader_navigation_configure_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetPower);
    }
    else if(index == 6) {
        // Header to display on the AP value input screen.
        if(App->item_locks[3].locked) {
            show_locked_notification(App);
            return;
        }
        byte_input_set_header_text(App->ApInput, App->AccessPasswordPlaceHolder);

        //Modify the value of the AP for the read and write models
        bool Redraw = false;
        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * Model,
            {
                strncpy(
                    convert_to_hex_string(App->ApTempBuffer, 4),
                    furi_string_get_cstr(Model->SettingReadAp),
                    App->ApInputBufferSize);
            },
            Redraw);
        

        //Setting the AP text input callback function
        byte_input_set_result_callback(
            App->ApInput,
            uhf_reader_setting_6_text_updated,
            NULL,
            App,
            App->ApTempBuffer,
            App->ApInputBufferSize);
        view_set_previous_callback(
            byte_input_get_view(App->ApInput), uhf_reader_navigation_configure_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetReadAp);
    }
    
}

/**
 * @brief      Frees the configure screen.
 * @details    Frees all variables and views for the configure screen.
 * @param      app  The UHFReaderApp - used for freeing variables and text input.
*/
void view_config_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSetPower);
    text_input_free(App->TextInput);
    free(App->TempBuffer);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSetReadAp);
    byte_input_free(App->ApInput);
    free(App->ApTempBuffer);
    free(App->item_locks);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewConfigure);
    variable_item_list_free(App->VariableItemListConfig);
}