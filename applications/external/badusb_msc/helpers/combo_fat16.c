#include "combo_fat16.h"
#include <furi.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TAG               "ComboFAT16"
#define COMBO_IMAGE_SIZE  COMBO_FAT16_IMAGE_SIZE
#define COMBO_IMAGE_LABEL "BADUSBMSC  "
#define SECTOR            512u
#define FAT_START         (1u * SECTOR)
#define FAT_SECTORS       32u
#define FAT_SIZE          (FAT_SECTORS * SECTOR)
#define ROOT_START        ((1u + FAT_SECTORS * 2u) * SECTOR)
#define ROOT_SIZE         (32u * SECTOR)
#define DATA_START        ((1u + FAT_SECTORS * 2u + 32u) * SECTOR)
#define CLUSTER_SIZE      (2u * SECTOR)
#define MAX_CLUSTER       8144u
#define MAX_ROOT_FILES    512u

typedef struct {
    File* disk;
    uint8_t* fat;
    uint8_t* root;
} ComboFat;

static uint16_t get16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t get32(const uint8_t* p) {
    return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16);
}
static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t* p, uint32_t v) {
    put16(p, (uint16_t)v);
    put16(p + 2, (uint16_t)(v >> 16));
}
static bool disk_read(File* disk, uint32_t offset, void* buf, size_t size) {
    return storage_file_seek(disk, offset, true) && storage_file_read(disk, buf, size) == size;
}
static bool disk_write(File* disk, uint32_t offset, const void* buf, size_t size) {
    return storage_file_seek(disk, offset, true) && storage_file_write(disk, buf, size) == size;
}
// Validate an image before it can be exposed as a USB disk. A nonempty,
// invalid image is never overwritten: it may contain data worth recovering.
static bool image_header_valid(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    bool ok = opened;
    uint8_t boot[512];
    uint8_t fat1[4], fat2[4], label[12];
    if(ok) {
        ok = storage_file_size(file) == COMBO_IMAGE_SIZE &&
             disk_read(file, 0, boot, sizeof(boot)) &&
             disk_read(file, FAT_START, fat1, sizeof(fat1)) &&
             disk_read(file, FAT_START + FAT_SIZE, fat2, sizeof(fat2)) &&
             disk_read(file, ROOT_START, label, sizeof(label));
        if(ok) {
            ok = boot[510] == 0x55 && boot[511] == 0xAA && get16(boot + 11) == 512 &&
                 boot[13] == 2 && get16(boot + 14) == 1 && boot[16] == 2 &&
                 get16(boot + 17) == 512 && get16(boot + 19) == 16384 && boot[21] == 0xF8 &&
                 get16(boot + 22) == 32 && !memcmp(boot + 43, COMBO_IMAGE_LABEL, 11) &&
                 !memcmp(boot + 54, "FAT16   ", 8) && !memcmp(fat1, "\xF8\xFF\xFF\xFF", 4) &&
                 !memcmp(fat1, fat2, sizeof(fat1)) && !memcmp(label, COMBO_IMAGE_LABEL, 11) &&
                 label[11] == 0x08;
        }
    }
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool combo_fat16_image_valid(Storage* storage, const char* image) {
    return image_header_valid(storage, image);
}

static bool combo_fat16_create_new(
    Storage* storage,
    const char* image,
    ComboFat16ProgressCallback callback,
    void* context) {
    File* disk = storage_file_alloc(storage);
    bool opened = storage_file_open(disk, image, FSAM_WRITE, FSOM_CREATE_NEW);
    bool ok = opened;
    uint8_t* block = opened ? malloc(4096) : NULL;
    if(!block) ok = false;
    if(ok) {
        if(callback) callback(0, context);
        for(uint32_t i = 0; i < (COMBO_IMAGE_SIZE / 4096u); i++) {
            memset(block, 0, 4096);
            if(i == 0) {
                block[0] = 0xEB;
                block[1] = 0x3C;
                block[2] = 0x90;
                memcpy(block + 3, "MSDOS5.0", 8);
                put16(block + 11, 512);
                block[13] = 2;
                put16(block + 14, 1);
                block[16] = 2;
                put16(block + 17, 512);
                put16(block + 19, 16384);
                block[21] = 0xF8;
                put16(block + 22, 32);
                put16(block + 24, 32);
                put16(block + 26, 64);
                block[36] = 0x80;
                block[38] = 0x29;
                put32(block + 39, 0xB170CAFE);
                memcpy(block + 43, COMBO_IMAGE_LABEL, 11);
                memcpy(block + 54, "FAT16   ", 8);
                block[510] = 0x55;
                block[511] = 0xAA;
                block[512] = 0xF8;
                block[513] = 0xFF;
                block[514] = 0xFF;
                block[515] = 0xFF;
            } else if(i == 4) {
                // Second FAT starts at sector 33.
                block[512] = 0xF8;
                block[513] = 0xFF;
                block[514] = 0xFF;
                block[515] = 0xFF;
            } else if(i == 8) {
                // FAT root directory starts at sector 65.
                memcpy(block + 512, COMBO_IMAGE_LABEL, 11);
                block[512 + 11] = 0x08;
            }
            if(storage_file_write(disk, block, 4096) != 4096) {
                ok = false;
                break;
            }
            // Percentage based on actual bytes written (not a fake timer).
            if(callback && ((i & 15u) == 15u || i + 1 == COMBO_IMAGE_SIZE / 4096u)) {
                callback((uint8_t)(((i + 1u) * 100u) / (COMBO_IMAGE_SIZE / 4096u)), context);
            }
            // Keep the UI responsive while the SD card is being initialized.
            if((i & 31u) == 31u) furi_delay_tick(1);
        }
        if(ok) ok = storage_file_sync(disk);
    }
    free(block);
    if(opened) storage_file_close(disk);
    storage_file_free(disk);
    if(!ok) {
        // This function exclusively owns a newly-created file. Never delete
        // an image which existed before the call.
        if(opened) storage_common_remove(storage, image);
        return false;
    }
    bool valid = image_header_valid(storage, image);
    if(!valid) storage_common_remove(storage, image);
    return valid;
}

ComboFat16PrepareResult combo_fat16_prepare_with_progress(
    Storage* storage,
    const char* image,
    ComboFat16ProgressCallback callback,
    void* context) {
    FileInfo info;
    FS_Error stat = storage_common_stat(storage, image, &info);
    if(stat == FSE_OK) {
        if(file_info_is_dir(&info)) return ComboFat16PrepareInvalidImage;
        if(info.size == 0) {
            // A zero-byte placeholder cannot contain user data. It is safe
            // to remove, unlike a nonzero but damaged disk image.
            if(storage_common_remove(storage, image) != FSE_OK) return ComboFat16PrepareIoError;
        } else {
            return image_header_valid(storage, image) ? ComboFat16PrepareReady :
                                                        ComboFat16PrepareInvalidImage;
        }
    } else if(storage_file_exists(storage, image)) {
        return ComboFat16PrepareIoError;
    }
    return combo_fat16_create_new(storage, image, callback, context) ? ComboFat16PrepareReady :
                                                                       ComboFat16PrepareIoError;
}

ComboFat16PrepareResult combo_fat16_prepare(Storage* storage, const char* image) {
    return combo_fat16_prepare_with_progress(storage, image, NULL, NULL);
}

static bool fat_open(ComboFat* fat, Storage* storage, const char* path) {
    memset(fat, 0, sizeof(*fat));
    // Validate before opening for writing. Avoid opening the same SD file twice.
    if(!image_header_valid(storage, path)) return false;
    fat->disk = storage_file_alloc(storage);
    fat->fat = malloc(FAT_SIZE);
    fat->root = malloc(ROOT_SIZE);
    if(!fat->fat || !fat->root ||
       !storage_file_open(fat->disk, path, FSAM_READ | FSAM_WRITE, FSOM_OPEN_EXISTING))
        return false;
    return disk_read(fat->disk, FAT_START, fat->fat, FAT_SIZE) &&
           disk_read(fat->disk, ROOT_START, fat->root, ROOT_SIZE);
}
static void fat_close(ComboFat* fat) {
    if(fat->disk) storage_file_free(fat->disk);
    free(fat->fat);
    free(fat->root);
}
static bool fat_commit(ComboFat* fat) {
    return disk_write(fat->disk, FAT_START, fat->fat, FAT_SIZE) &&
           disk_write(fat->disk, FAT_START + FAT_SIZE, fat->fat, FAT_SIZE) &&
           disk_write(fat->disk, ROOT_START, fat->root, ROOT_SIZE) && storage_file_sync(fat->disk);
}
static uint16_t fat_get(ComboFat* fat, uint16_t n) {
    return get16(fat->fat + ((uint32_t)n * 2u));
}
static void fat_set(ComboFat* fat, uint16_t n, uint16_t v) {
    put16(fat->fat + ((uint32_t)n * 2u), v);
}
static bool short_name(const char* src, uint8_t dst[11]) {
    memset(dst, ' ', 11);
    if(!src || !*src || src[0] == '.') return false;
    size_t base = 0, ext = 0;
    bool after_dot = false;
    for(const unsigned char* p = (const unsigned char*)src; *p; p++) {
        if(*p == '.') {
            if(after_dot || !base) return false;
            after_dot = true;
            continue;
        }
        if(!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
             *p == '_' || *p == '-'))
            return false;
        if(!after_dot) {
            if(base >= 8) return false;
            dst[base++] = toupper(*p);
        } else {
            if(ext >= 3) return false;
            dst[8 + ext++] = toupper(*p);
        }
    }
    return base != 0;
}
static void long_name(const uint8_t* ent, char out[13]) {
    size_t j = 0;
    for(size_t i = 0; i < 8 && ent[i] != ' '; i++)
        out[j++] = tolower(ent[i]);
    if(ent[8] != ' ') {
        out[j++] = '.';
        for(size_t i = 8; i < 11 && ent[i] != ' '; i++)
            out[j++] = tolower(ent[i]);
    }
    out[j] = '\0';
}
static bool is_file(const uint8_t* ent) {
    return ent[0] != 0 && ent[0] != 0xE5 && ent[11] != 0x0F && !(ent[11] & (0x08 | 0x10));
}
static void fat_free_chain(ComboFat* fat, uint16_t cluster) {
    for(size_t i = 0; i < MAX_CLUSTER && cluster >= 2 && cluster < MAX_CLUSTER; i++) {
        uint16_t next = fat_get(fat, cluster);
        fat_set(fat, cluster, 0);
        cluster = next;
    }
}
static uint16_t fat_alloc_cluster(ComboFat* fat) {
    for(uint16_t n = 2; n < MAX_CLUSTER; n++) {
        if(fat_get(fat, n) == 0) {
            fat_set(fat, n, 0xFFFF);
            return n;
        }
    }
    return 0;
}
static bool import_one(
    ComboFat* fat,
    Storage* storage,
    const char* folder,
    const char* filename,
    uint32_t size) {
    uint8_t name[11];
    if(!short_name(filename, name)) return true; // Unsupported names remain on SD.
    if(size > 6u * 1024u * 1024u) return true;
    int32_t idx = -1, free_idx = -1;
    for(uint16_t i = 0; i < MAX_ROOT_FILES; i++) {
        uint8_t* ent = fat->root + i * 32;
        if(ent[0] == 0 || ent[0] == 0xE5) {
            if(free_idx < 0) free_idx = i;
            continue;
        }
        if(is_file(ent) && !memcmp(ent, name, sizeof(name))) {
            idx = i;
            break;
        }
    }
    if(idx < 0) idx = free_idx;
    if(idx < 0) return false;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", folder, filename);
    File* src = storage_file_alloc(storage);
    if(!storage_file_open(src, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(src);
        return false;
    }

    uint8_t* ent = fat->root + (uint32_t)idx * 32u;
    if(is_file(ent)) fat_free_chain(fat, get16(ent + 26));
    memset(ent, 0, 32);
    memcpy(ent, name, sizeof(name));
    ent[11] = 0x20;
    put32(ent + 28, size);
    uint32_t remaining = size;
    uint16_t first = 0, prev = 0;
    bool ok = true;
    uint8_t buf[CLUSTER_SIZE];
    while(remaining) {
        uint16_t cluster = fat_alloc_cluster(fat);
        if(!cluster) {
            ok = false;
            break;
        }
        if(!first) first = cluster;
        if(prev) fat_set(fat, prev, cluster);
        prev = cluster;
        size_t n = MIN(remaining, CLUSTER_SIZE);
        memset(buf, 0, sizeof(buf));
        if(storage_file_read(src, buf, n) != n ||
           !disk_write(
               fat->disk, DATA_START + (uint32_t)(cluster - 2) * CLUSTER_SIZE, buf, sizeof(buf))) {
            ok = false;
            break;
        }
        remaining -= n;
    }
    storage_file_free(src);
    if(!ok) {
        fat_free_chain(fat, first);
        ent[0] = 0xE5;
        return false;
    }
    put16(ent + 26, first);
    return true;
}

bool combo_fat16_import(Storage* storage, const char* image, const char* folder) {
    ComboFat fat;
    if(!fat_open(&fat, storage, image)) {
        fat_close(&fat);
        FURI_LOG_E(TAG, "Invalid FAT16 disk image");
        return false;
    }
    // Mirror files removed from the qFlipper staging folder back to the image.
    // The staging folder is authoritative when the USB disk is not mounted.
    for(uint16_t i = 0; i < MAX_ROOT_FILES; i++) {
        uint8_t* ent = fat.root + (uint32_t)i * 32u;
        if(!is_file(ent)) continue;
        char short_file[13], full_path[256];
        long_name(ent, short_file);
        snprintf(full_path, sizeof(full_path), "%s/%s", folder, short_file);
        if(!storage_file_exists(storage, full_path)) {
            fat_free_chain(&fat, get16(ent + 26));
            ent[0] = 0xE5;
        }
    }
    File* dir = storage_file_alloc(storage);
    FileInfo info;
    char filename[128];
    bool result = storage_dir_open(dir, folder);
    if(result) {
        while(storage_dir_read(dir, &info, filename, sizeof(filename))) {
            if(!file_info_is_dir(&info) &&
               !import_one(&fat, storage, folder, filename, info.size)) {
                result = false;
                break;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    if(result) result = fat_commit(&fat);
    fat_close(&fat);
    return result;
}

bool combo_fat16_export(Storage* storage, const char* image, const char* folder) {
    ComboFat fat;
    if(!fat_open(&fat, storage, image)) {
        fat_close(&fat);
        return false;
    }
    bool result = true;
    // Mirror deletions made in Windows back to the qFlipper staging directory.
    // Only 8.3-compatible names are touched: any other SD files are preserved.
    File* directory = storage_file_alloc(storage);
    FileInfo info;
    char existing[128];
    if(storage_dir_open(directory, folder)) {
        while(storage_dir_read(directory, &info, existing, sizeof(existing))) {
            uint8_t short_filename[11];
            if(file_info_is_dir(&info) || !short_name(existing, short_filename)) continue;
            bool present = false;
            for(uint16_t i = 0; i < MAX_ROOT_FILES; i++) {
                uint8_t* entry = fat.root + (uint32_t)i * 32u;
                if(is_file(entry) && !memcmp(entry, short_filename, 11)) {
                    present = true;
                    break;
                }
            }
            if(!present) {
                char stale[256];
                snprintf(stale, sizeof(stale), "%s/%s", folder, existing);
                if(storage_common_remove(storage, stale) != FSE_OK) result = false;
            }
        }
        storage_dir_close(directory);
    }
    storage_file_free(directory);
    if(!result) {
        fat_close(&fat);
        return false;
    }
    uint8_t buf[CLUSTER_SIZE];
    for(uint16_t i = 0; i < MAX_ROOT_FILES; i++) {
        uint8_t* ent = fat.root + (uint32_t)i * 32u;
        if(!is_file(ent)) continue;
        char name[13], dest[256], temp[256];
        long_name(ent, name);
        snprintf(dest, sizeof(dest), "%s/%s", folder, name);
        snprintf(temp, sizeof(temp), "%s/.__combo_tmp", folder);
        File* dst = storage_file_alloc(storage);
        if(!storage_file_open(dst, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_free(dst);
            result = false;
            break;
        }
        uint32_t remaining = get32(ent + 28);
        uint16_t cluster = get16(ent + 26);
        uint16_t steps = 0;
        bool ok = true;
        while(remaining && steps++ < MAX_CLUSTER) {
            if(cluster < 2 || cluster >= MAX_CLUSTER) {
                ok = false;
                break;
            }
            size_t n = MIN(remaining, CLUSTER_SIZE);
            if(!disk_read(
                   fat.disk,
                   DATA_START + (uint32_t)(cluster - 2) * CLUSTER_SIZE,
                   buf,
                   sizeof(buf)) ||
               storage_file_write(dst, buf, n) != n) {
                ok = false;
                break;
            }
            remaining -= n;
            cluster = fat_get(&fat, cluster);
        }
        if(remaining) ok = false;
        if(ok) ok = storage_file_sync(dst);
        storage_file_close(dst);
        storage_file_free(dst);
        if(ok) {
            // The temporary file preserves the old destination if transfer fails.
            storage_common_remove(storage, dest);
            ok = storage_common_rename(storage, temp, dest) == FSE_OK;
        }
        if(!ok) {
            result = false;
            break;
        }
    }
    fat_close(&fat);
    return result;
}
