#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/loading.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "analyzer/types.h"

#define BITRAW_TAG "BitRaw"

typedef enum {
    BitrawViewLoading,   /* spinner during parse + features + classify */
    BitrawViewSummary,   /* Widget: overview card (landing view) */
    BitrawViewSections,  /* Submenu: drill-down chooser */
    BitrawViewTextBox,   /* per-section detail OR full report */
} BitrawView;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;

    ViewDispatcher* view_dispatcher;
    TextBox* text_box;
    Widget* summary;
    Submenu* sections;
    Loading* loading;

    FuriString* selected_path;
    FuriString* report;          /* full report - written to .report.txt sidecar */
    FuriString* section_text;    /* scratch for per-section TextBox content */
    FuriString* parse_error;

    SubFile sub;
    FeatureVector fv;
    ClassificationResult result;
} BitrawApp;
