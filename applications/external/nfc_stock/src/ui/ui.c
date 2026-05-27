#include "include/ui.h"
#include <gui/canvas.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nfc_stock_ui_submenu_callback(void *context, uint32_t index) {
  SceneManager *scene_manager = (SceneManager *)context;
  scene_manager_next_scene(scene_manager, (NfcStockScene)index);
}

NfcStockUi *nfc_stock_ui_alloc(void) {
  NfcStockUi *ui = malloc(sizeof(NfcStockUi));
  ui->submenu = submenu_alloc();
  ui->widget = widget_alloc();
  ui->text_input = text_input_alloc();
  ui->inventory_list = variable_item_list_alloc();
  ui->edit_list = variable_item_list_alloc();
  return ui;
}

void nfc_stock_ui_free(NfcStockUi *ui) {
  submenu_free(ui->submenu);
  widget_free(ui->widget);
  text_input_free(ui->text_input);
  variable_item_list_free(ui->inventory_list);
  variable_item_list_free(ui->edit_list);
  free(ui);
}

void nfc_stock_ui_configure(NfcStockUi *ui, ViewDispatcher *view_dispatcher,
                            SceneManager *scene_manager) {
  (void)scene_manager;

  view_dispatcher_add_view(view_dispatcher, NfcStockViewSubmenu,
                           submenu_get_view(ui->submenu));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewWidget,
                           widget_get_view(ui->widget));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewTextInput,
                           text_input_get_view(ui->text_input));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewInventory,
                           variable_item_list_get_view(ui->inventory_list));
  view_dispatcher_add_view(view_dispatcher, NfcStockViewEdit,
                           variable_item_list_get_view(ui->edit_list));
}

void nfc_stock_ui_apply_main_menu(NfcStockUi *ui, SceneManager *scene_manager,
                                  const char *active_db_path) {
  const char *slash = strrchr(active_db_path, '/');
  const char *base = slash ? slash + 1 : active_db_path;
  char header[40];
  snprintf(header, sizeof(header), "DB: %s", base);

  submenu_reset(ui->submenu);
  submenu_set_header(ui->submenu, header);
  submenu_add_item(ui->submenu, "Scan tag", NfcStockSceneScan,
                   nfc_stock_ui_submenu_callback, scene_manager);
  submenu_add_item(ui->submenu, "Inventory", NfcStockSceneInventory,
                   nfc_stock_ui_submenu_callback, scene_manager);
  submenu_add_item(ui->submenu, "Settings", NfcStockSceneSettings,
                   nfc_stock_ui_submenu_callback, scene_manager);
}

void nfc_stock_ui_show_item(NfcStockUi *ui, const StockItem *item) {
  char buffer[128];
  char uid_hex[32] = {0};
  widget_reset(ui->widget);

  const char *name = (item->name[0] != '\0') ? item->name : "(empty)";
  const char *location =
      (item->location[0] != '\0') ? item->location : "(empty)";

  size_t pos = 0;
  for (uint8_t i = 0; i < item->uid_len && pos + 2 < sizeof(uid_hex); i++) {
    int written =
        snprintf(uid_hex + pos, sizeof(uid_hex) - pos, "%02X", item->uid[i]);
    if (written <= 0) {
      break;
    }
    pos += (size_t)written;
  }
  if (pos == 0) {
    strncpy(uid_hex, "--", sizeof(uid_hex) - 1);
  }

  snprintf(buffer, sizeof(buffer), "Name: %s", name);
  widget_add_string_element(ui->widget, 0, 0, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  snprintf(buffer, sizeof(buffer), "Stock: %" PRId32 "  Min: %" PRId32,
           item->quantity, item->min_quantity);
  widget_add_string_element(ui->widget, 0, 14, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  snprintf(buffer, sizeof(buffer), "Location: %s", location);
  widget_add_string_element(ui->widget, 0, 28, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  snprintf(buffer, sizeof(buffer), "UID: %s", uid_hex);
  widget_add_string_element(ui->widget, 0, 42, AlignLeft, AlignTop, FontPrimary,
                            buffer);

  widget_add_string_element(ui->widget, 0, 55, AlignLeft, AlignTop,
                            FontSecondary, "Up/Down +/-  OK:Edit  Back:Save");
}
