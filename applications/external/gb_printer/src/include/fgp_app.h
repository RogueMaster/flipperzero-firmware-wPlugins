// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#ifndef FGP_APP_H
#define FGP_APP_H

#pragma once

// Libraries
#include <gblink/include/gblink.h>

// FURI
#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <src/include/fgp_palette.h>

#define OPT_SAVE_BIN     (1 << 0)
#define OPT_SAVE_BIN_HDR (1 << 1)
#define OPT_SAVE_PNG     (1 << 2)
#define RECV_OPTS        (OPT_SAVE_BIN | OPT_SAVE_BIN_HDR | OPT_SAVE_PNG)

struct fgp_app {
    ViewDispatcher* view_dispatcher;

    // Scenes
    SceneManager* scene_manager;
    Submenu* submenu;
    VariableItemList* variable_item_list;
    void* receive_view;

    Storage* storage;

    void* printer_handle;

    unsigned int options;
    unsigned int palette_idx;
};

typedef enum {
    fgpViewSubmenu,
    fgpViewVariableItemList,
    fgpViewReceive,
} fgpView;

#endif // FGP_APP_H
