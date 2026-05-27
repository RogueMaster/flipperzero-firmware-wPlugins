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
  ViewDispatcher *view_dispatcher;
  SceneManager *scene_manager;
  struct NfcStockUi *ui;
  char active_db_path[128];
  StockItem current_item;
  bool item_is_new;
  /** True while TextInput is shown from Edit; next Back returns to Edit list.
   */
  bool edit_text_input_blocking_back;
} NfcStockApp;
