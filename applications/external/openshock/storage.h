#pragma once

#include "protocols.h"
#include <stdbool.h>

#define OPENSHOCK_SAVE_DIR     APP_DATA_PATH("shockers")
#define OPENSHOCK_MAX_SAVED    32
#define OPENSHOCK_NAME_MAX_LEN 32
#define OPENSHOCK_PATH_MAX_LEN 128

typedef struct {
    char filename[OPENSHOCK_PATH_MAX_LEN]; // Full path
    char name[OPENSHOCK_NAME_MAX_LEN];
    ShockerModel model;
    uint16_t shocker_id;
    uint8_t channel;
} SavedShocker;

// Save a shocker config. Name is used for the filename and stored in the file.
bool openshock_shocker_save(const char* name, ShockerModel model, uint16_t id, uint8_t channel);

// Load the list of saved shockers. Returns number loaded.
size_t openshock_shocker_list(SavedShocker* list, size_t max_count);

// Delete a saved shocker by its full path.
bool openshock_shocker_delete(const char* path);
