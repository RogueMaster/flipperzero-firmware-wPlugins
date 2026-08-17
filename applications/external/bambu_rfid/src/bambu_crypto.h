#pragma once

#include "bambu_rfid.h"
#include <nfc/protocols/mf_classic/mf_classic.h>

bool br_derive_sector_keys(const uint8_t uid[BR_UID_SIZE], MfClassicDeviceKeys* keys);
