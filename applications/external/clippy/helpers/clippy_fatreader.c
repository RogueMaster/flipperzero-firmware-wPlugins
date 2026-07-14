#include "clippy_fatreader.h"
#include "storage/filesystem_api_defines.h"
#include <stdlib.h>
#include <unistd.h>

#define IS_FILE_NOT_VALID(first_byte, attr) (first_byte == 0xe5 || attr & FR_ATTR_LONG_FILE_NAME)
#define IS_END(first_byte)                  (first_byte == 0x00)
#define TAG                                 "ClippyFatreaderBootSector"

static FatType determine_fat_type(u32 clusters) {
    if(clusters <= 4085) {
        return FT_12;
    }
    return FT_16;
}

static void read_dir_into(DIR* target, u8* directory) {
    furi_assert(target);
    target->cluster = fatreader_get_word(&directory[26]);
    target->file_size = fatreader_get_dword(&directory[28]);
    target->attr = directory[11];
    memcpy(target->name, directory, 11);
    target->name[11] = 0;
}

static u16 marker_by_fat_type(FatType type) {
    if(type == FT_12) {
        return 0xFF8;
    }
    return 0xFFF8;
}

u16 fatreader_get_word(u8* ptr) {
    return ptr[0] | ptr[1] << 8;
}

u32 fatreader_get_dword(u8* ptr) {
    return ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
}

FRESULT fatreader_open_image(FHandle* handle, const char* image_filename, Storage* storage) {
    File* fp = storage_file_alloc(storage);
    size_t ret;

    u8* sector_buffer = malloc(BYTES_PER_SECTOR);

    bool ok = storage_file_open(fp, image_filename, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!ok) {
        free(sector_buffer);
        return FR_OPEN_FAILED;
    }

    ret = storage_file_read(fp, sector_buffer, BYTES_PER_SECTOR);
    if(ret != BYTES_PER_SECTOR) {
        storage_file_close(fp);
        storage_file_free(fp);
        free(sector_buffer);
        return FR_READ_FAILED;
    }

    u16 fat_signature = fatreader_get_word(&sector_buffer[510]);

    if(fat_signature != 0xAA55) {
        free(sector_buffer);
        return FR_NOT_FAT;
    }

    BootSector bs = {
        .extended_boot_signature = sector_buffer[38],
        .drive_number = sector_buffer[36],
        .volume_id = fatreader_get_dword(&sector_buffer[39]),
        .sign = fat_signature};

    BiosParameterBlock bpb = {
        .bytes_per_sec = fatreader_get_word(&sector_buffer[11]),
        .sectors_per_cluster = sector_buffer[13],
        .reserved_sectors_count = fatreader_get_word(&sector_buffer[14]),
        .num_fats = sector_buffer[16],
        .root_entries_count = fatreader_get_word(&sector_buffer[17]),
        .total_sectors_16 = fatreader_get_word(&sector_buffer[19]),
        .fat_size_16 = fatreader_get_word(&sector_buffer[22]),
        .media = sector_buffer[21]};

    handle->boot_sector = bs;
    handle->bios_parameter_block = bpb;
    handle->image_file_ptr = fp;

    // Calculated Parameters
    handle->fat_sectors = bpb.fat_size_16 * bpb.num_fats;
    handle->root_dir_start_sector = bpb.reserved_sectors_count + handle->fat_sectors;
    handle->root_dir_sectors =
        (32 * bpb.root_entries_count + bpb.bytes_per_sec - 1) / bpb.bytes_per_sec;
    handle->data_start_sector = handle->root_dir_start_sector + handle->root_dir_sectors;
    handle->data_sectors = bpb.total_sectors_16 - handle->data_start_sector;

    handle->fat_type = determine_fat_type(
        handle->data_sectors / handle->bios_parameter_block.sectors_per_cluster);

    free(sector_buffer);

    return FR_OK;
}

FRESULT fatreader_close_image(FHandle* handle) {
    furi_assert(handle);

    if(handle->image_file_ptr) {
        bool ret = storage_file_close(handle->image_file_ptr);
        if(!ret) {
            return FR_CLOSE_FAILED;
        }
        storage_file_free(handle->image_file_ptr);
    }

    return FR_OK;
}

void fatreader_print_info(FHandle* handle) {
    FURI_LOG_D(TAG, "Type: %d\n", handle->fat_type);
    FURI_LOG_D(TAG, "BPB_BytsPerSec: %d\n", handle->bios_parameter_block.bytes_per_sec);
    FURI_LOG_D(TAG, "BPB_SecPerClus: %d\n", handle->bios_parameter_block.sectors_per_cluster);
    FURI_LOG_D(TAG, "BPB_RsvdSecCnt: %d\n", handle->bios_parameter_block.reserved_sectors_count);
    FURI_LOG_D(TAG, "BPB_NumFATs: %d\n", handle->bios_parameter_block.num_fats);
    FURI_LOG_D(TAG, "BPB_RootEntCnt: %d\n", handle->bios_parameter_block.root_entries_count);
    FURI_LOG_D(TAG, "BPB_TotSec16: %d\n", handle->bios_parameter_block.total_sectors_16);
    FURI_LOG_D(TAG, "BPB_Media: %#02x\n", handle->bios_parameter_block.media);
    FURI_LOG_D(TAG, "BPB_FATSz16: %d\n", handle->bios_parameter_block.fat_size_16);

    // FAT16 specific fields
    FURI_LOG_D(TAG, "BS_DrvNum: %#02x\n", handle->boot_sector.drive_number);
    FURI_LOG_D(TAG, "BS_BootSig: %#02x\n", handle->boot_sector.extended_boot_signature);
    FURI_LOG_D(TAG, "BS_VolID: %#8lx\n", handle->boot_sector.volume_id);
    FURI_LOG_D(TAG, "BS_Sign: %#04x\n", handle->boot_sector.sign);

    FURI_LOG_D(TAG, "FatStartSector: %d\n", handle->bios_parameter_block.reserved_sectors_count);
    FURI_LOG_D(TAG, "FatSectors: %ld\n", handle->fat_sectors);
    FURI_LOG_D(TAG, "RootDirStartSector: %ld\n", handle->root_dir_start_sector);
    FURI_LOG_D(TAG, "RootDirSectors: %ld\n", handle->root_dir_sectors);
    FURI_LOG_D(TAG, "DataStartSector: %ld\n", handle->data_start_sector);
    FURI_LOG_D(TAG, "DataSectors: %ld\n", handle->data_sectors);

    FURI_LOG_D(
        TAG,
        "CountOfClusters: %ld\n",
        handle->data_sectors / handle->bios_parameter_block.sectors_per_cluster);
    FURI_LOG_D(TAG, "\n");
}

FRESULT fatreader_root_directory_open(RootDirectory* root_directory, FHandle* handle) {
    furi_assert(root_directory);
    furi_assert(handle);
    size_t ret;
    bool ok = storage_file_seek(
        handle->image_file_ptr,
        handle->root_dir_start_sector * handle->bios_parameter_block.bytes_per_sec,
        true);
    if(!ok) {
        return FR_SEEK_FAILED;
    }
    root_directory->buffer = malloc(BYTES_PER_SECTOR);
    ret = storage_file_read(handle->image_file_ptr, root_directory->buffer, BYTES_PER_SECTOR);
    if(ret != BYTES_PER_SECTOR) {
        free(root_directory->buffer);
        return FR_READ_FAILED;
    }

    root_directory->current_ptr = root_directory->buffer;
    root_directory->handle = handle;

    return FR_OK;
}

FRESULT fatreader_root_directory_close(RootDirectory* root_directory) {
    furi_assert(root_directory);
    free(root_directory->buffer);

    return FR_OK;
}

FRESULT fatreader_root_directory_find_first(DIR* dir, RootDirectory* root_directory) {
    furi_assert(root_directory);
    furi_assert(dir);

    for(; root_directory->processed_entries <
          root_directory->handle->bios_parameter_block.root_entries_count;
        root_directory->current_ptr += 32, root_directory->processed_entries++) {
        if((root_directory->current_ptr - root_directory->buffer) == BYTES_PER_SECTOR) {
            size_t ret;
            bool ok = storage_file_seek(
                root_directory->handle->image_file_ptr,
                root_directory->handle->root_dir_start_sector *
                        root_directory->handle->bios_parameter_block.bytes_per_sec +
                    root_directory->processed_entries * 32,
                true);
            if(!ok) {
                return FR_SEEK_FAILED;
            }
            ret = storage_file_read(
                root_directory->handle->image_file_ptr, root_directory->buffer, BYTES_PER_SECTOR);
            if(ret != BYTES_PER_SECTOR) {
                return FR_READ_FAILED;
            }
            root_directory->current_ptr = root_directory->buffer;
        }

        u8 filename_first_byte = root_directory->current_ptr[0];
        u8 attr = root_directory->current_ptr[11];
        if(IS_END(filename_first_byte)) {
            return FR_NO_FILE_FOUND;
        } else if(IS_FILE_NOT_VALID(filename_first_byte, attr)) {
            continue;
        }
        read_dir_into(dir, root_directory->current_ptr);
        dir->handle = root_directory->handle;
        root_directory->processed_entries++;
        return FR_OK;
    }
    return FR_NO_FILE_FOUND;
}

FRESULT fatreader_root_directory_find_next(DIR* dir, RootDirectory* root_directory) {
    furi_assert(root_directory);
    furi_assert(dir);

    root_directory->current_ptr += 32;

    return fatreader_root_directory_find_first(dir, root_directory);
}

FRESULT fatreader_root_directory_find_by_name(
    const char* name,
    DIR* dir,
    RootDirectory* root_directory) {
    furi_assert(root_directory);
    furi_assert(dir);

    root_directory->current_ptr = root_directory->buffer;
    FRESULT res = fatreader_root_directory_find_first(dir, root_directory);
    if(res != FR_OK) {
        return FR_FILE_NOT_FOUND;
    }
    if(strcmp(dir->name, name) == 0) {
        return FR_OK;
    } else {
        do {
            res = fatreader_root_directory_find_next(dir, root_directory);
        } while(strcmp(dir->name, name) != 0 && res != FR_NO_FILE_FOUND);
        if(res == FR_NO_FILE_FOUND) {
            return FR_FILE_NOT_FOUND;
        } else {
            return FR_OK;
        }
    }
}

FRESULT fatreader_file_open(FIL* file, DIR* dir) {
    file->dir = dir;
    file->read_bytes = 0;
    file->cur_cluster = dir->cluster;
    file->cur_sector =
        dir->handle->data_start_sector +
        (file->cur_cluster - 2) * dir->handle->bios_parameter_block.sectors_per_cluster;
    file->cur_sector_ptr = file->cur_sector * dir->handle->bios_parameter_block.bytes_per_sec;

    return FR_OK;
}

FRESULT fatreader_file_read(u8* buffer, FIL* file, size_t bytes_to_read, size_t* bytes_read) {
    if(file->dir->file_size == 0) {
        return FR_EMPTY_FILE;
    }

    FHandle* fat_info = file->dir->handle;

    u8 next_cluster_buffer[2];

    u32 marker = marker_by_fat_type(fat_info->fat_type);

    *bytes_read = 0;
    size_t ret;
    size_t available_to_read = file->dir->file_size - file->read_bytes;
    size_t size_to_read = bytes_to_read > available_to_read ? available_to_read : bytes_to_read;
    u8* buffer_ptr = buffer;
    do {
        for(u16 i = 0; i < fat_info->bios_parameter_block.sectors_per_cluster;
            ++i,
                file->cur_sector++,
                file->cur_sector_ptr =
                    file->cur_sector * fat_info->bios_parameter_block.bytes_per_sec) {
            bool ok = storage_file_seek(fat_info->image_file_ptr, file->cur_sector_ptr, true);
            if(!ok) {
                return FR_SEEK_FAILED;
            }
            u16 already_read_from_sector =
                file->cur_sector_ptr -
                (file->cur_sector * fat_info->bios_parameter_block.bytes_per_sec);
            u16 left_to_read_from_sector =
                fat_info->bios_parameter_block.bytes_per_sec - already_read_from_sector;
            size_t bytes_to_read_from_sector =
                size_to_read > left_to_read_from_sector ? left_to_read_from_sector : size_to_read;
            ret =
                storage_file_read(fat_info->image_file_ptr, buffer_ptr, bytes_to_read_from_sector);
            if(ret != bytes_to_read_from_sector) {
                return FR_READ_FAILED;
            }
            buffer_ptr += ret;
            size_to_read -= ret;
            *bytes_read += ret;
            file->read_bytes += ret;
            if((file->dir->file_size - file->read_bytes) == 0) {
                break;
            } else {
                if(size_to_read == 0) {
                    file->cur_sector_ptr += ret;
                    return FR_OK;
                }
            }
        }
        // Find next cluster
        if(fat_info->fat_type == FT_12) {
            size_t offset = (fat_info->bios_parameter_block.reserved_sectors_count * 512) +
                            (file->cur_cluster + file->cur_cluster / 2);
            bool ok = storage_file_seek(fat_info->image_file_ptr, offset, true);
            if(!ok) {
                return FR_SEEK_FAILED;
            }
            ret = storage_file_read(fat_info->image_file_ptr, &next_cluster_buffer, 2);
            if(ret != 2) {
                return FR_READ_FAILED;
            }
            if(file->cur_cluster & 1) {
                // Odd
                file->cur_cluster = (next_cluster_buffer[0] >> 4) | (u16)next_cluster_buffer[1]
                                                                        << 4;
            } else {
                // Even
                file->cur_cluster = next_cluster_buffer[0] | (next_cluster_buffer[1] & 0x0f) << 8;
            }
        } else if(fat_info->fat_type == FT_16) {
            size_t offset = (fat_info->bios_parameter_block.reserved_sectors_count * 512) +
                            (file->cur_cluster * 2);
            bool ok = storage_file_seek(fat_info->image_file_ptr, offset, true);
            if(!ok) {
                return FR_SEEK_FAILED;
            }
            ret = storage_file_read(fat_info->image_file_ptr, &file->cur_cluster, 2);
            if(ret != 2) {
                return FR_READ_FAILED;
            }
        }
        file->cur_sector =
            fat_info->data_start_sector +
            (file->cur_cluster - 2) * fat_info->bios_parameter_block.sectors_per_cluster;
        file->cur_sector_ptr = file->cur_sector * fat_info->bios_parameter_block.bytes_per_sec;
    } while(file->cur_cluster < marker);
    return FR_EOF;
}
