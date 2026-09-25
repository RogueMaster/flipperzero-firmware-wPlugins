#pragma once

#include "stock.h"
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

struct NfcStockUi;

typedef enum {
    NfcStockSceneMenu,
    NfcStockSceneScan,
    NfcStockSceneDetails,
    NfcStockSceneEdit,
    NfcStockSceneInventory,
    NfcStockSceneSettings,
    NfcStockSceneCount,
} NfcStockScene;

typedef enum {
    NfcStockEventScanKnown = 100,
    NfcStockEventScanUnknown,
    NfcStockEventDetailsStockUp = 110,
    NfcStockEventDetailsStockDown,
    NfcStockEventDetailsOpenEdit,
} NfcStockCustomEvent;

typedef struct {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    struct NfcStockUi* ui;
    char active_db_path[128];
    StockItem current_item;
    bool item_is_new;
    /** True once Up/Down actually changed current_item in Details; viewing
   * without changing anything must not write. */
    bool current_item_dirty;
    /** True once a text field was actually edited in Edit; an existing item
   * backed out of untouched must not write either. */
    bool edit_dirty;
    /** True right after Details navigates into Edit via OK; tells the next
   * Details on_enter this is a return from that detour, not a fresh
   * scan/select, so a pending current_item_dirty must survive it instead of
   * being reset and silently lost. */
    bool edit_opened_from_details;
    /** True while a save/quarantine notice is shown from Details or Edit; the
   * next Back dismisses it and lets the normal scene pop proceed. */
    bool write_notice_shown;
    /** True while TextInput is shown from Edit; next Back returns to Edit list.
   */
    bool edit_text_input_blocking_back;
} NfcStockApp;
