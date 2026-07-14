#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "main.h"
#include "mtp.h"
#include "storage_ops.h"

// MTP Operations (Actions)
void GetStorageIDs(AppMTP* mtp, uint32_t* storage_ids, uint32_t* count);
int GetStorageInfo(AppMTP* mtp, uint32_t storage_id, uint8_t* buf);
int GetObjectInfo(AppMTP* mtp, uint32_t handle, uint8_t* buffer);
void GetObject(AppMTP* mtp, uint32_t transaction_id, uint32_t handle);
int GetObjectHandles(AppMTP* mtp, uint32_t storage_id, uint32_t association, uint8_t* buffer);
void GetObjectPropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t handle, uint32_t prop_code);
bool DeleteObject(AppMTP* mtp, uint32_t handle);
void MoveObject(
    AppMTP* mtp,
    uint32_t transaction_id,
    uint32_t handle,
    uint32_t storage_id,
    uint32_t parent);
