# Flipper Zero — File I/O & Storage Reference

> Sources:
> - Flipper firmware headers: `storage/storage.h`
> - https://github.com/m1ch3al/flipper-zero-dev-tutorial

## Overview

Flipper Zero has a microSD card slot for persistent storage. Apps store data under `/ext/apps_data/<appid>/`. The Storage API provides file and directory operations.

## Path Conventions

```
/ext/                                    # SD card root
/ext/apps/                               # installed FAP files
/ext/apps_data/                          # app-specific data
/ext/apps_data/claude_remote/            # our app's data root
/ext/apps_data/claude_remote/manual/     # manual chapter files
/ext/apps_data/claude_remote/guide/      # optional shortcut metadata
```

The `APP_DATA_PATH()` macro resolves to `/ext/apps_data/<appid>/`:

```c
#define MANUAL_DIR APP_DATA_PATH("manual")
// expands to: "/ext/apps_data/claude_remote/manual"
```

## Required Includes

```c
#include <storage/storage.h>
#include <toolbox/path.h>       // path helpers
#include <toolbox/stream/stream.h>        // optional: stream API
#include <toolbox/stream/file_stream.h>   // optional: file stream
```

## Basic File Operations

### Reading a File

```c
Storage* storage = furi_record_open(RECORD_STORAGE);
File* file = storage_file_alloc(storage);

if(storage_file_open(file, "/ext/apps_data/claude_remote/manual/01_getting_started.txt",
                     FSAM_READ, FSOM_OPEN_EXISTING)) {
    char buffer[256];
    uint16_t bytes_read;

    while((bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        // process buffer
    }
}

storage_file_close(file);
storage_file_free(file);
furi_record_close(RECORD_STORAGE);
```

### File Open Modes

| Mode | Description |
|------|-------------|
| `FSAM_READ` | Read access |
| `FSAM_WRITE` | Write access |
| `FSAM_READ_WRITE` | Read and write |

| Open Mode | Description |
|-----------|-------------|
| `FSOM_OPEN_EXISTING` | Fail if file doesn't exist |
| `FSOM_OPEN_ALWAYS` | Open or create |
| `FSOM_OPEN_APPEND` | Open for appending |
| `FSOM_CREATE_NEW` | Create new, fail if exists |
| `FSOM_CREATE_ALWAYS` | Create new, overwrite if exists |

### Check if File/Dir Exists

```c
Storage* storage = furi_record_open(RECORD_STORAGE);
bool exists = storage_file_exists(storage, path);
bool dir_exists = storage_dir_exists(storage, dir_path);
furi_record_close(RECORD_STORAGE);
```

## Directory Operations

### List Files in Directory

```c
Storage* storage = furi_record_open(RECORD_STORAGE);
File* dir = storage_file_alloc(storage);

if(storage_dir_open(dir, MANUAL_DIR)) {
    FileInfo fileinfo;
    char name[64];

    while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
        if(!(fileinfo.flags & FSF_DIRECTORY)) {
            // name contains filename (e.g., "01_getting_started.txt")
            FURI_LOG_I(TAG, "Found: %s", name);
        }
    }
}

storage_dir_close(dir);
storage_file_free(dir);
furi_record_close(RECORD_STORAGE);
```

### Create Directory

```c
Storage* storage = furi_record_open(RECORD_STORAGE);
storage_simply_mkdir(storage, MANUAL_DIR);
furi_record_close(RECORD_STORAGE);
```

## Loading Manual Chapters

Pattern for Claude Remote's manual loader:

```c
#define MAX_CHAPTERS 10
#define MAX_CHAPTER_SIZE 2048
#define MANUAL_DIR APP_DATA_PATH("manual")

typedef struct {
    char title[32];
    char* content;        // malloc'd
    uint16_t content_len;
} Chapter;

typedef struct {
    Chapter chapters[MAX_CHAPTERS];
    uint8_t chapter_count;
} Manual;

static bool load_manual_from_sd(Manual* manual) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    manual->chapter_count = 0;

    if(!storage_dir_open(dir, MANUAL_DIR)) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return false;  // no SD files, use fallback
    }

    // Collect filenames
    char names[MAX_CHAPTERS][64];
    uint8_t count = 0;
    FileInfo fi;
    while(storage_dir_read(dir, &fi, names[count], 64) && count < MAX_CHAPTERS) {
        if(!(fi.flags & FSF_DIRECTORY)) count++;
    }
    storage_dir_close(dir);

    // Sort names (simple bubble sort on short array)
    for(int i = 0; i < count - 1; i++) {
        for(int j = 0; j < count - i - 1; j++) {
            if(strcmp(names[j], names[j+1]) > 0) {
                char tmp[64];
                memcpy(tmp, names[j], 64);
                memcpy(names[j], names[j+1], 64);
                memcpy(names[j+1], tmp, 64);
            }
        }
    }

    // Load each file
    for(int i = 0; i < count; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", MANUAL_DIR, names[i]);

        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t size = storage_file_size(file);
            if(size > MAX_CHAPTER_SIZE) size = MAX_CHAPTER_SIZE;

            manual->chapters[manual->chapter_count].content = malloc(size + 1);
            uint16_t read = storage_file_read(
                file,
                manual->chapters[manual->chapter_count].content,
                size);
            manual->chapters[manual->chapter_count].content[read] = '\0';
            manual->chapters[manual->chapter_count].content_len = read;

            // Extract title from filename (strip prefix + extension)
            // "01_getting_started.txt" -> "Getting Started"
            // ... title extraction logic ...

            manual->chapter_count++;
        }
        storage_file_close(file);
        storage_file_free(file);
    }

    furi_record_close(RECORD_STORAGE);
    return manual->chapter_count > 0;
}

static void free_manual(Manual* manual) {
    for(int i = 0; i < manual->chapter_count; i++) {
        if(manual->chapters[i].content) {
            free(manual->chapters[i].content);
            manual->chapters[i].content = NULL;
        }
    }
    manual->chapter_count = 0;
}
```

## Compiled-In Fallback

For when SD card files are missing, embed default content as C string arrays:

```c
// manual_defaults.h
static const char* default_chapter_titles[] = {
    "Getting Started",
    "Keybindings",
    "Slash Commands",
    "Skills & Tools",
    "Playbooks",
};

static const char* default_chapter_content[] = {
    "Claude Code is an AI coding agent...",
    "Ctrl+C: Cancel current operation...",
    "/help: Show help...",
    "Skills are specialized capabilities...",
    "Recipe: Set up a new repo...",
};

#define DEFAULT_CHAPTER_COUNT 5
```

Load fallback when `load_manual_from_sd()` returns false.

## Storage Error Handling

```c
FS_Error error = storage_file_get_error(file);
if(error != FSE_OK) {
    FURI_LOG_E(TAG, "Storage error: %d", error);
}
```

| Error | Meaning |
|-------|---------|
| `FSE_OK` | Success |
| `FSE_NOT_READY` | SD card not mounted |
| `FSE_EXIST` | File already exists |
| `FSE_NOT_EXIST` | File not found |
| `FSE_INVALID_PARAMETER` | Bad argument |
| `FSE_DENIED` | Access denied |
| `FSE_INVALID_NAME` | Invalid path/name |
| `FSE_INTERNAL` | Internal error |
