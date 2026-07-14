#pragma once

// memory info
#define MEMORY_INCLUDE "stdlib.h"
#define MEMORY_ALLOC   malloc
#define MEMORY_FREE    free
#define MEMORY_REALLOC realloc

// storage info
#define STORAGE_INCLUDE       "auto_complete/storage.h"
#define STORAGE_READ          storage_read
#define STORAGE_MAX_READ_SIZE 4096
