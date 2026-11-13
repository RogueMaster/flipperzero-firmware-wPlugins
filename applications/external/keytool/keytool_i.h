#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#define KEYTOOL_APP_FOLDER "/ext/apps_data/keytool"
#define KEYTOOL_DEFAULT_DUMP_FOLDER "/ext/nfc"

typedef enum {
    KeyToolViewSubmenu,
    KeyToolViewLogList,
    KeyToolViewCacheList,
    KeyToolViewLogDetail,
    KeyToolViewAbout,
    KeyToolViewLoading,
} KeyToolView;

typedef enum {
    KeyToolSubmenuIndexStart,
    KeyToolSubmenuIndexLog,
    KeyToolSubmenuIndexCache,
    KeyToolSubmenuIndexAbout,
} KeyToolSubmenuIndex;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* log_list_view;
    View* cache_view;
    TextBox* text_box;
    Widget* widget_about;
    Loading* loading;
    DialogsApp* dialogs;
    Storage* storage;

    FuriString* dump_file_path;
    FuriString* log_text;

    // Where the BACK button on the log-detail TextBox should go:
    // KeyToolViewSubmenu right after a scan, KeyToolViewLogList when
    // opened from the saved-logs list.
    KeyToolView text_box_return_view;
} KeyToolApp;
