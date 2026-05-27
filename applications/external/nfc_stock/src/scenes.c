/**
 * Presentation layer: scene_manager hooks and settings submenu wiring.
 * Single TU keeps Flipper build simple and avoids dozens of tiny scene files.
 */
#include "../version.h"
#include "include/app_types.h"
#include "include/clock.h"
#include "include/db.h"
#include "include/export.h"
#include "include/fs_compat.h"
#include "include/stock.h"
#include "include/storage_helper.h"
#include "include/ui.h"
#include <furi.h>
#include <furi_hal_random.h>
#include <gui/canvas.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <inttypes.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <string.h>

/* ----- Scan ----- */

typedef struct {
  NfcStockApp *app;
  Nfc *nfc;
  NfcPoller *poller;
} ScanContext;

static ScanContext g_scan;

static NfcCommand nfc_stock_scene_scan_callback(NfcGenericEvent event,
                                                void *context) {
  ScanContext *ctx = (ScanContext *)context;
  if (event.protocol != NfcProtocolIso14443_3a || !event.event_data) {
    return NfcCommandContinue;
  }

  const Iso14443_3aPollerEvent *iso_ev =
      (const Iso14443_3aPollerEvent *)event.event_data;
  if (iso_ev->type != Iso14443_3aPollerEventTypeReady) {
    return NfcCommandContinue;
  }

  const Iso14443_3aData *data =
      (const Iso14443_3aData *)nfc_poller_get_data(ctx->poller);
  if (!data || data->uid_len == 0) {
    return NfcCommandContinue;
  }

  NfcStockApp *app = ctx->app;
  const uint8_t *uid = data->uid;
  uint8_t uid_len = data->uid_len;

  StockItem found;
  if (stock_db_find_by_uid(app->active_db_path, uid, uid_len, &found)) {
    app->item_is_new = false;
    app->current_item = found;
    view_dispatcher_send_custom_event(app->view_dispatcher,
                                      NfcStockEventScanKnown);
  } else {
    memset(&app->current_item, 0, sizeof(StockItem));
    memcpy(app->current_item.uid, uid, uid_len);
    app->current_item.uid_len = uid_len;
    stock_item_init(&app->current_item, "NEW", 0, 0, "Unassigned");
    app->item_is_new = true;
    view_dispatcher_send_custom_event(app->view_dispatcher,
                                      NfcStockEventScanUnknown);
  }

  return NfcCommandStop;
}

void nfc_stock_scene_scan_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  g_scan.app = app;
  g_scan.nfc = nfc_alloc();
  g_scan.poller = nfc_poller_alloc(g_scan.nfc, NfcProtocolIso14443_3a);
  nfc_poller_start(g_scan.poller, nfc_stock_scene_scan_callback, &g_scan);

  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 64, 6, AlignCenter, AlignTop,
                            FontPrimary, "NFC Scanner");
  widget_add_string_element(app->ui->widget, 64, 21, AlignCenter, AlignTop,
                            FontSecondary, "[    NFC TAG    ]");
  widget_add_string_element(app->ui->widget, 64, 35, AlignCenter, AlignTop,
                            FontPrimary, "Bring tag near back");
  widget_add_string_element(app->ui->widget, 64, 49, AlignCenter, AlignTop,
                            FontSecondary, "Back: cancel");
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

bool nfc_stock_scene_scan_on_event(void *context, SceneManagerEvent event) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event.type == SceneManagerEventTypeCustom) {
    if (event.event == NfcStockEventScanKnown) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneDetails);
      return true;
    }
    if (event.event == NfcStockEventScanUnknown) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
      return true;
    }
  }
  return false;
}

void nfc_stock_scene_scan_on_exit(void *context) {
  UNUSED(context);
  if (g_scan.poller) {
    nfc_poller_stop(g_scan.poller);
    nfc_poller_free(g_scan.poller);
    g_scan.poller = NULL;
  }
  if (g_scan.nfc) {
    nfc_free(g_scan.nfc);
    g_scan.nfc = NULL;
  }
}
/* ----- Details ----- */

static bool nfc_stock_details_input_callback(InputEvent *event, void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event->type == InputTypeShort) {
    if (event->key == InputKeyUp) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsStockUp);
      return true;
    }
    if (event->key == InputKeyDown) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsStockDown);
      return true;
    }
    if (event->key == InputKeyOk) {
      view_dispatcher_send_custom_event(app->view_dispatcher,
                                        NfcStockEventDetailsOpenEdit);
      return true;
    }
  }
  return false;
}

void nfc_stock_scene_details_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_ui_show_item(app->ui, &app->current_item);
  View *view = widget_get_view(app->ui->widget);
  view_set_context(view, app);
  view_set_input_callback(view, nfc_stock_details_input_callback);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

bool nfc_stock_scene_details_on_event(void *context, SceneManagerEvent event) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (event.type == SceneManagerEventTypeCustom) {
    if (event.event == NfcStockEventDetailsStockUp) {
      stock_item_update_quantity(&app->current_item, 1);
    } else if (event.event == NfcStockEventDetailsStockDown) {
      stock_item_update_quantity(&app->current_item, -1);
    } else if (event.event == NfcStockEventDetailsOpenEdit) {
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
      return true;
    } else {
      return false;
    }
    nfc_stock_ui_show_item(app->ui, &app->current_item);
    return true;
  }
  return false;
}

void nfc_stock_scene_details_on_exit(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  View *view = widget_get_view(app->ui->widget);
  view_set_input_callback(view, NULL);
  view_set_context(view, NULL);
  stock_db_upsert(app->active_db_path, &app->current_item);
}
/* ----- Edit ----- */

typedef enum {
  EditFieldName = 0,
  EditFieldQty,
  EditFieldMin,
  EditFieldLoc,
} EditFieldIndex;

static char s_text_buf[64];
static uint32_t s_edit_field_index;

static void nfc_stock_edit_refresh_list(NfcStockApp *app);

static void nfc_stock_edit_after_text_input(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  app->edit_text_input_blocking_back = false;
  StockItem *it = &app->current_item;

  switch ((EditFieldIndex)s_edit_field_index) {
  case EditFieldName:
    strncpy(it->name, s_text_buf, MAX_ITEM_NAME - 1);
    it->name[MAX_ITEM_NAME - 1] = '\0';
    break;
  case EditFieldQty: {
    char *end = NULL;
    long v = strtol(s_text_buf, &end, 10);
    if (end != s_text_buf && v >= 0 && v <= (long)INT32_MAX) {
      it->quantity = (int32_t)v;
    }
    break;
  }
  case EditFieldMin: {
    char *end = NULL;
    long v = strtol(s_text_buf, &end, 10);
    if (end != s_text_buf && v >= 0 && v <= (long)INT32_MAX) {
      it->min_quantity = (int32_t)v;
    }
    break;
  }
  case EditFieldLoc:
    strncpy(it->location, s_text_buf, MAX_LOCATION - 1);
    it->location[MAX_LOCATION - 1] = '\0';
    break;
  default:
    break;
  }
  it->last_updated = app_now_timestamp();
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
  nfc_stock_edit_refresh_list(app);
}

static void nfc_stock_edit_enter_cb(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  s_edit_field_index = index;
  const StockItem *it = &app->current_item;

  text_input_reset(app->ui->text_input);
  memset(s_text_buf, 0, sizeof(s_text_buf));

  switch ((EditFieldIndex)index) {
  case EditFieldName:
    text_input_set_header_text(app->ui->text_input, "Name");
    strncpy(s_text_buf, it->name, sizeof(s_text_buf) - 1);
    break;
  case EditFieldQty:
    text_input_set_header_text(app->ui->text_input, "Stock (digits)");
    snprintf(s_text_buf, sizeof(s_text_buf), "%" PRId32, it->quantity);
    break;
  case EditFieldMin:
    text_input_set_header_text(app->ui->text_input, "Min stock (digits)");
    snprintf(s_text_buf, sizeof(s_text_buf), "%" PRId32, it->min_quantity);
    break;
  case EditFieldLoc:
    text_input_set_header_text(app->ui->text_input, "Location");
    strncpy(s_text_buf, it->location, sizeof(s_text_buf) - 1);
    break;
  default:
    return;
  }

  text_input_set_result_callback(app->ui->text_input,
                                 nfc_stock_edit_after_text_input, app,
                                 s_text_buf, sizeof(s_text_buf), false);
  app->edit_text_input_blocking_back = true;
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewTextInput);
}

static void nfc_stock_edit_refresh_list(NfcStockApp *app) {
  static char line_name[48];
  static char line_stock[48];
  static char line_min[48];
  static char line_location[48];

  variable_item_list_reset(app->ui->edit_list);
  const StockItem *it = &app->current_item;

  snprintf(line_name, sizeof(line_name), "Name: %s", it->name);
  variable_item_list_add(app->ui->edit_list, line_name, 0, NULL, NULL);
  snprintf(line_stock, sizeof(line_stock), "Stock: %" PRId32, it->quantity);
  variable_item_list_add(app->ui->edit_list, line_stock, 0, NULL, NULL);
  snprintf(line_min, sizeof(line_min), "Min stock: %" PRId32, it->min_quantity);
  variable_item_list_add(app->ui->edit_list, line_min, 0, NULL, NULL);
  snprintf(line_location, sizeof(line_location), "Location: %s", it->location);
  variable_item_list_add(app->ui->edit_list, line_location, 0, NULL, NULL);

  variable_item_list_set_enter_callback(app->ui->edit_list,
                                        nfc_stock_edit_enter_cb, app);
}

void nfc_stock_scene_edit_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_edit_refresh_list(app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
}

bool nfc_stock_scene_edit_on_event(void *context, SceneManagerEvent event) {
  UNUSED(context);
  UNUSED(event);
  return false;
}

void nfc_stock_scene_edit_on_exit(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (app->item_is_new) {
    if (strlen(app->current_item.name) == 0 ||
        strcmp(app->current_item.name, "NEW") == 0) {
      snprintf(app->current_item.name, MAX_ITEM_NAME, "ITEM_%04lX",
               (unsigned long)(furi_hal_random_get() & 0xFFFF));
    }
    app->item_is_new = false;
  }
  stock_db_upsert(app->active_db_path, &app->current_item);
}

bool nfc_stock_scene_edit_consume_nav_back(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (!app->edit_text_input_blocking_back) {
    return false;
  }
  app->edit_text_input_blocking_back = false;
  text_input_reset(app->ui->text_input);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewEdit);
  return true;
}
/* ----- Inventory ----- */

typedef enum {
  InvRow_Edit,
  InvRow_Delete,
} InventoryRowAction;

static uint32_t selected_index = 0;

static void nfc_stock_inventory_callback(void *context, uint32_t index);

static void nfc_stock_inventory_show_empty(NfcStockApp *app) {
  variable_item_list_reset(app->ui->inventory_list);
  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                            FontPrimary, "No items in inventory");
  widget_add_string_element(app->ui->widget, 0, 26, AlignLeft, AlignTop,
                            FontSecondary, "Scan a tag or add from NFC");
  widget_add_string_element(app->ui->widget, 0, 42, AlignLeft, AlignTop,
                            FontSecondary, "Back: return");
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

static void nfc_stock_inventory_refresh(NfcStockApp *app) {
  static char row_labels[128][64];

  variable_item_list_reset(app->ui->inventory_list);

  StockItem *items = NULL;
  size_t count = 0;
  if (!fs_read_all_stock_items(app->active_db_path, &items, &count) ||
      count == 0) {
    if (items) {
      free(items);
    }
    nfc_stock_inventory_show_empty(app);
    return;
  }

  for (size_t i = 0; i < count; i++) {
    if (i >= 128) {
      break;
    }
    bool alert = (items[i].quantity <= items[i].min_quantity);
    snprintf(row_labels[i], sizeof(row_labels[i]), "%s (%" PRId32 ")%s",
             items[i].name, items[i].quantity, alert ? " !" : "");
    variable_item_list_add(app->ui->inventory_list, row_labels[i], 0, NULL,
                           NULL);
  }
  free(items);

  variable_item_list_set_enter_callback(app->ui->inventory_list,
                                        nfc_stock_inventory_callback, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewInventory);
}

static void nfc_stock_inventory_action_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;

  if (index == InvRow_Edit) {
    if (fs_read_stock_at(app->active_db_path, selected_index,
                         &app->current_item)) {
      app->item_is_new = false;
      scene_manager_next_scene(app->scene_manager, NfcStockSceneEdit);
    }
  } else if (index == InvRow_Delete) {
    fs_delete_stock_at_index(app->active_db_path, selected_index);
    nfc_stock_inventory_refresh(app);
  }
}

static void nfc_stock_inventory_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  selected_index = index;

  submenu_reset(app->ui->submenu);
  submenu_add_item(app->ui->submenu, "Edit", InvRow_Edit,
                   nfc_stock_inventory_action_callback, app);
  submenu_add_item(app->ui->submenu, "Delete", InvRow_Delete,
                   nfc_stock_inventory_action_callback, app);

  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

void nfc_stock_scene_inventory_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_inventory_refresh(app);
}
/* ----- Settings ----- */

typedef enum {
  Settings_ExportCSV,
  Settings_SelectDB,
  Settings_ResetStock,
  Settings_ClearAll,
  Settings_Credits,
} SettingsMenuAction;

typedef enum {
  SettingsDbActionCreate = 1,
  SettingsDbActionRename = 2,
  SettingsDbActionSelect = 3,
} SettingsDbAction;

static char s_db_name_buf[40];
static SettingsDbAction s_db_action = SettingsDbActionCreate;
static char s_db_paths[32][128];
static char s_db_labels[32][40];
static size_t s_db_count = 0;

static void nfc_stock_ui_configure_settings_menu(NfcStockUi *ui,
                                                 NfcStockApp *app);

static void nfc_stock_settings_sanitize_name(char *name) {
  for (size_t i = 0; name[i] != '\0'; i++) {
    char c = name[i];
    bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!valid) {
      name[i] = '_';
    }
  }
}

static void nfc_stock_settings_build_db_path(char *out, size_t out_size,
                                             const char *name) {
  size_t len = strlen(name);
  if (len >= 4 && strcmp(name + len - 4, ".bin") == 0) {
    snprintf(out, out_size, STORAGE_PATH "/%s", name);
  } else {
    snprintf(out, out_size, STORAGE_PATH "/%s.bin", name);
  }
}

static void nfc_stock_settings_apply_db_name(NfcStockApp *app) {
  if (s_db_name_buf[0] == '\0') {
    nfc_stock_ui_configure_settings_menu(app->ui, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
    return;
  }

  nfc_stock_settings_sanitize_name(s_db_name_buf);
  char new_path[sizeof(app->active_db_path)];
  nfc_stock_settings_build_db_path(new_path, sizeof(new_path), s_db_name_buf);

  if (s_db_action == SettingsDbActionRename &&
      strcmp(new_path, app->active_db_path) != 0) {
    StockItem *items = NULL;
    size_t count = 0;
    if (fs_read_all_stock_items(app->active_db_path, &items, &count)) {
      fs_write_replace(new_path, items, count * sizeof(StockItem));
      free(items);
      fs_write_replace(app->active_db_path, "", 0);
    } else {
      fs_write_replace(new_path, "", 0);
    }
  } else {
    fs_write_replace(new_path, "", 0);
  }

  storage_helper_set_active_db(new_path);
  strncpy(app->active_db_path, new_path, sizeof(app->active_db_path) - 1);
  app->active_db_path[sizeof(app->active_db_path) - 1] = '\0';

  nfc_stock_ui_configure_settings_menu(app->ui, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static void nfc_stock_settings_db_text_done(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_settings_apply_db_name(app);
}

static bool nfc_stock_path_has_bin_suffix(const char *path) {
  size_t len = strlen(path);
  return len >= 4 && strcmp(path + len - 4, ".bin") == 0;
}

static void nfc_stock_db_label_from_path(const char *path, char *out,
                                         size_t out_size) {
  const char *slash = strrchr(path, '/');
  const char *base = slash ? slash + 1 : path;
  strncpy(out, base, out_size - 1);
  out[out_size - 1] = '\0';
  size_t len = strlen(out);
  if (len >= 4 && strcmp(out + len - 4, ".bin") == 0) {
    out[len - 4] = '\0';
  }
}

static void nfc_stock_settings_select_existing_callback(void *context,
                                                        uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (index >= s_db_count) {
    return;
  }
  storage_helper_set_active_db(s_db_paths[index]);
  strncpy(app->active_db_path, s_db_paths[index],
          sizeof(app->active_db_path) - 1);
  app->active_db_path[sizeof(app->active_db_path) - 1] = '\0';

  nfc_stock_ui_configure_settings_menu(app->ui, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static void nfc_stock_ui_open_existing_db_menu(NfcStockApp *app) {
  s_db_count = 0;

  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *dir = storage_file_alloc(storage);
  if (storage_dir_open(dir, STORAGE_PATH)) {
    FileInfo file_info;
    char name[96];
    while (s_db_count < 32 &&
           storage_dir_read(dir, &file_info, name, sizeof(name))) {
      if (!nfc_stock_path_has_bin_suffix(name)) {
        continue;
      }

      size_t needed = strlen(STORAGE_PATH) + 1 + strlen(name) + 1;
      if (needed > sizeof(s_db_paths[s_db_count])) {
        continue;
      }
      size_t base_len = strlen(STORAGE_PATH);
      strncpy(s_db_paths[s_db_count], STORAGE_PATH,
              sizeof(s_db_paths[s_db_count]) - 1);
      s_db_paths[s_db_count][sizeof(s_db_paths[s_db_count]) - 1] = '\0';
      s_db_paths[s_db_count][base_len] = '/';
      strncpy(&s_db_paths[s_db_count][base_len + 1], name,
              sizeof(s_db_paths[s_db_count]) - base_len - 2);
      s_db_paths[s_db_count][needed - 1] = '\0';
      nfc_stock_db_label_from_path(s_db_paths[s_db_count],
                                   s_db_labels[s_db_count],
                                   sizeof(s_db_labels[s_db_count]));
      s_db_count++;
    }
    storage_dir_close(dir);
  }
  storage_file_free(dir);
  furi_record_close(RECORD_STORAGE);

  submenu_reset(app->ui->submenu);
  if (s_db_count == 0) {
    widget_reset(app->ui->widget);
    widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                              FontPrimary, "No .bin databases found");
    widget_add_string_element(app->ui->widget, 0, 26, AlignLeft, AlignTop,
                              FontSecondary, "Create one from");
    widget_add_string_element(app->ui->widget, 0, 38, AlignLeft, AlignTop,
                              FontSecondary, "Select warehouse");
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
    return;
  }

  for (size_t i = 0; i < s_db_count; i++) {
    submenu_add_item(app->ui->submenu, s_db_labels[i], (uint32_t)i,
                     nfc_stock_settings_select_existing_callback, app);
  }
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static void nfc_stock_settings_db_action_callback(void *context,
                                                  uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;
  s_db_action = (SettingsDbAction)index;
  if (s_db_action == SettingsDbActionSelect) {
    nfc_stock_ui_open_existing_db_menu(app);
    return;
  }
  memset(s_db_name_buf, 0, sizeof(s_db_name_buf));
  text_input_reset(app->ui->text_input);

  if (s_db_action == SettingsDbActionRename) {
    const char *slash = strrchr(app->active_db_path, '/');
    const char *base = slash ? slash + 1 : app->active_db_path;
    strncpy(s_db_name_buf, base, sizeof(s_db_name_buf) - 1);
    size_t len = strlen(s_db_name_buf);
    if (len >= 4 && strcmp(s_db_name_buf + len - 4, ".bin") == 0) {
      s_db_name_buf[len - 4] = '\0';
    }
    text_input_set_header_text(app->ui->text_input, "Rename DB");
  } else {
    strncpy(s_db_name_buf, "warehouse", sizeof(s_db_name_buf) - 1);
    text_input_set_header_text(app->ui->text_input, "New DB name");
  }

  text_input_set_result_callback(app->ui->text_input,
                                 nfc_stock_settings_db_text_done, app,
                                 s_db_name_buf, sizeof(s_db_name_buf), false);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewTextInput);
}

static void nfc_stock_ui_open_select_db_menu(NfcStockApp *app) {
  submenu_reset(app->ui->submenu);
  submenu_add_item(app->ui->submenu, "Switch existing", SettingsDbActionSelect,
                   nfc_stock_settings_db_action_callback, app);
  submenu_add_item(app->ui->submenu, "New database", SettingsDbActionCreate,
                   nfc_stock_settings_db_action_callback, app);
  submenu_add_item(app->ui->submenu, "Rename current", SettingsDbActionRename,
                   nfc_stock_settings_db_action_callback, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static void nfc_stock_ui_export_csv_execute(NfcStockApp *app) {
  char csv_path[sizeof(app->active_db_path) + 8];
  char csv_line_1[32] = {0};
  char csv_line_2[32] = {0};
  const char *src_path = app->active_db_path;
  size_t src_len = strlen(src_path);
  size_t base_len = src_len;
  if (src_len >= 4 && strcmp(src_path + src_len - 4, ".bin") == 0) {
    base_len = src_len - 4;
  }
  snprintf(csv_path, sizeof(csv_path), "%.*s.csv", (int)base_len, src_path);

  StockItem *items = NULL;
  size_t count = 0;
  if (!fs_read_all_stock_items(app->active_db_path, &items, &count)) {
    return;
  }

  stock_export_csv(csv_path, items, count);
  free(items);

  widget_reset(app->ui->widget);
  widget_add_string_element(app->ui->widget, 0, 10, AlignLeft, AlignTop,
                            FontPrimary, "Exported to:");

  const char *split_token = "/nfc_stock_";
  const char *split = strstr(csv_path, split_token);
  if (split) {
    size_t first_len = (size_t)(split - csv_path) + strlen("/nfc_stock_");
    if (first_len > sizeof(csv_line_1) - 1) {
      first_len = sizeof(csv_line_1) - 1;
    }
    strncpy(csv_line_1, csv_path, first_len);
    csv_line_1[first_len] = '\0';
    strncpy(csv_line_2, split + strlen("/nfc_stock_"), sizeof(csv_line_2) - 1);
  } else {
    const char *slash = strrchr(csv_path, '/');
    const char *filename = slash ? slash + 1 : csv_path;
    if (slash) {
      size_t dir_len = (size_t)(slash - csv_path);
      if (dir_len > sizeof(csv_line_1) - 1) {
        dir_len = sizeof(csv_line_1) - 1;
      }
      strncpy(csv_line_1, csv_path, dir_len);
      csv_line_1[dir_len] = '\0';
    } else {
      strncpy(csv_line_1, "(current folder)", sizeof(csv_line_1) - 1);
    }
    strncpy(csv_line_2, filename, sizeof(csv_line_2) - 1);
  }

  widget_add_string_element(app->ui->widget, 0, 25, AlignLeft, AlignTop,
                            FontSecondary, csv_line_1);
  widget_add_string_element(app->ui->widget, 0, 38, AlignLeft, AlignTop,
                            FontSecondary, csv_line_2);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
}

static void nfc_stock_ui_settings_callback(void *context, uint32_t index) {
  NfcStockApp *app = (NfcStockApp *)context;

  switch (index) {
  case Settings_ExportCSV:
    nfc_stock_ui_export_csv_execute(app);
    break;
  case Settings_SelectDB:
    nfc_stock_ui_open_select_db_menu(app);
    break;
  case Settings_ResetStock: {
    StockItem *items = NULL;
    size_t count = 0;
    if (!fs_read_all_stock_items(app->active_db_path, &items, &count)) {
      break;
    }
    for (size_t i = 0; i < count; i++) {
      items[i].quantity = 0;
    }
    fs_write_replace(app->active_db_path, items, count * sizeof(StockItem));
    free(items);
    break;
  }
  case Settings_ClearAll:
    fs_write_replace(app->active_db_path, "", 0);
    break;
  case Settings_Credits: {
    char version_line[48];
    snprintf(version_line, sizeof(version_line), "by Endika v%s", APP_VERSION);
    widget_reset(app->ui->widget);
    widget_add_string_element(app->ui->widget, 0, 5, AlignLeft, AlignTop,
                              FontPrimary, "NFC Stock Manager");
    widget_add_string_element(app->ui->widget, 0, 20, AlignLeft, AlignTop,
                              FontSecondary, version_line);
    widget_add_string_element(app->ui->widget, 0, 35, AlignLeft, AlignTop,
                              FontSecondary, "https://github.com/endika");
    widget_add_string_element(app->ui->widget, 0, 50, AlignLeft, AlignTop,
                              FontSecondary, "/flipper-nfc-stock");
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewWidget);
    break;
  }
  }
}

void nfc_stock_ui_configure_settings(NfcStockUi *ui, NfcStockApp *app) {
  nfc_stock_ui_configure_settings_menu(ui, app);
}

static void nfc_stock_ui_configure_settings_menu(NfcStockUi *ui,
                                                 NfcStockApp *app) {
  submenu_reset(ui->submenu);
  submenu_add_item(ui->submenu, "Select warehouse", Settings_SelectDB,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Export CSV", Settings_ExportCSV,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Reset stock to 0", Settings_ResetStock,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Clear all inventory", Settings_ClearAll,
                   nfc_stock_ui_settings_callback, app);
  submenu_add_item(ui->submenu, "Credits", Settings_Credits,
                   nfc_stock_ui_settings_callback, app);
}