#pragma once
#include "irb_storage.h"
#define IRB_CATALOG_LIMIT 256
#define IRB_PAGE_SIZE 6
#define IRB_FILE_LIMIT 512
#define IRB_FILE_NAME_BYTES (16U * 1024U)
typedef struct {
    char name[IRB_NAME_SIZE];
    uint32_t offset;
    bool usable;
} IrbImportEntry;
typedef struct {
    char path[IRB_PATH_SIZE];
    uint32_t hash, size, count;
    IrbImportEntry entries[IRB_CATALOG_LIMIT];
} IrbCatalog;
typedef struct {
    char path[IRB_PATH_SIZE];
    char names[IRB_PAGE_SIZE][IRB_PATH_SIZE];
    bool directories[IRB_PAGE_SIZE];
    uint32_t start, count, total;
    bool projects;
} IrbFilePage;
typedef struct {
    char* name;
    bool directory;
} IrbFileEntry;
typedef struct {
    IrbFileEntry* entries;
    char path[IRB_PATH_SIZE];
    uint32_t count, capacity, name_bytes;
    bool projects;
} IrbFileList;
void irb_files_clear(IrbFileList* files);
bool irb_files_load(Storage* storage, IrbFileList* files, const char* path, bool projects,
                    char* error, IrbLoadProgress progress, void* context);
void irb_files_get_page(const IrbFileList* files, uint32_t start, IrbFilePage* page);
bool irb_catalog_load(Storage* storage, IrbCatalog* catalog, const char* path, char* error,
                      IrbLoadProgress progress, void* context);
// entry == -1 adds all available names. Original source is copied into app storage.
bool irb_catalog_add(Storage* storage, const IrbCatalog* catalog, IrbProject* project, int entry,
                     uint32_t* added, uint32_t* mapped, char* error, IrbLoadProgress progress,
                     void* context);
bool irb_files_page(Storage* storage, IrbFilePage* page, char* error, IrbLoadProgress progress,
                    void* context);
