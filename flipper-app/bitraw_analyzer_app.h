#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "analyzer/types.h"

#define BITRAW_TAG "BitRaw"

typedef enum {
    BitrawViewTextBox,
} BitrawView;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;

    ViewDispatcher* view_dispatcher;
    TextBox* text_box;

    FuriString* selected_path;
    FuriString* report;
    FuriString* parse_error;

    SubFile sub;
    FeatureVector fv;
    ClassificationResult result;
} BitrawApp;
