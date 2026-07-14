#pragma once

#include "modules/loading_cancellable.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
// #include <gui/modules/loading.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/file_browser.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

#define TAG "FlipAIC"

typedef enum {
    FlipAICViewSubmenu,
    FlipAICViewLoading,
    FlipAICViewDialogEx,
    FlipAICViewFileBrowser,
} FlipAICView;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Gui* gui;
    Submenu* submenu;
    LoadingCancellable* loading;
    DialogEx* dialog_ex;
    FileBrowser* file_browser;
    FuriString* file_browser_result_path;

    Nfc* nfc;
    NfcPoller* nfc_poller;
    NfcDevice* nfc_device;
    // NfcListener* nfc_listener;

    FuriHalUsbInterface* usb_mode_prev;
} FlipAIC;
