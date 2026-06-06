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

#define SUBHOUND_TAG "Subhound"

typedef enum {
    SubhoundViewLoading,   /* spinner during parse + features + classify */
    SubhoundViewSummary,   /* Widget: overview card (landing view) */
    SubhoundViewSections,  /* Submenu: drill-down chooser */
    SubhoundViewTextBox,   /* per-section detail OR full report */
} SubhoundView;

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
} SubhoundApp;
