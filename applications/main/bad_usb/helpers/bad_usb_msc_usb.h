#pragma once

#include <storage/storage.h>
#include "bad_usb_msc_scsi.h"

typedef struct MassStorageUsb MassStorageUsb;

MassStorageUsb* mass_storage_usb_start(
    const char* filename,
    SCSIDeviceFunc fn,
    const MassStorageConfig* config);
void mass_storage_usb_stop(MassStorageUsb* mass);
