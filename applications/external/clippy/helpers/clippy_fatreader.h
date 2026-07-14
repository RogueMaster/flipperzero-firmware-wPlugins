#pragma once

#include <stdio.h>
#include <unistd.h>
#include "../clippy_app_i.h"

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))
#define BYTES_PER_SECTOR 512

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;

#define FR_ATTR_READ_ONLY      0x01
#define FR_ATTR_HIDDEN         0x02
#define FR_ATTR_SYSTEM         0x04
#define FR_ATTR_VOLUME_ID      0x08
#define FR_ATTR_DIRECTORY      0x10
#define FR_ATTR_ARCHIVE        0x20
#define FR_ATTR_LONG_FILE_NAME 0x0f

typedef enum {
    FR_OK,
    FR_READ_FAILED,
    FR_OPEN_FAILED,
    FR_CLOSE_FAILED,
    FR_SEEK_FAILED,
    FR_NO_FILE_FOUND,
    FR_EMPTY_FILE,
    FR_EOF,
    FR_FILE_NOT_FOUND,
    FR_NOT_FAT,
} FRESULT;

typedef struct {
    u32 volume_id;
    u16 sign;
    u8 drive_number;
    u8 extended_boot_signature;
} BootSector;

typedef struct {
    u16 bytes_per_sec;
    u16 reserved_sectors_count;
    u16 root_entries_count;
    u16 total_sectors_16;
    u16 fat_size_16;
    u8 sectors_per_cluster;
    u8 num_fats;
    u8 media;
} BiosParameterBlock;

typedef enum {
    FT_12 = 12,
    FT_16 = 16
} FatType;

typedef struct {
    FatType fat_type;
    BootSector boot_sector;
    BiosParameterBlock bios_parameter_block;
    File* image_file_ptr;
    u32 fat_sectors;
    u32 root_dir_start_sector;
    u32 root_dir_sectors;
    u32 data_start_sector;
    u32 data_sectors;
} FHandle;

typedef struct {
    FHandle* handle;
    u32 file_size;
    u32 cluster;
    char name[12];
    u8 attr;
} DIR;

typedef struct {
    DIR* dir;
    u16 cur_cluster;
    u32 cur_sector;
    size_t cur_sector_ptr;
    size_t read_bytes;
} FIL;

typedef struct {
    FHandle* handle;
    u16 processed_entries;
    u8* buffer;
    u8* current_ptr;
} RootDirectory;

// Main functions
FRESULT fatreader_open_image(FHandle* handle, const char* image_filename, Storage* storage);
FRESULT fatreader_close_image(FHandle* handle);
void fatreader_print_info(FHandle* handle);

// Root Directory
FRESULT fatreader_root_directory_open(RootDirectory* root_directory, FHandle* handle);
FRESULT fatreader_root_directory_close(RootDirectory* root_directory);
FRESULT fatreader_root_directory_find_first(DIR* dir, RootDirectory* root_directory);
FRESULT fatreader_root_directory_find_next(DIR* dir, RootDirectory* root_directory);
FRESULT fatreader_root_directory_find_by_name(
    const char* name,
    DIR* dir,
    RootDirectory* root_directory);

// File operations
FRESULT fatreader_file_open(FIL* file, DIR* dir);
FRESULT fatreader_file_read(u8* buffer, FIL* file, size_t bytes_to_read, size_t* bytes_read);

// Utility functions
u16 fatreader_get_word(u8* ptr);
u32 fatreader_get_dword(u8* ptr);
