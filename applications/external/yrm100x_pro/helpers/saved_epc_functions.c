#include "saved_epc_functions.h"

static bool uhf_saved_write_index_count(UHFReaderApp* App, uint32_t count) {
    if(!App || !App->EpcIndexFile) return false;

    FuriString* count_str = furi_string_alloc();
    furi_string_printf(count_str, "%lu", (unsigned long)count);

    bool ok = false;

    if(flipper_format_file_open_existing(App->EpcIndexFile, APP_DATA_PATH("Index_File.txt"))) {
        /*
         * IMPORTANT:
         * write_string_cstr() appends a new key/value. It does NOT replace
         * the first existing "Number of Tags". After app restart the reader
         * could therefore see the stale initial value 0.
         *
         * update_string_cstr() replaces the existing key in place.
         */
        ok = flipper_format_update_string_cstr(
            App->EpcIndexFile, "Number of Tags", furi_string_get_cstr(count_str));

        if(!ok) {
            // Old/corrupt index without the key: append it once.
            ok = flipper_format_write_string_cstr(
                App->EpcIndexFile, "Number of Tags", furi_string_get_cstr(count_str));
        }

        flipper_format_file_close(App->EpcIndexFile);
    } else {
        flipper_format_file_close(App->EpcIndexFile);

        if(flipper_format_file_open_new(App->EpcIndexFile, APP_DATA_PATH("Index_File.txt"))) {
            ok = flipper_format_write_string_cstr(
                App->EpcIndexFile, "Number of Tags", furi_string_get_cstr(count_str));
            flipper_format_file_close(App->EpcIndexFile);
        }
    }

    UHF_I("SAVE", "INDEX REPAIR count=%lu ok=%d", (unsigned long)count, ok ? 1 : 0);
    uhf_debug_flush();

    furi_string_free(count_str);
    return ok;
}

void uhf_saved_update_count_labels(UHFReaderApp* App) {
    if(!App) return;

    char label[32];
    snprintf(label, sizeof(label), "Saved Dumps (%lu)", (unsigned long)App->NumberOfSavedTags);

    if(App->Submenu) {
        submenu_change_item_label(App->Submenu, UHFReaderSubmenuIndexSaved, label);
    }

    if(App->SubmenuSaved) {
        small_submenu_set_header(App->SubmenuSaved, label);
    }
}

uint32_t uhf_saved_recount_and_repair_index(UHFReaderApp* App) {
    if(!App || !App->EpcFile) return 0;

    uint32_t count = 0;

    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        /*
         * Saved_EPCs.txt is the source of truth.
         * Entries are sequential Tag1, Tag2, ...; stop at the first missing key.
         * This makes Saved survive app/FAP replacement even if Index_File was stale.
         */
        FuriString* key = furi_string_alloc();
        FuriString* value = furi_string_alloc();
        for(uint32_t i = 1; i <= 1000U; i++) {
            furi_string_printf(key, "Tag%lu", (unsigned long)i);
            furi_string_reset(value);

            bool found =
                flipper_format_read_string(App->EpcFile, furi_string_get_cstr(key), value);

            if(!found) break;
            count = i;
        }
        furi_string_free(key);
        furi_string_free(value);

        flipper_format_file_close(App->EpcFile);
    } else {
        flipper_format_file_close(App->EpcFile);
    }

    App->NumberOfSavedTags = count;
    uhf_saved_write_index_count(App, count);
    uhf_saved_update_count_labels(App);

    UHF_I("SAVE", "RECOUNT Saved_EPCs.txt -> %lu entries", (unsigned long)count);
    uhf_debug_flush();

    return count;
}

/**
 * @brief      Function to update the saved epcs file
 * @details    This function updates the dictionary that is being used to store all the saved epcs.
 * @param      context  - The UHFReaderApp
*/
void update_dictionary_keys(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    uhf_reader_saved_menu_rebuild(App);
}

/**
 * @brief      Function to delete and update a saved tag in the saved epcs file
 * @details    This function deletes the specified tag and updates the saved epcs file
 * @param      context  - The UHFReaderApp
 * @param      key_to_delete  - The index of the saved UHF tag to delete
*/
void delete_and_update_entry(void* context, uint32_t KeyToDelete) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    uint32_t TotalTags = App->NumberOfSavedTags;

    UHF_I(
        "DELETE",
        "FILE BEGIN index=%lu total=%lu",
        (unsigned long)KeyToDelete,
        (unsigned long)TotalTags);

    if((KeyToDelete == 0U) || (KeyToDelete > TotalTags)) {
        UHF_E(
            "DELETE",
            "FILE INVALID index=%lu total=%lu",
            (unsigned long)KeyToDelete,
            (unsigned long)TotalTags);
        return;
    }

    FuriString* EpcToDelete = furi_string_alloc();

    // Open the saved epcs file
    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FuriString* TempStrOld = furi_string_alloc();
        FuriString* TempStrNew = furi_string_alloc();
        FuriString* TempTag = furi_string_alloc();

        // Update subsequent keys
        for(uint32_t i = 1; i <= TotalTags; i++) {
            furi_string_printf(TempStrOld, "Tag%ld", i);
            furi_string_reset(TempStrNew);
            furi_string_reset(TempTag);

            // Calculate the new key based on the deletion
            uint32_t NewKey = (i > KeyToDelete) ? i - 1 : i;

            // Skip the deleted key
            if(i != KeyToDelete) {
                furi_string_printf(TempStrNew, "Tag%ld", NewKey);
                if(!flipper_format_read_string(
                       App->EpcFile, furi_string_get_cstr(TempStrOld), TempTag)) {
                    FURI_LOG_D(TAG, "Could not read tag %ld data", i);
                } else {
                    if(!flipper_format_update_string_cstr(
                           App->EpcFile,
                           furi_string_get_cstr(TempStrNew),
                           furi_string_get_cstr(TempTag))) {
                        FURI_LOG_D(TAG, "Could not update tag %ld data", i);
                        flipper_format_write_string_cstr(
                            App->EpcFile,
                            furi_string_get_cstr(TempStrNew),
                            furi_string_get_cstr(TempTag));
                    }
                }
            }
        }

        furi_string_free(TempTag);
        furi_string_free(TempStrOld);
        furi_string_free(TempStrNew);

        furi_string_printf(EpcToDelete, "Tag%ld", App->NumberOfSavedTags);
        if(!flipper_format_delete_key(App->EpcFile, furi_string_get_cstr(EpcToDelete))) {
            FURI_LOG_D(TAG, "Could not delete saved tag with index %ld", App->NumberOfSavedTags);
        }

        // Update the total number of saved tags
        App->NumberOfSavedTags--;
        flipper_format_file_close(App->EpcFile);
    } else {
        UHF_E("DELETE", "FILE OPEN FAILED");
        flipper_format_file_close(App->EpcFile);
    }

    // Update/repair the count key after deletion.
    if(!uhf_saved_write_index_count(App, App->NumberOfSavedTags)) {
        FURI_LOG_E(TAG, "Failed to update index file after delete");
    }
    uhf_saved_update_count_labels(App);

    furi_string_free(EpcToDelete);
    UHF_I("DELETE", "FILE END total=%lu", (unsigned long)App->NumberOfSavedTags);
}

/**
 * @brief      Function to convert a hex character to its integer value
 * @details    This function converts a hex character to its integer value in ASCII
 * @param      char c  - The char to convert
 * @return     the int - the converted integer
*/
uint8_t hex_char_to_int(char c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    } else if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return 0;
}

/**
 * @brief      Function to convert a hex string to byte array
 * @details    This function converts a hex string to a byte array
 * @param      char hex_string  - The string to convert
 * @param      the byte_array - the allocated byte array that should be empty and passed in
 * @param      the lenght - The length variable fort the byte array
*/
bool hex_string_to_bytes(
    const char* hex_string,
    uint8_t* byte_array,
    size_t byte_array_capacity,
    size_t* byte_array_len) {
    if(byte_array_len) *byte_array_len = 0U;
    if(!hex_string || !byte_array || !byte_array_len) return false;

    const size_t hex_len = strlen(hex_string);
    const size_t required = hex_len / 2U;
    if((hex_len & 1U) != 0U || required > byte_array_capacity) return false;

    for(size_t i = 0; i < hex_len; i++) {
        const char c = hex_string[i];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }

    for(size_t i = 0; i < required; i++) {
        byte_array[i] = (uint8_t)((hex_char_to_int(hex_string[2U * i]) << 4U) |
                                  hex_char_to_int(hex_string[2U * i + 1U]));
    }
    *byte_array_len = required;
    return true;
}

/**
 * @brief      Function to convert a hex string to byte array (uint16)
 * @details    This function converts a hex string to a byte array
 * @param      char hex_string  - The string to convert
 * @param      the byte_array - the allocated byte array that should be empty and passed in
 * @param      the lenght - The length variable fort the byte array
*/
bool hex_string_to_uint16(
    const char* hex_string,
    uint16_t* uint16_array,
    size_t uint16_array_capacity,
    size_t* uint16_array_len) {
    if(uint16_array_len) *uint16_array_len = 0U;
    if(!hex_string || !uint16_array || !uint16_array_len) return false;

    const size_t hex_len = strlen(hex_string);
    const size_t required = hex_len / 4U;
    if((hex_len % 4U) != 0U || required > uint16_array_capacity) return false;

    for(size_t i = 0; i < hex_len; i++) {
        const char c = hex_string[i];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }

    for(size_t i = 0; i < required; i++) {
        uint16_array[i] = (uint16_t)((hex_char_to_int(hex_string[4U * i]) << 12U) |
                                     (hex_char_to_int(hex_string[4U * i + 1U]) << 8U) |
                                     (hex_char_to_int(hex_string[4U * i + 2U]) << 4U) |
                                     hex_char_to_int(hex_string[4U * i + 3U]));
    }
    *uint16_array_len = required;
    return true;
}

/**
 * @brief      Function to convert a uint16 value to a hex string
 * @details    Function to convert a uint16 value to a hex string
 * @param      uint16 value  - value to convert
 * @return     char* pointer to the string
*/
char* uint16_to_hex_string(uint16_t value) {
    // Allocate memory for the hex string (4 characters for hex + 1 for null terminator)
    char* hex_string = (char*)malloc(5 * sizeof(char));
    if(hex_string == NULL) {
        // Handle memory allocation failure
        return NULL;
    }

    // Convert the value to hex and store it in the string
    snprintf(hex_string, 5, "%04X", value);

    return hex_string;
}

/**
 * @brief       Function to combine two arrays together
 * @details    Function to combine two arrays together specifically for the PCs and CRCs...
 * @param      array1 - the first array to combine
 * @param      array2 - the second array to combine
 * @return     char* pointer to the string
*/
char* combineArrays(const char* array1, const char* array2) {
    // Allocate memory for the new array (size 4 + 4 = 8)
    char* combinedArray = (char*)malloc(16 * sizeof(char));
    if(combinedArray == NULL) {
        return NULL;
    }

    // Copy the elements from the first array
    memcpy(combinedArray, array1, 8 * sizeof(char));

    // Copy the elements from the second array
    memcpy(combinedArray + 8, array2, 8 * sizeof(char));

    return combinedArray;
}

/**
 * @brief      Function that converts byte array to uint32
 * @details    Function that converts byte array to 32-bit int
 * @param      bytes - the byte array
 * @param      length - the length of the byte array
 * @return     uint32 the 32 bit int
*/
uint32_t bytes_to_uint32(uint8_t* bytes, size_t length) {
    if(length > sizeof(uint32_t)) {
        return 0;
    }

    uint32_t result = 0;
    for(size_t i = 0; i < length; i++) {
        result = (result << 8) | bytes[i];
    }

    return result;
}

/**
 * @brief      Appends a single tag to the saved EPCs file.
 * @details    Writes "Name:Epc:Tid:Res:Mem:Pc:Crc" under a new TagN key,
 *             refreshes the active Saved page, and bumps the index file.
 *             This is the single source of truth for the on-disk save format.
*/
void save_uhf_tag_to_file(
    UHFReaderApp* App,
    const char* Name,
    const char* Epc,
    const char* Tid,
    const char* Res,
    const char* Mem,
    const char* Pc,
    const char* Crc) {
    //Open the saved EPCS file for appending
    if(!flipper_format_file_open_append(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        FURI_LOG_E(TAG, "Failed to open file");
        flipper_format_file_close(App->EpcFile);
        return;
    }

    FuriString* NumEpcs = furi_string_alloc();
    FuriString* EpcAndName = furi_string_alloc();

    //Increment the total number of saved tags and build the new tag entry
    App->NumberOfSavedTags++;
    furi_string_printf(NumEpcs, "Tag%ld", App->NumberOfSavedTags);
    furi_string_printf(EpcAndName, "%s:%s:%s:%s:%s:%s:%s", Name, Epc, Tid, Res, Mem, Pc, Crc);

    //Attempt to write the string using the given format
    if(!flipper_format_write_string_cstr(
           App->EpcFile, furi_string_get_cstr(NumEpcs), furi_string_get_cstr(EpcAndName))) {
        FURI_LOG_E(TAG, "Failed to write to file");
        App->NumberOfSavedTags--;
        flipper_format_file_close(App->EpcFile);
    } else {
        flipper_format_file_close(App->EpcFile);

        // Persist the count by UPDATING the existing key, not appending duplicates.
        if(!uhf_saved_write_index_count(App, App->NumberOfSavedTags)) {
            FURI_LOG_E(TAG, "Failed to update index file");
        }
        uhf_saved_update_count_labels(App);
        if(App->SubmenuSaved) uhf_reader_saved_menu_rebuild(App);
    }

    furi_string_free(EpcAndName);
    furi_string_free(NumEpcs);
}

/**
 * @brief      Text-input result callback for the EPC Dump / Banks Up-key save.
 * @details    Reads the currently displayed tag fields from App->SaveSourceView
 *             and writes them to disk, then returns to App->SaveReturnView.
*/
void uhf_reader_save_tag_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    FuriString* Epc = furi_string_alloc();
    FuriString* Tid = furi_string_alloc();
    FuriString* Res = furi_string_alloc();
    FuriString* Mem = furi_string_alloc();
    FuriString* Pc = furi_string_alloc();
    FuriString* Crc = furi_string_alloc();

    //Snapshot the tag fields from the originating view model
    with_view_model(
        App->SaveSourceView,
        UHFRFIDTagModel * model,
        {
            furi_string_set(Epc, model->Epc);
            furi_string_set(Tid, model->Tid);
            furi_string_set(Res, model->Reserved);
            furi_string_set(Mem, model->User);
            furi_string_set(Pc, model->Pc);
            furi_string_set(Crc, model->Crc);
        },
        false);

    save_uhf_tag_to_file(
        App,
        App->TempSaveBuffer,
        furi_string_get_cstr(Epc),
        furi_string_get_cstr(Tid),
        furi_string_get_cstr(Res),
        furi_string_get_cstr(Mem),
        furi_string_get_cstr(Pc),
        furi_string_get_cstr(Crc));

    furi_string_free(Epc);
    furi_string_free(Tid);
    furi_string_free(Res);
    furi_string_free(Mem);
    furi_string_free(Pc);
    furi_string_free(Crc);

    dolphin_deed(DolphinDeedRfidSave);
    view_dispatcher_switch_to_view(App->ViewDispatcher, App->SaveReturnView);
}

/**
 * @brief      Back/cancel callbacks for the Up-key save text input.
 * @details    The text-input view's context is the module's own object, not the
 *             app, so these hardcode the return view instead of reading
 *             App->SaveReturnView (which would dereference the wrong pointer).
*/
static uint32_t uhf_reader_save_cancel_to_epc_dump(void* context) {
    UNUSED(context);
    return UHFReaderViewEpcDump;
}

static uint32_t uhf_reader_save_cancel_to_banks(void* context) {
    UNUSED(context);
    return UHFReaderViewEpcInfo;
}

/**
 * @brief      Opens the save text input for the tag shown in SourceView.
 * @details    Prefills a default name of Tag_<last 8 EPC chars> and arranges
 *             the result/cancel callbacks to return to ReturnView.
*/
void uhf_reader_begin_save_tag(UHFReaderApp* App, View* SourceView, uint32_t ReturnView) {
    App->SaveSourceView = SourceView;
    App->SaveReturnView = ReturnView;

    //Build a default name from the last 8 characters of the EPC
    with_view_model(
        SourceView,
        UHFRFIDTagModel * model,
        {
            const char* EpcStr = furi_string_get_cstr(model->Epc);
            size_t Len = strlen(EpcStr);
            const char* Tail = Len > 8 ? EpcStr + (Len - 8) : EpcStr;
            snprintf(App->TempSaveBuffer, App->TempBufferSaveSize, "Tag_%s", Tail);
        },
        false);

    text_input_set_header_text(App->SaveInput, "Save EPC");
    text_input_set_result_callback(
        App->SaveInput,
        uhf_reader_save_tag_text_updated,
        App,
        App->TempSaveBuffer,
        App->TempBufferSaveSize,
        false);
    view_set_previous_callback(
        text_input_get_view(App->SaveInput),
        ReturnView == UHFReaderViewEpcInfo ? uhf_reader_save_cancel_to_banks :
                                             uhf_reader_save_cancel_to_epc_dump);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaveInput);
}
