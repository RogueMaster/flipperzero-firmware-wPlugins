#include "combo_runtime.h"
#include "combo_fat16.h"
#include <furi.h>
#include <furi_hal.h>

#define TAG "BadUsbMsc"

struct ComboRuntime {
    Storage* storage;
    File* image_file;
    MassStorageUsb* usb;
    FuriMutex* mutex;
};

static bool scsi_read(
    void* context,
    uint32_t lba,
    uint16_t count,
    uint8_t* out,
    uint32_t* out_len,
    uint32_t cap) {
    ComboRuntime* rt = context;
    uint32_t bytes = MIN((uint32_t)count * SCSI_BLOCK_SIZE, cap);
    if(lba >= 16384 || bytes > (16384u - lba) * SCSI_BLOCK_SIZE) return false;
    furi_mutex_acquire(rt->mutex, FuriWaitForever);
    bool ok = storage_file_seek(rt->image_file, lba * SCSI_BLOCK_SIZE, true);
    *out_len = ok ? storage_file_read(rt->image_file, out, bytes) : 0;
    ok = ok && *out_len == bytes;
    furi_mutex_release(rt->mutex);
    return ok;
}

static bool scsi_write(void* context, uint32_t lba, uint16_t count, uint8_t* buf, uint32_t len) {
    ComboRuntime* rt = context;
    if(lba >= 16384 || len != (uint32_t)count * SCSI_BLOCK_SIZE ||
       len > (16384u - lba) * SCSI_BLOCK_SIZE)
        return false;
    furi_mutex_acquire(rt->mutex, FuriWaitForever);
    bool ok = storage_file_seek(rt->image_file, lba * SCSI_BLOCK_SIZE, true) &&
              storage_file_write(rt->image_file, buf, len) == len;
    furi_mutex_release(rt->mutex);
    return ok;
}

static uint32_t scsi_num_blocks(void* ctx) {
    UNUSED(ctx);
    return 16384;
}

static void scsi_eject(void* ctx) {
    // SCSI eject is advisory: the UI stays until the script screen is closed.
    ComboRuntime* rt = ctx;
    UNUSED(rt);
    FURI_LOG_I(TAG, "PC requested safe removal");
}

ComboRuntime* combo_runtime_start(const FuriHalUsbHidConfig* usb_cfg) {
    ComboRuntime* rt = malloc(sizeof(ComboRuntime));
    rt->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(rt->storage, COMBO_FOLDER);
    storage_common_mkdir(rt->storage, COMBO_FILES_FOLDER);

    // The image must already have been prepared by the startup worker.
    // Never create 8 MiB synchronously when a script is launched.
    if(!combo_fat16_image_valid(rt->storage, COMBO_IMAGE_PATH) ||
       !combo_fat16_import(rt->storage, COMBO_IMAGE_PATH, COMBO_FILES_FOLDER)) {
        FURI_LOG_E(TAG, "Cannot prepare BADUSBMSC image in %s", COMBO_FOLDER);
        furi_record_close(RECORD_STORAGE);
        free(rt);
        return NULL;
    }

    rt->image_file = storage_file_alloc(rt->storage);
    if(!storage_file_open(
           rt->image_file, COMBO_IMAGE_PATH, FSAM_READ | FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        storage_file_free(rt->image_file);
        furi_record_close(RECORD_STORAGE);
        free(rt);
        return NULL;
    }
    rt->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    SCSIDeviceFunc fn = {
        .ctx = rt,
        .read = scsi_read,
        .write = scsi_write,
        .num_blocks = scsi_num_blocks,
        .eject = scsi_eject,
    };
    rt->usb = mass_storage_usb_start("BADUSBMSC", fn, usb_cfg);
    if(!rt->usb) {
        furi_mutex_free(rt->mutex);
        storage_file_free(rt->image_file);
        furi_record_close(RECORD_STORAGE);
        free(rt);
        return NULL;
    }
    return rt;
}

void combo_runtime_stop(ComboRuntime* rt) {
    if(!rt) return;
    // Host should eject volume or unplug first; never sync while MSC is live.
    if(rt->usb) mass_storage_usb_stop(rt->usb);
    if(rt->image_file) {
        storage_file_sync(rt->image_file);
        storage_file_close(rt->image_file);
        storage_file_free(rt->image_file);
    }
    if(rt->mutex) furi_mutex_free(rt->mutex);
    if(!combo_fat16_export(rt->storage, COMBO_IMAGE_PATH, COMBO_FILES_FOLDER)) {
        FURI_LOG_E(TAG, "Sync failed; PC data remains safely in disk image");
    }
    furi_record_close(RECORD_STORAGE);
    free(rt);
}
