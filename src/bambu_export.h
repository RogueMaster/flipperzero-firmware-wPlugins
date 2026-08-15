#pragma once

#include "bambu_rfid.h"

#include <nfc/protocols/mf_classic/mf_classic.h>

bool br_save_tag_bundle(
    Storage* storage,
    const MfClassicData* data,
    const BrTagInfo* info,
    char* out_path,
    size_t out_path_size);
