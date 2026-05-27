/**
 * Composition root: wires Flipper GUI, scene_manager, and application state.
 * Domain rules live in src/domain; persistence in src/persistence; UI views in
 * src/ui.
 */
#include "include/app_types.h"
#include "include/scenes.h"
#include "include/storage_helper.h"
#include "include/ui.h"
#include "version.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>

static void nfc_stock_scene_menu_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  storage_helper_get_active_db(app->active_db_path,
                               sizeof(app->active_db_path));
  nfc_stock_ui_apply_main_menu(app->ui, app->scene_manager,
                               app->active_db_path);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static void nfc_stock_scene_scan_on_enter_main(void *context) {
  nfc_stock_scene_scan_on_enter(context);
}

static void nfc_stock_scene_settings_on_enter(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  nfc_stock_ui_configure_settings(app->ui, app);
  view_dispatcher_switch_to_view(app->view_dispatcher, NfcStockViewSubmenu);
}

static bool nfc_stock_scene_menu_on_event(void *context,
                                          SceneManagerEvent event) {
  UNUSED(context);
  UNUSED(event);
  return false;
}

static bool nfc_stock_scene_scan_on_event_main(void *context,
                                               SceneManagerEvent event) {
  return nfc_stock_scene_scan_on_event(context, event);
}

static bool nfc_stock_scene_stub_on_event(void *context,
                                          SceneManagerEvent event) {
  UNUSED(context);
  UNUSED(event);
  return false;
}

static void nfc_stock_scene_stub_on_exit(void *context) { UNUSED(context); }

static void nfc_stock_scene_scan_on_exit_main(void *context) {
  nfc_stock_scene_scan_on_exit(context);
}

void (*const nfc_stock_on_enter_handlers[])(void *) = {
    [NfcStockSceneMenu] = nfc_stock_scene_menu_on_enter,
    [NfcStockSceneScan] = nfc_stock_scene_scan_on_enter_main,
    [NfcStockSceneDetails] = nfc_stock_scene_details_on_enter,
    [NfcStockSceneEdit] = nfc_stock_scene_edit_on_enter,
    [NfcStockSceneInventory] = nfc_stock_scene_inventory_on_enter,
    [NfcStockSceneSettings] = nfc_stock_scene_settings_on_enter,
};

bool (*const nfc_stock_on_event_handlers[])(void *, SceneManagerEvent) = {
    [NfcStockSceneMenu] = nfc_stock_scene_menu_on_event,
    [NfcStockSceneScan] = nfc_stock_scene_scan_on_event_main,
    [NfcStockSceneDetails] = nfc_stock_scene_details_on_event,
    [NfcStockSceneEdit] = nfc_stock_scene_edit_on_event,
    [NfcStockSceneInventory] = nfc_stock_scene_stub_on_event,
    [NfcStockSceneSettings] = nfc_stock_scene_stub_on_event,
};

void (*const nfc_stock_on_exit_handlers[])(void *) = {
    [NfcStockSceneMenu] = nfc_stock_scene_stub_on_exit,
    [NfcStockSceneScan] = nfc_stock_scene_scan_on_exit_main,
    [NfcStockSceneDetails] = nfc_stock_scene_details_on_exit,
    [NfcStockSceneEdit] = nfc_stock_scene_edit_on_exit,
    [NfcStockSceneInventory] = nfc_stock_scene_stub_on_exit,
    [NfcStockSceneSettings] = nfc_stock_scene_stub_on_exit,
};

static const SceneManagerHandlers nfc_stock_scene_handlers = {
    .on_enter_handlers = nfc_stock_on_enter_handlers,
    .on_event_handlers = nfc_stock_on_event_handlers,
    .on_exit_handlers = nfc_stock_on_exit_handlers,
    .scene_num = NfcStockSceneCount,
};

static bool nfc_stock_custom_event_callback(void *context, uint32_t event) {
  NfcStockApp *app = (NfcStockApp *)context;
  return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_stock_navigation_event_callback(void *context) {
  NfcStockApp *app = (NfcStockApp *)context;
  if (nfc_stock_scene_edit_consume_nav_back(app)) {
    return true;
  }
  return scene_manager_handle_back_event(app->scene_manager);
}

int32_t nfc_stock_app(void *p) {
  UNUSED(p);
  NfcStockApp *app = calloc(1, sizeof(NfcStockApp));
  if (!app) {
    return -1;
  }

  storage_helper_ensure_default();
  storage_helper_get_active_db(app->active_db_path,
                               sizeof(app->active_db_path));

  app->scene_manager = scene_manager_alloc(&nfc_stock_scene_handlers, app);
  app->view_dispatcher = view_dispatcher_alloc();

  app->ui = nfc_stock_ui_alloc();
  nfc_stock_ui_configure(app->ui, app->view_dispatcher, app->scene_manager);

  view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
  view_dispatcher_set_custom_event_callback(app->view_dispatcher,
                                            nfc_stock_custom_event_callback);
  view_dispatcher_set_navigation_event_callback(
      app->view_dispatcher, nfc_stock_navigation_event_callback);

  Gui *gui = furi_record_open(RECORD_GUI);
  view_dispatcher_attach_to_gui(app->view_dispatcher, gui,
                                ViewDispatcherTypeFullscreen);

  scene_manager_next_scene(app->scene_manager, NfcStockSceneMenu);

  view_dispatcher_run(app->view_dispatcher);

  /* Remove registered views before tearing down UI modules. */
  view_dispatcher_remove_view(app->view_dispatcher, NfcStockViewSubmenu);
  view_dispatcher_remove_view(app->view_dispatcher, NfcStockViewWidget);
  view_dispatcher_remove_view(app->view_dispatcher, NfcStockViewTextInput);
  view_dispatcher_remove_view(app->view_dispatcher, NfcStockViewInventory);
  view_dispatcher_remove_view(app->view_dispatcher, NfcStockViewEdit);

  /* Keep UI alive while scene_manager runs final scene on_exit handlers. */
  scene_manager_free(app->scene_manager);
  nfc_stock_ui_free(app->ui);
  view_dispatcher_free(app->view_dispatcher);
  furi_record_close(RECORD_GUI);
  free(app);

  return 0;
}
