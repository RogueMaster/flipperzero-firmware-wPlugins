#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "analyzer/types.h"

#define BITRAW_TAG "BitrawAnalyzer"

typedef enum {
    BitrawSceneBrowser,
    BitrawSceneAnalyzing,
    BitrawSceneResult,
    BitrawSceneCount,
} BitrawScene;

typedef enum {
    BitrawViewTextBox,
    BitrawViewPopup,
} BitrawView;

typedef enum {
    BitrawCustomEventAnalyzeDone = 0x100,
    BitrawCustomEventAnalyzeFailed,
    BitrawCustomEventSaveOk,
    BitrawCustomEventSaveFailed,
} BitrawCustomEvent;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    TextBox* text_box;
    Popup* popup;

    FuriString* selected_path;
    FuriString* report;
    FuriString* parse_error;

    SubFile sub;
    FeatureVector fv;
    ClassificationResult result;
} BitrawApp;
