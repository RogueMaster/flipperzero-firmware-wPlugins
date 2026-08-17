#pragma once

#include <storage/storage.h>
#include "mass_storage_scsi.h"

typedef struct MassStorageUsb MassStorageUsb;
typedef void (*MassStorageUsbConnectionStatusCallback)(bool connected, void* context);

MassStorageUsb* mass_storage_usb_start(const char* filename, SCSIDeviceFunc fn);
void mass_storage_usb_stop(MassStorageUsb* mass);
void mass_storage_usb_set_connection_status_callback(
    MassStorageUsb* mass,
    MassStorageUsbConnectionStatusCallback cb,
    void* context);
