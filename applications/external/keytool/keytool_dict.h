#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "keytool_mifare.h"

// Maximum number of dictionary files scanned in one run.
#define KEYTOOL_MAX_DICTS 16

// Automatically finds every dictionary KeyTool knows how to look for,
// with no configuration needed:
//  - /ext/nfc/assets/mf_classic_dict_user.nfc (Flipper's own user dict)
//  - /ext/nfc/assets/mf_classic_dict.nfc      (Flipper's own system dict)
//  - every *.dict / *.txt / *.nfc file directly inside /ext/nfc/dict
//  - every *.dict / *.txt / *.nfc file directly inside /ext/dict
// `paths_out` receives the full path to use for reading each file,
// `names_out` receives a short label to use when printing results.
// Both must point to `max_count` already-allocated FuriString*.
void keytool_dict_autodetect(
    Storage* storage,
    FuriString** paths_out,
    FuriString** names_out,
    size_t* count_out,
    size_t max_count);

// Scans one dictionary file line by line and, for every key in `keys`
// that matches a line, sets found_matrix[k * dict_count + dict_index].
// found_matrix must be pre-allocated as key_count * dict_count booleans.
void keytool_dict_scan_file(
    Storage* storage,
    const char* file_path,
    const KeyToolKeyEntry* keys,
    size_t key_count,
    bool* found_matrix,
    size_t dict_count,
    size_t dict_index);
