#pragma once

#include "logic.h"
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

#define SCAN_DIR      "/ext/subghz"
#define FULL_PATH_LEN 280

typedef enum {
    SubDupFinderViewSubmenu,
    SubDupFinderViewGroups,
    SubDupFinderViewFiles,
    SubDupFinderViewConfirm,
    SubDupFinderViewCredits,
    SubDupFinderViewPopup,
} SubDupFinderView;

typedef enum {
    SubDupFinderSubmenuIndexScan,
    SubDupFinderSubmenuIndexCredits,
} SubDupFinderSubmenuIndex;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* main_submenu;
    Submenu* groups_submenu;
    Submenu* files_in_group_submenu;
    DialogEx* confirm_dialog;
    Widget* credits_widget;
    Popup* popup;
    HashDatabase db;
    char selected_path[APP_MAX_PATH_LEN];
    size_t selected_group_index;
} SubDupFinderApp;
