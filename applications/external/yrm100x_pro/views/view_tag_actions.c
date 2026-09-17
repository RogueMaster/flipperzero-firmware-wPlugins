#include "view_tag_actions.h"

/**
 * @brief      Callback when the user exits the epc info screen.
 * @details    This function is called when the user exits the epc info screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_epc_info_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

/**
 * @brief      Callback for the rename text input screen.
 * @details    This function handles the renaming logic for saved UHF tags.
 * @param      context  The UHFReaderApp - Used to change app variables.
*/
void uhf_reader_rename_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    //Allocating FuriStrings to store each of the values associated with each UHF Tag
    FuriString* TempTid = furi_string_alloc();
    FuriString* TempRes = furi_string_alloc();
    FuriString* TempMem = furi_string_alloc();
    FuriString* TempCrc = furi_string_alloc();
    FuriString* TempPc = furi_string_alloc();
    FuriString* TempStr = furi_string_alloc();
    FuriString* TempTag = furi_string_alloc();
    FuriString* TempEpcStr = furi_string_alloc();

    //Open the saved EPCs file
    if(!flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FURI_LOG_E(TAG, "Failed to open Saved file");
        flipper_format_file_close(App->EpcFile);

    } else {
        furi_string_printf(TempStr, "Tag%ld", App->SelectedTagIndex);

        //Get the current tag data for the tag being renamed
        if(!flipper_format_read_string(App->EpcFile, furi_string_get_cstr(TempStr), TempTag)) {
            FURI_LOG_D(TAG, "Could not read tag %ld data", App->SelectedTagIndex);
            // Must close here — the file was successfully opened above.
            flipper_format_file_close(App->EpcFile);
        } else {
            //Get all the fields for the current tag being renamed
            const char* InputString = furi_string_get_cstr(TempTag);
            char* ExtractedEpc = extract_epc(InputString);
            char* ExtractedTid = extract_tid(InputString);
            char* ExtractedRes = extract_res(InputString);
            char* ExtractedMem = extract_mem(InputString);
            char* ExtractedCrc = extract_crc(InputString);
            char* ExtractedPc = extract_pc(InputString);
            if(ExtractedEpc) {
                furi_string_set_str(TempEpcStr, ExtractedEpc);
                free(ExtractedEpc);
            }
            if(ExtractedTid) {
                furi_string_set_str(TempTid, ExtractedTid);
                free(ExtractedTid);
            }
            if(ExtractedRes) {
                furi_string_set_str(TempRes, ExtractedRes);
                free(ExtractedRes);
            }
            if(ExtractedMem) {
                furi_string_set_str(TempMem, ExtractedMem);
                free(ExtractedMem);
            }
            if(ExtractedCrc) {
                furi_string_set_str(TempCrc, ExtractedCrc);
                free(ExtractedCrc);
            }
            if(ExtractedPc) {
                furi_string_set_str(TempPc, ExtractedPc);
                free(ExtractedPc);
            }
            flipper_format_file_close(App->EpcFile);
        }
    }

    //Open the file again
    if(!flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FURI_LOG_E(TAG, "Failed to open file for rename update");
        // Cannot update the file — free strings and bail out.
        furi_string_free(TempEpcStr);
        furi_string_free(TempTid);
        furi_string_free(TempRes);
        furi_string_free(TempMem);
        furi_string_free(TempCrc);
        furi_string_free(TempPc);
        furi_string_free(TempStr);
        furi_string_free(TempTag);
        return;
    }

    //Allocate new FuriStrings for storing the newly named UHF tag
    FuriString* NumEpcs = furi_string_alloc();
    FuriString* EpcAndName = furi_string_alloc();
    furi_string_printf(NumEpcs, "Tag%ld", App->SelectedTagIndex);
    furi_string_printf(
        EpcAndName,
        "%s:%s:%s:%s:%s:%s:%s",
        App->TempSaveBuffer,
        furi_string_get_cstr(TempEpcStr),
        furi_string_get_cstr(TempTid),
        furi_string_get_cstr(TempRes),
        furi_string_get_cstr(TempMem),
        furi_string_get_cstr(TempPc),
        furi_string_get_cstr(TempCrc));

    //Write to the file and update the current string in its index
    if(!flipper_format_update_string_cstr(
           App->EpcFile, furi_string_get_cstr(NumEpcs), furi_string_get_cstr(EpcAndName))) {
        FURI_LOG_E(TAG, "Failed to write to file");
    }

    //Close the file
    flipper_format_file_close(App->EpcFile);

    //Free all FuriStrings used
    furi_string_free(EpcAndName);
    furi_string_free(NumEpcs);
    furi_string_free(TempEpcStr);
    furi_string_free(TempTid);
    furi_string_free(TempRes);
    furi_string_free(TempMem);
    furi_string_free(TempCrc);
    furi_string_free(TempPc);
    furi_string_free(TempStr);
    furi_string_free(TempTag);

    // Keep the existing registered Saved view and rebuild only its current
    // fixed-size page. This avoids both GUI corruption and per-dump heap growth.
    uhf_reader_saved_menu_rebuild(App);

    //The Saved view is still registered (we only reset its items), so just
    //navigate back to it.
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaved);
}

static bool selected_saved_source_pc_is_3400(UHFReaderApp* App) {
    if(!App || App->ActionContext != ActionFromSaved || App->SelectedTagIndex == 0) {
        return false;
    }

    bool match = false;
    FuriString* key = furi_string_alloc();
    FuriString* record = furi_string_alloc();

    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        furi_string_printf(key, "Tag%ld", App->SelectedTagIndex);

        if(flipper_format_read_string(App->EpcFile, furi_string_get_cstr(key), record)) {
            char* pc_str = extract_pc(furi_string_get_cstr(record));
            if(pc_str) {
                unsigned long pc = strtoul(pc_str, NULL, 16);
                match = (pc == UHF_CLONE_PC3400_SOURCE_PC);

                UHF_I(
                    "CLONE3400",
                    "ACTION source Tag%lu PC=%04lX eligible=%d",
                    (unsigned long)App->SelectedTagIndex,
                    pc,
                    match ? 1 : 0);
                uhf_debug_flush();

                free(pc_str);
            }
        }

        flipper_format_file_close(App->EpcFile);
    } else {
        flipper_format_file_close(App->EpcFile);
    }

    furi_string_free(key);
    furi_string_free(record);
    return match;
}

/**
 * @brief      Tag Info Submenu Callback
 * @details    Handles the different submenu options for the tag actions menu.
 * @param      context    The UHFReaderApp - the app for working with variables
 * @param      index  The selected submenu index
*/
void uhf_reader_submenu_tag_info_callback(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(index) {
    case UHFReaderSubmenuIndexTagWrite:
    case UHFReaderSubmenuIndexTagLock:
    case UHFReaderSubmenuIndexTagKill:
    case UHFReaderSubmenuIndexTagClone:
    case UHFReaderSubmenuIndexTagClonePc3400:
    case UHFReaderSubmenuIndexUpdateAP:
    case UHFReaderSubmenuIndexUpdateKP:
        if(!uhf_reader_require_antenna(App, UHFReaderViewTagAction)) return;
        break;
    default:
        break;
    }

    switch(index) {
    //Handles saving a live, in-memory scanned tag. Reuses the same keyboard/save
    //flow the old EPC-dump left-arrow used, returning to the EPC dump afterward.
    case UHFReaderSubmenuIndexTagSave:
        uhf_reader_ensure_epc_views(App);
        uhf_reader_begin_save_tag(App, App->ViewEpc, UHFReaderViewEpcDump);
        break;

    //Handles the Tag Info menu
    case UHFReaderSubmenuIndexTagInfo:
        uhf_reader_ensure_epc_info_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcInfo);
        break;

    //Handles the rename screen
    case UHFReaderSubmenuIndexTagRename:
        text_input_set_header_text(App->RenameInput, "Rename EPC");

        //Pre-populate the keyboard with the tag's current name so the user can
        //edit it in place. Read the saved record for the selected tag and pull
        //out the name field into the text-input buffer.
        App->TempSaveBuffer[0] = '\0';
        if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
            FuriString* TempKey = furi_string_alloc();
            FuriString* TempTag = furi_string_alloc();
            furi_string_printf(TempKey, "Tag%ld", App->SelectedTagIndex);
            if(flipper_format_read_string(App->EpcFile, furi_string_get_cstr(TempKey), TempTag)) {
                char* ExtractedName = extract_name(furi_string_get_cstr(TempTag));
                if(ExtractedName != NULL) {
                    strncpy(App->TempSaveBuffer, ExtractedName, App->TempBufferSaveSize);
                    App->TempSaveBuffer[App->TempBufferSaveSize - 1] = '\0';
                    free(ExtractedName);
                }
            }
            furi_string_free(TempKey);
            furi_string_free(TempTag);
            flipper_format_file_close(App->EpcFile);
        }

        // Configure the text input. Keep the pre-filled current name (don't clear)
        // so it appears as an editable default when the keyboard opens.
        bool ClearPreviousText = false;
        text_input_set_result_callback(
            App->RenameInput,
            uhf_reader_rename_text_updated,
            App,
            App->TempSaveBuffer,
            App->TempBufferSaveSize,
            ClearPreviousText);

        // Pressing the BACK button will reload the configure screen.
        view_set_previous_callback(
            text_input_get_view(App->RenameInput), uhf_reader_navigation_epc_info_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRenameInput);
        break;

    //Handles the write screen
    case UHFReaderSubmenuIndexTagWrite:
        uhf_reader_ensure_write_view(App);

        //Fresh entry from the action menu: let the enter callback reset the bank
        //selection. (Returning from the value keyboard must NOT reset it.)
        App->WriteMenuFreshEntry = true;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewWrite);
        break;
    case UHFReaderSubmenuIndexTagLock:
        if(App->ActionContext != ActionFromLive) {
            uhf_notify_error(App);
            break;
        }
        uhf_reader_ensure_lock_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLock);
        break;
    case UHFReaderSubmenuIndexTagKill:
        if(App->ActionContext != ActionFromLive) {
            uhf_notify_error(App);
            break;
        }

        uhf_reader_ensure_write_view(App);
        uhf_reader_ensure_lock_view(App);
        uhf_reader_ensure_kill_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewKill);
        break;
    //Handles the delete screen
    case UHFReaderSubmenuIndexTagDelete:
        uhf_reader_ensure_delete_views(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewDelete);
        break;
    case UHFReaderSubmenuIndexTagClone:
        // Normal clone preserves the original alpha26 behavior.
        App->ClonePc3400Mode = false;
        uhf_reader_ensure_clone_views(App);

        //Initialise the clone bank-selection view (deep-copy source tag +
        //reset item states) with the real App context, then switch. The
        //VariableItemList view's own context is the list module, so we cannot
        //rely on a view enter callback receiving App here.
        view_clone_banks_enter_callback(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCloneBanks);
        break;

    case UHFReaderSubmenuIndexTagClonePc3400:
        if(App->ActionContext != ActionFromSaved || !selected_saved_source_pc_is_3400(App)) {
            App->ClonePc3400Mode = false;
            uhf_notify_error(App);
            break;
        }

        App->ClonePc3400Mode = true;
        uhf_reader_ensure_clone_views(App);
        UHF_I(
            "CLONE3400",
            "ACTION ENTER Tag%lu marker=auto-by-TID registry_entries=%u",
            (unsigned long)App->SelectedTagIndex,
            (unsigned int)App->UmiMarkerRegistry.count);
        uhf_debug_flush();

        view_clone_banks_enter_callback(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCloneBanks);
        break;
    case UHFReaderSubmenuIndexUpdateAP:
        if(App->ActionContext != ActionFromLive) {
            uhf_notify_error(App);
            break;
        }
        uhf_reader_ensure_lock_view(App);
        byte_input_set_header_text(App->CurrentApInput, "Current Access Pwd");
        byte_input_set_result_callback(
            App->CurrentApInput, uhf_reader_current_ap_updated, NULL, App, App->CurrentApBuffer, 4);
        view_set_previous_callback(
            byte_input_get_view(App->CurrentApInput), uhf_reader_navigation_lock_exit_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCurrentApInput);
        break;
    case UHFReaderSubmenuIndexUpdateKP:
        if(App->ActionContext != ActionFromLive) {
            uhf_notify_error(App);
            break;
        }
        uhf_reader_ensure_lock_view(App);
        byte_input_set_header_text(App->CurrentKpInput, "Current Kill Pwd");
        byte_input_set_result_callback(
            App->CurrentKpInput, uhf_reader_current_kp_updated, NULL, App, App->CurrentKpBuffer, 4);
        view_set_previous_callback(
            byte_input_get_view(App->CurrentKpInput), uhf_reader_navigation_lock_exit_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCurrentKpInput);
        break;
    default:
        break;
    }
}

/**
 * @brief      Callback when the user exits the tag action screen.
 * @details    This function is called when the user exits the tag action screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_tag_action_exit_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSaved;
}

/**
 * @brief      Rebuilds the shared Tag Action submenu based on App->ActionContext.
 * @details    Live (unsaved) tags get Update/Lock/Kill; saved tags get the full
 *             menu (Tag Data/Rename/Write/Lock/Kill/Delete). Call this before
 *             switching to UHFReaderViewTagAction from either entry point.
 * @param      App  The UHFReaderApp object.
*/
void uhf_reader_build_tag_action_menu(UHFReaderApp* App) {
    submenu_reset(App->SubmenuTagActions);
    submenu_set_header(App->SubmenuTagActions, "EPC Actions");

    if(App->ActionContext == ActionFromLive) {
        //Unsaved, in-memory scanned tag: targeted actions only. Save is offered
        //here (instead of the old EPC-dump left-arrow) and only for live tags.
        submenu_add_item(
            App->SubmenuTagActions,
            "Save",
            UHFReaderSubmenuIndexTagSave,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Update",
            UHFReaderSubmenuIndexTagWrite,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Lock",
            UHFReaderSubmenuIndexTagLock,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Kill",
            UHFReaderSubmenuIndexTagKill,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Clone",
            UHFReaderSubmenuIndexTagClone,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Update AP",
            UHFReaderSubmenuIndexUpdateAP,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Update KP",
            UHFReaderSubmenuIndexUpdateKP,
            uhf_reader_submenu_tag_info_callback,
            App);
    } else {
        //Saved tag: full menu.
        submenu_add_item(
            App->SubmenuTagActions,
            "Tag Data",
            UHFReaderSubmenuIndexTagInfo,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Rename",
            UHFReaderSubmenuIndexTagRename,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Write",
            UHFReaderSubmenuIndexTagWrite,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Delete",
            UHFReaderSubmenuIndexTagDelete,
            uhf_reader_submenu_tag_info_callback,
            App);
        submenu_add_item(
            App->SubmenuTagActions,
            "Clone",
            UHFReaderSubmenuIndexTagClone,
            uhf_reader_submenu_tag_info_callback,
            App);

        // Dedicated mode is offered for Saved PC3400 sources. The target TID
        // must match a marker learned by Test UMI Auto (or the proven built-in).
        if(selected_saved_source_pc_is_3400(App)) {
            submenu_add_item(
                App->SubmenuTagActions,
                "Clone_PC3400",
                UHFReaderSubmenuIndexTagClonePc3400,
                uhf_reader_submenu_tag_info_callback,
                App);
        }
    }
}

/**
 * @brief      Allocates the tag actions view.
 * @details    This function allocates all variables for the tag actions view.
 * @param      app  The UHFReaderApp object.
*/
void view_tag_actions_alloc(UHFReaderApp* App) {
    //Allocate the tag actions submenu
    App->SubmenuTagActions = submenu_alloc();
    submenu_set_header(App->SubmenuTagActions, "EPC Actions");
    //Populate with an initial set; rebuilt on every entry via ActionContext.
    uhf_reader_build_tag_action_menu(App);
    view_set_previous_callback(
        submenu_get_view(App->SubmenuTagActions), uhf_reader_navigation_tag_action_exit_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewTagAction, submenu_get_view(App->SubmenuTagActions));

    //Allocate the rename text input
    App->RenameInput = text_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewRenameInput, text_input_get_view(App->RenameInput));
}

/**
 * @brief      Frees the tag action view.
 * @details    This function frees all variables for the tag actions view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_tag_actions_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewRenameInput);
    text_input_free(App->RenameInput);
    App->RenameInput = NULL;
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewTagAction);
    submenu_free(App->SubmenuTagActions);
    App->SubmenuTagActions = NULL;
}
