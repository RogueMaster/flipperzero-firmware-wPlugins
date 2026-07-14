#pragma once
// #ifndef NFC_COMPARATOR_H
// #define NFC_COMPARATOR_H

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <nfc_device.h>
#include <nfc_listener.h>
#include <nfc_scanner.h>
#include <storage/storage.h>
#include <dir_walk.h>
#include <path.h>
#include <dolphin/dolphin.h>
#include <simple_array.h>

#include "nfc_comparator_icons.h"
#include "scenes/nfc_comparator_scene.h"
#include "lib/compare_reader_worker/nfc_comparator_compare_reader_worker.h"
#include "lib/finder_reader_worker/nfc_comparator_finder_reader_worker.h"
#include "lib/compare_worker/nfc_comparator_compare_worker.h"
#include "lib/finder_searcher_worker/nfc_comparator_finder_searcher_worker.h"
#include "lib/led_worker/nfc_comparator_led_worker.h"

#define NFC_ITEM_LOCATION "/ext/nfc/"

/** Enum for all possible views in the NFC Comparator app */
typedef enum {
    NfcComparatorView_Submenu,
    NfcComparatorView_FileBrowser,
    NfcComparatorView_Popup,
    NfcComparatorView_Widget,
    NfcComparatorView_Loading,
    NfcComparatorView_VariableItemList,
    NfcComparatorView_TextBox,
    NfcComparatorView_Count
} NfcComparatorViews;

/** Main app struct holding all state */
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notification_app;

    struct {
        Submenu* submenu;
        struct {
            FileBrowser* view;
            FuriString* output;
            FuriString* tmp_output;
        } file_browser;
        Popup* popup;
        Widget* widget;
        Loading* loading;
        VariableItemList* variable_item_list;
        struct {
            TextBox* view;
            FuriString* store;
        } text_box;
    } views;

    struct {
        struct {
            NfcComparatorFinderSearcherWorker* worker;
            NfcComparatorFinderSearcherWorkerSettings settings;
        } searcher;
        NfcComparatorFinderReaderWorker* finder;
        NfcComparatorCompareReaderWorker* reader;
        NfcComparatorCompareWorker* compare;
    } workers;
} NfcComparator;

// #endif // NFC_COMPARATOR_H
