// GPL-3.0. Derived from
// flipperdevices/flipperzero-good-faps/mass_storage/helpers. Modified by Roger
// F5, 2026-11-10: bounded buffers, validation, read-only support, product
// identity.
#pragma once

#include "mass_storage_scsi.h"
#include <storage/storage.h>

typedef struct MassStorageUsb MassStorageUsb;

MassStorageUsb* mass_storage_usb_start(const char* filename, SCSIDeviceFunc fn);
bool mass_storage_usb_stop(MassStorageUsb* mass);
