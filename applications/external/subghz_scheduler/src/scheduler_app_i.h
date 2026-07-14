#pragma once

#include "helpers/scheduler_custom_file_types.h"
#include "scenes/scheduler_scene.h"
#include "scheduler_app.h"
#include "scheduler_custom_event.h"
#include "subghz_scheduler.h"
#include "views/scheduler_run_view.h"
#include "views/scheduler_interval_view.h"
#include "views/scheduler_start_view_settings.h"

#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <subghz_scheduler_icons.h>

#define SCHEDULER_APP_NAME    "Sub-GHz Scheduler"
#define SCHEDULER_APP_VERSION "3.0"
#define SCHEDULER_APP_AUTHOR  "Patrick Edwards"
#define SCHEDULER_APP_HANDLE  "@shalebridge"

#define GUI_DISPLAY_HEIGHT 64
#define GUI_DISPLAY_WIDTH  128
#define GUI_MARGIN         3
#define GUI_TEXT_GAP       10

#define GUI_TEXTBOX_HEIGHT 12
#define GUI_TABLE_ROW_A    13
#define GUI_TABLE_ROW_B    (GUI_TABLE_ROW_A + GUI_TEXTBOX_HEIGHT) - 1

struct SchedulerApp {
    Expansion* expansion;
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    VariableItemList* var_item_list;
    Submenu* menu;
    Widget* about_widget;
    FuriThread* thread;
    Scheduler* scheduler;
    FuriString* tx_file_path;
    FuriString* save_dir;
    Popup* popup;
    TextInput* text_input;
    SchedulerRunView* run_view;
    SchedulerIntervalView* interval_view;

    char popup_msg[80];
    char save_name_tmp[SCHEDULER_MAX_LEN_NAME];
    volatile bool is_transmitting;
    bool should_reset;
    bool ext_radio_present;
};

typedef enum {
    SchedulerAppViewMenu,
    SchedulerAppViewVarItemList,
    SchedulerAppViewRunSchedule,
    SchedulerAppViewAbout,
    SchedulerAppViewTextInput,
    SchedulerAppViewPopup,
    SchedulerAppViewInterval,
} SchedulerAppView;
