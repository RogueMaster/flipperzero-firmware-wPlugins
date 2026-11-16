#pragma once

#include "dfc.h"
#include "dfc_credential.h"

DfcFile* dfc_credential_find_transaction_mac_file(DfcCredential* credential, size_t app_index);
