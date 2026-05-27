/**
 * Presentation: scene_manager enter/event/exit hooks and settings submenu.
 * Implementations live in src/scenes.c (single TU for Flipper builds).
 */
#pragma once

#include "app_types.h"
#include "ui.h"
#include <gui/scene_manager.h>
#include <stdbool.h>

void nfc_stock_scene_scan_on_enter(void *context);
void nfc_stock_scene_scan_on_exit(void *context);
bool nfc_stock_scene_scan_on_event(void *context, SceneManagerEvent event);

void nfc_stock_scene_details_on_enter(void *context);
bool nfc_stock_scene_details_on_event(void *context, SceneManagerEvent event);
void nfc_stock_scene_details_on_exit(void *context);

void nfc_stock_scene_edit_on_enter(void *context);
bool nfc_stock_scene_edit_on_event(void *context, SceneManagerEvent event);
void nfc_stock_scene_edit_on_exit(void *context);
bool nfc_stock_scene_edit_consume_nav_back(void *context);

void nfc_stock_scene_inventory_on_enter(void *context);

void nfc_stock_ui_configure_settings(NfcStockUi *ui, NfcStockApp *app);
