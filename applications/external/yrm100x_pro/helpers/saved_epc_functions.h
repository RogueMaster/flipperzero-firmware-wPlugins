#pragma once
#include "../app.h"

//Function Declarations
void delete_and_update_entry(void* context, uint32_t KeyToDelete);

void update_dictionary_keys(void* context);

uint8_t hex_char_to_int(char c);

bool hex_string_to_bytes(
    const char* hex_string,
    uint8_t* byte_array,
    size_t byte_array_capacity,
    size_t* byte_array_len);

char* uint16_to_hex_string(uint16_t value);

bool hex_string_to_uint16(
    const char* hex_string,
    uint16_t* uint16_array,
    size_t uint16_array_capacity,
    size_t* uint16_array_len);

char* combineArrays(const char* array1, const char* array2);

uint32_t bytes_to_uint32(uint8_t* bytes, size_t length);

// Recount Tag1..TagN directly from Saved_EPCs.txt and repair Index_File.txt.
// Returns the number of valid sequential saved entries found on disk.
uint32_t uhf_saved_recount_and_repair_index(UHFReaderApp* App);

void uhf_saved_update_count_labels(UHFReaderApp* App);

// Shared writer: appends one tag to Saved_EPCs.txt, updates the Saved submenu
// and Index_File.txt. All fields are plain colon-free strings.
void save_uhf_tag_to_file(
    UHFReaderApp* App,
    const char* Name,
    const char* Epc,
    const char* Tid,
    const char* Res,
    const char* Mem,
    const char* Pc,
    const char* Crc);

// Text-input result callback for the EPC Dump / Banks Up-key save. Reads the
// tag fields from App->SaveSourceView and returns to App->SaveReturnView.
void uhf_reader_save_tag_text_updated(void* context);

// Opens the save text input for the tag currently shown in SourceView,
// prefilling a default name and arranging to return to ReturnView.
void uhf_reader_begin_save_tag(UHFReaderApp* App, View* SourceView, uint32_t ReturnView);
