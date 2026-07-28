#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "protocols.h"
#include <stdbool.h>

#define OPENSHOCK_SAVE_DIR     APP_DATA_PATH("shockers")
#define OPENSHOCK_MAX_SAVED    32
#define OPENSHOCK_NAME_MAX_LEN 32
#define OPENSHOCK_PATH_MAX_LEN 128

typedef struct {
    char filename[OPENSHOCK_PATH_MAX_LEN];
    char name[OPENSHOCK_NAME_MAX_LEN];
    ShockerModel model;
    uint16_t shocker_id;
    uint8_t channel;
    uint8_t sync_group;
} SavedShocker;

// replace_path: remove after successful write when renaming to a different file.
bool openshock_shocker_write(
    const char* stem,
    ShockerModel model,
    uint16_t id,
    uint8_t channel,
    uint8_t sync_group,
    const char* replace_path);

bool openshock_shocker_unique_stem(ShockerModel model, uint16_t id, char* stem_out, size_t stem_sz);

size_t openshock_shocker_list(SavedShocker* list, size_t max_count);

bool openshock_shocker_delete(const char* path);

bool openshock_stem_exists(const char* stem);
