#pragma once

#include "bambu_rfid.h"

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

bool br_dump_from_mf(const MfClassicData* data, BrTagDump* dump);
bool br_parse_dump(const BrTagDump* dump, BrTagInfo* info);
bool br_parse_mf(const MfClassicData* data, BrTagInfo* info);

bool br_load_tag_file(Storage* storage, const char* path, BrTagInfo* info);

void br_format_uid(const uint8_t uid[BR_UID_SIZE], char out[BR_UID_SIZE * 2U + 1U]);
