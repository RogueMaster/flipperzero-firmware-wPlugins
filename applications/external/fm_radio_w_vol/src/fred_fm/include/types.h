/**
 * @file types.h
 * @brief FredFm application struct, view IDs, and submenu indices.
 */
#pragma once

#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include "src/drivers/pt/PT22xx.h"

typedef enum {
    FredFmSubmenuIndexListen,
    FredFmSubmenuIndexConstellation,
    FredFmSubmenuIndexConfigure,
    FredFmSubmenuIndexAbout,
} FredFmSubmenuIndex;

typedef enum {
    FredFmViewSubmenu,
    FredFmViewConfigure,
    FredFmViewListen,
    FredFmViewConstellation,
    FredFmViewAbout,
} FredFmView;

typedef struct {
    uint8_t _dummy;
} FredFmViewModel;

typedef struct FredFm {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    VariableItem* item_freq;
    VariableItem* item_volume;
    VariableItem* item_pt_chip;
    VariableItem* item_amp_power;
    VariableItem* item_amp_mode;
    VariableItem* item_snc;
    VariableItem* item_deemph;
    VariableItem* item_highcut;
    VariableItem* item_mono;
    VariableItem* item_backlight;
#ifdef ENABLE_RDS
    VariableItem* item_rds;
    VariableItem* item_rds_debug;
#endif
    View* listen_view;
    View* constellation_view;
    Widget* widget_about;
    FuriTimer* tick_timer;
#ifdef ENABLE_RDS
    FuriTimer* rds_adc_timer;
#endif
} FredFm;
