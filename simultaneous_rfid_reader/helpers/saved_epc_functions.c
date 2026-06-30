#include "saved_epc_functions.h"

/**
 * @brief      Function to update the saved epcs file
 * @details    This function updates the dictionary that is being used to store all the saved epcs.
 * @param      context  - The UHFReaderApp
*/
void update_dictionary_keys(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    // Updating the saved epcs menu
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSaved);
    submenu_free(App->SubmenuSaved);
    App->SubmenuSaved = submenu_alloc();
    submenu_set_header(App->SubmenuSaved, "Saved EPCs");
    uint32_t TotalTags = App->NumberOfSavedTags;

    // Open the saved epcs file and extract the tag name and create the submenu items
    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        for(uint32_t i = 0; i < TotalTags; i++) {
            
            FuriString* TempStr = furi_string_alloc();
            FuriString* TempTag = furi_string_alloc();
            furi_string_printf(TempStr, "Tag%ld", i + 1);
            
            if(!flipper_format_read_string(
                   App->EpcFile, furi_string_get_cstr(TempStr), TempTag)) {
                FURI_LOG_D(TAG, "Could not read tag %ld data", i + 1);
            } else {
                
                // Extract the name of the saved UHF Tag
                const char* InputString = furi_string_get_cstr(TempTag);
                char* ExtractedName = extract_name(InputString);

                if(ExtractedName != NULL) {
                    submenu_add_item(
                        App->SubmenuSaved,
                        ExtractedName,
                        (i + 1),
                        uhf_reader_submenu_saved_callback,
                        App); 
                    free(ExtractedName);
                } 
            }
            furi_string_free(TempStr);
            furi_string_free(TempTag);
        }
    }
    view_set_previous_callback(
        submenu_get_view(App->SubmenuSaved), uhf_reader_navigation_saved_exit_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSaved, submenu_get_view(App->SubmenuSaved));
    flipper_format_file_close(App->EpcFile);
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
    FuriString* EpcToDelete = furi_string_alloc();

    // Open the saved epcs file
    if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
        
        // Update subsequent keys
        for(uint32_t i = 1; i <= TotalTags; i++) {
            FuriString* TempStrOld = furi_string_alloc();
            FuriString* TempStrNew = furi_string_alloc();
            furi_string_printf(TempStrOld, "Tag%ld", i);

            // Calculate the new key based on the deletion
            uint32_t NewKey = (i > KeyToDelete) ? i - 1 : i;

            // Skip the deleted key
            if(i != KeyToDelete) { 
                furi_string_printf(TempStrNew, "Tag%ld", NewKey);
                FuriString* TempTag = furi_string_alloc();
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

                furi_string_free(TempTag);
            }
            furi_string_free(TempStrOld);
            furi_string_free(TempStrNew);
        }

        furi_string_printf(EpcToDelete, "Tag%ld", App->NumberOfSavedTags);
        if(!flipper_format_delete_key(App->EpcFile, furi_string_get_cstr(EpcToDelete))) {
            FURI_LOG_D(
                TAG, "Could not delete saved tag with index %ld", App->NumberOfSavedTags);
        }
        
        // Update the total number of saved tags
        App->NumberOfSavedTags--;
        flipper_format_file_close(App->EpcFile);
    }

    // Update the index file
    FuriString* NewNumEpcs = furi_string_alloc();
    furi_string_printf(NewNumEpcs, "%ld", App->NumberOfSavedTags);
    if(flipper_format_file_open_existing(App->EpcIndexFile, APP_DATA_PATH("Index_File.txt"))) {
        if(!flipper_format_write_string_cstr(
               App->EpcIndexFile, "Number of Tags", furi_string_get_cstr(NewNumEpcs))) {
            FURI_LOG_E(TAG, "Failed to write to file");
        } else {
            FURI_LOG_E(TAG, "Updated index file!");
        }
    }
    furi_string_free(EpcToDelete);
    flipper_format_file_close(App->EpcIndexFile);
    furi_string_free(NewNumEpcs);
}


/**
 * @brief      Function to convert a hex character to its integer value
 * @details    This function converts a hex character to its integer value in ASCII
 * @param      char c  - The char to convert
 * @return     the int - the converted integer       
*/
uint8_t hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
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
void hex_string_to_bytes(const char* hex_string, uint8_t* byte_array, size_t* byte_array_len) {
    size_t hex_len = strlen(hex_string);
    if (hex_len % 2 != 0) {
        // Handle error: hex string length must be even
        *byte_array_len = 0;
        return;
    }
    
    *byte_array_len = hex_len / 2;
    for (size_t i = 0; i < *byte_array_len; i++) {
        byte_array[i] = (hex_char_to_int(hex_string[2 * i]) << 4) | hex_char_to_int(hex_string[2 * i + 1]);
    }
}

/**
 * @brief      Function to convert a hex string to byte array (uint16)
 * @details    This function converts a hex string to a byte array
 * @param      char hex_string  - The string to convert
 * @param      the byte_array - the allocated byte array that should be empty and passed in   
 * @param      the lenght - The length variable fort the byte array     
*/
void hex_string_to_uint16(const char* hex_string, uint16_t* uint16_array, size_t* uint16_array_len) {
    size_t hex_len = strlen(hex_string);
    if (hex_len % 4 != 0) {
        // Handle error: hex string length must be a multiple of 4 for uint16_t conversion
        *uint16_array_len = 0;
        return;
    }
    
    *uint16_array_len = hex_len / 4;
    for (size_t i = 0; i < *uint16_array_len; i++) {
        uint16_array[i] = (hex_char_to_int(hex_string[4 * i]) << 12) |
                          (hex_char_to_int(hex_string[4 * i + 1]) << 8) |
                          (hex_char_to_int(hex_string[4 * i + 2]) << 4) |
                          hex_char_to_int(hex_string[4 * i + 3]);
    }
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
    if (hex_string == NULL) {
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
    if (combinedArray == NULL) {
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
    if (length > sizeof(uint32_t)) {
        return 0;
    }

    uint32_t result = 0;
    for (size_t i = 0; i < length; i++) {
        result = (result << 8) | bytes[i];
    }

    return result;
}