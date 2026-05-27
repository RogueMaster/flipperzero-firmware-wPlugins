#pragma once
#include "app_types.h"
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef enum {
  NfcStockViewSubmenu,
  NfcStockViewWidget,
  NfcStockViewTextInput,
  NfcStockViewInventory,
  NfcStockViewEdit,
} NfcStockView;

typedef struct NfcStockUi {
  Submenu *submenu;
  Widget *widget;
  TextInput *text_input;
  VariableItemList *inventory_list;
  VariableItemList *edit_list;
} NfcStockUi;

NfcStockUi *nfc_stock_ui_alloc(void);
void nfc_stock_ui_free(NfcStockUi *ui);
void nfc_stock_ui_configure(NfcStockUi *ui, ViewDispatcher *view_dispatcher,
                            SceneManager *scene_manager);

/** Rebuild main menu items and show active DB file (submenu header). */
void nfc_stock_ui_apply_main_menu(NfcStockUi *ui, SceneManager *scene_manager,
                                  const char *active_db_path);

void nfc_stock_ui_show_item(NfcStockUi *ui, const StockItem *item);
