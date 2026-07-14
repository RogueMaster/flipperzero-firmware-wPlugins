#pragma once

#include <storage/storage.h>
#include "main.h"
#include "mtp.h"

// Storage utility functions
char* get_base_path_from_storage_id(uint32_t storage_id);
char* get_path_from_handle(AppMTP* mtp, uint32_t handle);
uint32_t issue_object_handle(AppMTP* mtp, char* path);
uint32_t update_object_handle_path(AppMTP* mtp, uint32_t handle, char* path);
int list_and_issue_handles(
    AppMTP* mtp,
    uint32_t storage_id,
    uint32_t association,
    uint32_t* handles);
