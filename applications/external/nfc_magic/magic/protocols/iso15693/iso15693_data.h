#pragma once

#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>

// Thin wrapper around the SDK's Iso15693_3Data. The ISO15693-3 poller already fills in
// system_info (block count/size, DSFID, AFI, IC ref) and the block data during activation,
// so there is nothing to duplicate here, everything the ISO15693 scenes need lives inside
// iso15693_3_data.
typedef struct {
    Iso15693_3Data* iso15693_3_data;
} Iso15693Data;

Iso15693Data* iso15693_data_alloc();

void iso15693_data_free(Iso15693Data* instance);

void iso15693_data_reset(Iso15693Data* instance);

void iso15693_data_copy(Iso15693Data* target, const Iso15693Data* source);
