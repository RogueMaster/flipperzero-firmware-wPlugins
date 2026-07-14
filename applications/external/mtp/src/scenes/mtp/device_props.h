#pragma once

#include <furi.h>
#include "main.h"
#include "mtp.h"

// Device property operations
void GetDevicePropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code);
void GetDevicePropDesc(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code);
int GetDevicePropValueInternal(uint32_t prop_code, uint8_t* buffer);
int GetDevicePropDescInternal(uint32_t prop_code, uint8_t* buffer);
int BuildDeviceInfo(uint8_t* buffer);
