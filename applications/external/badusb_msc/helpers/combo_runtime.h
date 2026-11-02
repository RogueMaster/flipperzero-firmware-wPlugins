#pragma once
#include <storage/storage.h>
#include "mass_storage_usb.h"
#include <furi_hal_usb_hid.h>

#define COMBO_FOLDER       EXT_PATH("apps_data/badusb_msc")
#define COMBO_FILES_FOLDER COMBO_FOLDER "/files"
#define COMBO_IMAGE_PATH   COMBO_FOLDER "/drive.img"

typedef struct ComboRuntime ComboRuntime;
ComboRuntime* combo_runtime_start(const FuriHalUsbHidConfig* usb_cfg);
void combo_runtime_stop(ComboRuntime* runtime);
