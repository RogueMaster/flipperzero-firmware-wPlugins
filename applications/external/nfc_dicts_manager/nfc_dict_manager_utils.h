#pragma once

#include "nfc_dict_manager.h"

void nfc_dict_manager_create_directories(Storage* storage);
bool nfc_dict_manager_copy_file(Storage* storage, const char* src, const char* dst);
bool nfc_dict_manager_is_valid_key(const char* key);
void nfc_dict_manager_normalize_key(char* key);
bool nfc_dict_manager_merge_dictionaries(NfcDictManager* app);
bool nfc_dict_manager_backup_dictionaries(NfcDictManager* app);

bool nfc_dict_manager_append_file_to_file(
    Storage* storage,
    FuriString* source_path,
    File* dest_file);
bool nfc_dict_manager_optimize_file(
    Storage* storage,
    FuriString* source_path,
    FuriString* dest_path);
int nfc_dict_manager_count_lines_in_file(Storage* storage, FuriString* file_path);

bool nfc_dict_manager_optimize_single_dict(NfcDictManager* app, FuriString* dict_path);
bool nfc_dict_manager_remove_duplicates_from_file(
    Storage* storage,
    FuriString* source_path,
    FuriString* dest_path);

void nfc_dict_manager_submenu_callback(void* context, uint32_t index);
bool nfc_dict_manager_back_event_callback(void* context);
void nfc_dict_manager_dialog_callback(DialogExResult result, void* context);
void nfc_dict_manager_success_timer_callback(void* context);
void nfc_dict_manager_merge_timer_callback(void* context);
void nfc_dict_manager_backup_timer_callback(void* context);
void nfc_dict_manager_optimize_timer_callback(void* context);
