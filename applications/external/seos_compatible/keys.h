#pragma once

#include <stdbool.h>
#include "seos.h"

#define SEOS_DEFAULT_KEYS_FILENAME "keys"

extern size_t SEOS_ADF_OID_LEN;
extern uint8_t SEOS_ADF_OID[32];
extern uint8_t SEOS_ADF1_PRIV_ENC[16];
extern uint8_t SEOS_ADF1_PRIV_MAC[16];
extern uint8_t SEOS_ADF1_READ[16];
extern uint8_t SEOS_ADF1_WRITE[16];

bool seos_load_keys_from_file(Seos* seos, const char* filename);
void seos_reset_to_zero_keys(Seos* seos);
bool seos_migrate_keys(Seos* seos);
