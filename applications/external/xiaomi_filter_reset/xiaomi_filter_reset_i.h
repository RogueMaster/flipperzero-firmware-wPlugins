// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_i.h
 * @brief Shared application state and identifiers.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>

#include "src/nfc/xiaomi_filter_worker.h"
#include "scenes/xiaomi_filter_reset_scene.h"

/** @brief Registered view identifiers. */
typedef enum {
    XiaomiFilterResetViewSubmenu,
    XiaomiFilterResetViewPopup,
    XiaomiFilterResetViewWidget,
} XiaomiFilterResetViewId;

/** @brief Custom events dispatched to scenes. */
typedef enum {
    XiaomiFilterResetCustomEventWorkerDone = 100,
    XiaomiFilterResetCustomEventPopupTimeout,
    XiaomiFilterResetCustomEventButtonRetry,
} XiaomiFilterResetCustomEvent;

/** @brief Application context, shared by all scenes. */
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    Popup* popup;
    Widget* widget;

    Nfc* nfc;
    XiaomiFilterWorker* worker;

    // Which action the scan scene should perform, chosen from the start menu.
    XiaomiFilterWorkerOp pending_op;
} XiaomiFilterResetApp;
