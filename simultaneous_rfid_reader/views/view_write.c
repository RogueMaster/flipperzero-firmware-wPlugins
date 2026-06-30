#include "view_write.h"

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
            
            furi_string_set_str(Model->NewEpcValue, App->TempSaveBuffer);},
            
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

    //Start the timer
    furi_assert(App->Timer == NULL);
    App->Timer =
        furi_timer_alloc(uhf_reader_view_write_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(App->Timer, Period);

    //Setting default reading states and freeing FuriStrings used
    App->IsWriting = false;

    furi_string_free(TempTag);
    furi_string_free(TempStr);
}

/**
 * @brief      Callback when the user exits the write screen.
 * @details    This function is called when the user exits the write screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_write_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    furi_timer_stop(App->Timer);
    furi_timer_free(App->Timer);
    App->Timer = NULL;
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
    if(event == UHFWorkerEventSuccess) {
        dolphin_deed(DolphinDeedNfcReadSuccess);
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        notification_message(App->Notifications, &sequence_success);
        
        //Reset booleans tracking if the kill or access password were set for writing.
        App->YRM100XWorker->KillPwd = false;
        App->YRM100XWorker->AccessPwd = false; 

        //If the save on write option is toggled, then update the fields for the saved tag
        if(App->UHFSaveType == YES_SAVE_ON_WRITE){
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
                    if(furi_string_equal(Model->WriteFunction,WRITE_EPC_VAL)){
                        furi_string_set(TempTid, Model->TidValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->MemValue);
                        furi_string_set(TempEpc, Model->NewEpcValue);
                        
                    }
                    else if(furi_string_equal(Model->WriteFunction,WRITE_USR_MEM)){
                        furi_string_set(TempTid, Model->TidValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->NewEpcValue);
                        furi_string_set(TempEpc, Model->EpcValue);
                        
                    }
                    else if(furi_string_equal(Model->WriteFunction,WRITE_TID_MEM)){
                        furi_string_set(TempTid, Model->NewEpcValue);
                        furi_string_set(TempRes, Model->ResValue);
                        furi_string_set(TempMem, Model->MemValue);
                        furi_string_set(TempEpc, Model->EpcValue);
                        
                    }
                    else if(furi_string_equal(Model->WriteFunction,WRITE_RES_MEM)){
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
    } else if(event == UHFWorkerEventAborted) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        notification_message(App->Notifications, &sequence_error);
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
                furi_string_set(Model->WriteFunction, WRITE_EPC_OK);
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
    //The ok button was pressed to trigger a write
    case UHFReaderEventIdOkPressed: {
        
        
        bool redraw = true;
        dolphin_deed(DolphinDeedNfcRead);


        //If the user presses the ok button while the app is writing (to cancel the operation if the tag is inactive) then the worker is stopped
        if(App->IsWriting){
            uhf_worker_stop(App->YRM100XWorker);
            App->IsWriting = false;
             
            return true;
        }

        with_view_model(
            App->ViewWrite,
            UHFReaderWriteModel * Model,
            {
                Model->IsWriting = true;
                //TODO: Modify this to work for the M6E and M7E
                if(App->UHFModuleType != YRM100X_MODULE) {
                    if(furi_string_equal(Model->WriteFunction, WRITE_EPC_VAL)) {
                        uart_helper_send(App->UartHelper, "WRITE\n", 6);
                        uart_helper_send_string(App->UartHelper, Model->EpcValue);
                        uart_helper_send_string(App->UartHelper, Model->NewEpcValue);
                    } else if(furi_string_equal(Model->WriteFunction, WRITE_RES_MEM)) {
                        uart_helper_send(App->UartHelper, "WRITERES\n", 9);
                        uart_helper_send_string(App->UartHelper, Model->EpcValue);
                        uart_helper_send_string(App->UartHelper, Model->NewEpcValue);
                    } else if(furi_string_equal(Model->WriteFunction, WRITE_USR_MEM)) {
                        uart_helper_send(App->UartHelper, "WRITEUSR\n", 9);
                        uart_helper_send_string(App->UartHelper, Model->EpcValue);
                        uart_helper_send_string(App->UartHelper, Model->NewEpcValue);
                    } else if(furi_string_equal(Model->WriteFunction, WRITE_TID_MEM)) {
                        uart_helper_send(App->UartHelper, "WRITETID\n", 9);
                        uart_helper_send_string(App->UartHelper, Model->EpcValue);
                        uart_helper_send_string(App->UartHelper, Model->NewEpcValue);
                    }
                } else {
                    // Resetting the dynamically allocated arrays to zero
                    uhf_reader_fetch_selected_tag(App);
                    memset(App->EpcBytes, 0, 12 * sizeof(uint8_t));
                    memset(App->ResBytes, 0, 8 * sizeof(uint8_t));
                    memset(App->TidBytes, 0, 16 * sizeof(uint8_t));
                    memset(App->UserBytes, 0, 16 * sizeof(uint8_t));
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

                    hex_string_to_uint16(furi_string_get_cstr(Model->Pc), App->PcBytes, &App->PcBytesLen);
                    hex_string_to_uint16(furi_string_get_cstr(Model->Crc), App->CrcBytes, &App->CrcBytesLen);
                    
                    uint16_t combinedPc = 0;
                    uint16_t combinedCrc = 0;

                    for(size_t i = 0; i < 4; i++) {
                        combinedPc |= App->PcBytes[i];
                    }

                    for(size_t i = 0; i < 4; i++) {
                        combinedCrc |= App->CrcBytes[i];
                    }

                    if(furi_string_equal(Model->WriteFunction, WRITE_EPC_VAL) &&
                       Model->NewEpcValue != NULL) {
                        hex_string_to_bytes(
                            App->TempSaveBuffer, App->EpcBytes, &App->EpcBytesLen);
                        
                        uhf_tag_set_epc(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->EpcBytes, App->EpcBytesLen * sizeof(uint8_t));

                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_EPC);
                    } else {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->EpcValue), App->EpcBytes, &App->EpcBytesLen);
                        uhf_tag_set_epc(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->EpcBytes, App->EpcBytesLen * sizeof(uint8_t));
                    }

                    if(furi_string_equal(Model->WriteFunction, WRITE_USR_MEM) &&
                       Model->NewEpcValue != NULL) {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->NewEpcValue), App->UserBytes, &App->UserBytesLen);
                        uhf_tag_set_user(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->UserBytes, App->UserBytesLen * sizeof(uint8_t));
                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_USER);
                        
                    } else {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->MemValue), App->UserBytes, &App->UserBytesLen);
                        uhf_tag_set_user(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->UserBytes, App->UserBytesLen * sizeof(uint8_t));
                    }
                    if(furi_string_equal(Model->WriteFunction, WRITE_TID_MEM) &&
                       Model->NewEpcValue != NULL) {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->NewEpcValue), App->TidBytes, &App->TidBytesLen);
                        uhf_tag_set_tid(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->TidBytes, App->TidBytesLen * sizeof(uint8_t));
                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_TID);
                    } else {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->TidValue), App->TidBytes, &App->TidBytesLen);
                        uhf_tag_set_tid(
                            App->YRM100XWorker->NewTag, (uint8_t*)App->TidBytes,App->TidBytesLen * sizeof(uint8_t));
                    }
                    if(furi_string_equal(Model->WriteFunction, WRITE_RES_MEM) &&
                       Model->NewEpcValue != NULL) {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->NewEpcValue), App->ResBytes, &App->ResBytesLen);
                        uhf_tag_set_kill_pwd(App->YRM100XWorker->NewTag, App->ResBytes, App->ResBytesLen);
                        uhf_tag_set_access_pwd(App->YRM100XWorker->NewTag, App->ResBytes, App->ResBytesLen);
                        
                        m100_enable_write_mask(App->YRM100XWorker->module, WRITE_RFU);
                    } else {
                        hex_string_to_bytes(
                            furi_string_get_cstr(Model->ResValue), App->ResBytes, &App->ResBytesLen);

                        
                    }
                    

                    uhf_tag_set_epc_pc(App->YRM100XWorker->NewTag, combinedPc);
                    uhf_tag_set_epc_crc(App->YRM100XWorker->NewTag, combinedCrc);
                    uhf_tag_set_epc_size(App->YRM100XWorker->NewTag, App->EpcBytesLen * sizeof(uint8_t));
                    uhf_tag_set_user_size(App->YRM100XWorker->NewTag, App->UserBytesLen * sizeof(uint8_t));
                    uhf_tag_set_tid_size(App->YRM100XWorker->NewTag, App->TidBytesLen * sizeof(uint8_t));

                    App->YRM100XWorker->KillPwd = true;
                    App->YRM100XWorker->AccessPwd = true; 
                    
                    App->IsWriting = true;


                    
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
    FuriString* xstr = furi_string_alloc();

    //Clearing the canvas, setting the color, font and content displayed.
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, "           Write Menu:");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 33, "Write Mode:");

    //Displaying the current write mode selected
    canvas_draw_str(canvas, 51, 33, furi_string_get_cstr(MyModel->WriteFunction));

    //Display the CRC
    canvas_draw_str(canvas, 4, 22, "CRC:");

    canvas_draw_str(canvas, 28, 22, furi_string_get_cstr(MyModel->Crc));

    //Display the PC
    canvas_draw_str(canvas, 70, 22, "PC:");
    canvas_draw_str(canvas, 90, 22, furi_string_get_cstr(MyModel->Pc));

    //Display the current write status
    canvas_draw_str(canvas, 0, 44, "Write Status: ");
    canvas_draw_str(canvas, 65, 44, furi_string_get_cstr(MyModel->WriteStatus));

    //Display the write button
    if(!MyModel->IsWriting) {
        elements_button_center(canvas, "Write");

    } else {
        elements_button_center(canvas, "Cancel");
    }
    furi_string_free(xstr);
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
        //If the left button is pressed, then pull up the EPC value and keyboard
        if(event->key == InputKeyLeft && !App->IsWriting) {
            text_input_set_header_text(App->EpcWrite, "EPC Value");

            bool redraw = false;
            with_view_model(
                App->ViewWrite,

                //Store the new epc value and mark the write function as the epc selection
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->EpcValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_EPC_VAL);
                },
                redraw);

            // Configure the text input
            

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

        //If the right button is pressed, then display the reserved memory bank and display the keyboard
        else if(event->key == InputKeyRight && !App->IsWriting) {
            text_input_set_header_text(App->EpcWrite, "Reserved Memory Bank");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,

                //Store the modified value for the reserved memory bank
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->ResValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_RES_MEM);
                },
                redraw);

            // Configure the text input
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

        //If the up button is pressed, then display the user memory bank and keyboard
        else if(event->key == InputKeyUp && !App->IsWriting) {
            text_input_set_header_text(App->EpcWrite, "User Memory Bank");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,

                //Store the modified user memory value
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->MemValue),
                        App->TempBufferSaveSize);
                    furi_string_set(Model->WriteFunction, WRITE_USR_MEM);
                },
                redraw);

            // Configure the text input
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

        //If the down button is pressed, then display the TID memory bank and the keyboard
        else if(event->key == InputKeyDown && !App->IsWriting) {
            text_input_set_header_text(App->EpcWrite, "TID Memory Bank");
            bool redraw = false;
            with_view_model(
                App->ViewWrite,

                //Store the modified TID value
                UHFReaderWriteModel * Model,
                {
                    strncpy(
                        App->TempSaveBuffer,
                        furi_string_get_cstr(Model->TidValue),
                        App->TempBufferSaveSize);
                    furi_string_set_str(Model->WriteFunction, WRITE_TID_MEM);
                },
                redraw);

            // Configure the text input
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
    App->EpcBytes = (uint8_t*)malloc(12 * sizeof(uint8_t));
    App->ResBytes = (uint8_t*)malloc(8 * sizeof(uint8_t)); 
    App->TidBytes = (uint8_t*)malloc(16 * sizeof(uint8_t));
    App->UserBytes = (uint8_t*)malloc(16 * sizeof(uint8_t));
    App->PcBytes = (uint16_t*)malloc(2 * sizeof(uint16_t));
    App->CrcBytes = (uint16_t*)malloc(2 * sizeof(uint16_t));

    //Setting default values for the view model
    ModelWrite->Setting1Index = App->Setting1Index;
    ModelWrite->Setting2Power = App->Setting2PowerStr;
    ModelWrite->Setting3Index = App->Setting3Index;
    ModelWrite->SettingKillPwd = furi_string_alloc_set(App->DefaultKillPassword);
    ModelWrite->EpcName = EpcNameWriteDefault;
    ModelWrite->Setting1Value = furi_string_alloc_set(App->Setting1Names[App->Setting1Index]);
    ModelWrite->Setting3Value = furi_string_alloc_set(App->Setting3Names[App->Setting3Index]);
    ModelWrite->Pc = furi_string_alloc_set("XXXX");
    ModelWrite->Crc = furi_string_alloc_set("XXXX");
    FuriString* EpcWriteDefault = furi_string_alloc();
    furi_string_set_str(EpcWriteDefault, "Press Write");
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
    free(App->EpcBytes);
    free(App->ResBytes);
    free(App->TidBytes);
    free(App->UserBytes);
    free(App->PcBytes);
    free(App->CrcBytes);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewEpcWriteInput);
    text_input_free(App->EpcWrite);
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewWrite);
    view_free(App->ViewWrite);
}