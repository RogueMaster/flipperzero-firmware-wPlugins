#pragma once
#include <storage/storage.h>

// The isolated disk is a FAT16 superfloppy, 8 MiB, with 8.3 root filenames.
// A non-empty invalid image must never be overwritten automatically.
typedef enum {
    ComboFat16PrepareReady = 0,
    ComboFat16PrepareInvalidImage = 1,
    ComboFat16PrepareIoError = 2,
} ComboFat16PrepareResult;

#define COMBO_FAT16_IMAGE_SIZE 8388608u

// Creates drive.img only when missing (or when it is exactly zero bytes).
// Validates the boot sector, FAT tables, volume label and exact file size.
typedef void (*ComboFat16ProgressCallback)(uint8_t percent, void* context);
ComboFat16PrepareResult combo_fat16_prepare_with_progress(
    Storage* storage,
    const char* image,
    ComboFat16ProgressCallback callback,
    void* context);
ComboFat16PrepareResult combo_fat16_prepare(Storage* storage, const char* image);
bool combo_fat16_image_valid(Storage* storage, const char* image);

bool combo_fat16_import(Storage* storage, const char* image, const char* folder);
bool combo_fat16_export(Storage* storage, const char* image, const char* folder);
