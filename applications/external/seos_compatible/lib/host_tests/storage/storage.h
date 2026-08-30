/* Enough of the storage service for the credential struct to have a layout. */
#pragma once

typedef struct Storage Storage;

#define STORAGE_APP_DATA_PATH_PREFIX "/data"
#define APP_DATA_PATH(path)          STORAGE_APP_DATA_PATH_PREFIX "/" path
