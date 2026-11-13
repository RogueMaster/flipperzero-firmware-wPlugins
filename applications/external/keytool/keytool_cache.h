#pragma once

#include "keytool_i.h"
#include "keytool_mifare.h"

#define KEYTOOL_CACHE_FOLDER KEYTOOL_APP_FOLDER "/cache"

// Flipper's own user dictionary: cached keys are imported here.
#define KEYTOOL_USER_DICT_PATH "/ext/nfc/assets/mf_classic_dict_user.nfc"

// Stores, for one scanned dump, every key that was NOT found in any
// dictionary. One file per dump name in KEYTOOL_CACHE_FOLDER; a rescan of
// the same dump replaces its file. If every key was found, the dump's
// cache file (if any) is removed.
void keytool_cache_save(
    Storage* storage,
    const char* dump_name,
    const KeyToolKeyEntry* keys,
    size_t key_count,
    const bool* found_matrix,
    size_t dict_count);

// Custom "Cache" list view: one row per dump ("<dump name> (<N keys>)").
//   UP/DOWN      move selection
//   OK           show the cached keys of that dump
//   hold OK      ask for confirmation, then import that dump's keys into
//                the user dictionary (and drop them from the cache)
//   LEFT         ask for confirmation, then delete that dump's cache file
//   BACK         return to the main menu
View* keytool_cache_list_alloc(KeyToolApp* app);
void keytool_cache_list_free(View* view);

// Rescans KEYTOOL_CACHE_FOLDER and rebuilds the list.
void keytool_cache_list_refresh(KeyToolApp* app);
