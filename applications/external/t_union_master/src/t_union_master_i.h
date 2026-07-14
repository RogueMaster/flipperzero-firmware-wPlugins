#pragma once

#include "t_union_master.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <nfc.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <t_union_master_icons.h>

#include "scene/t_union_master_scene.h"
#include "t_union_custom_events.h"

#include "view_modules/submenu.h"
#include "view_modules/popup.h"
#include "view_modules/widget.h"
#include "views/query_progress.h"
#include "views/card_info.h"
#include "views/transaction_list.h"
#include "views/travel_list.h"
#include "views/transaction_detail.h"
#include "views/travel_detail.h"

#include "protocol/t_union_poller.h"
#include "protocol/t_union_msg.h"
#include "utils/t_union_msgext.h"
#include "font_provider/c_font.h"

#include "query_worker.h"

#define TAG "T-UnionMasterApp"

struct TUM_App {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    // Storage* storage;

    Submenu* submenu;
    Popup* popup;
    Widget* widget;
    QureyProgressView* query_progress;
    CardInfoView* card_info;
    TransactionListView* transaction_list;
    TravelListView* travel_list;
    TransactionDetailView* transaction_detail;
    TravelDetailView* travel_detail;

    Nfc* nfc;
    TUnionPoller* poller;
    TUnionPollerError poller_error;

    TUM_QueryWorker* query_worker;

    TUnionMessage* card_message;
    TUnionMessageExt* card_message_ext;
};

typedef enum {
    TUM_ViewMenu,
    TUM_ViewPopup,
    TUM_ViewQureyProgress,
    TUM_ViewWidget,
    TUM_ViewCardInfo,
    TUM_ViewTransactionList,
    TUM_ViewTravelList,
    TUM_ViewTransactionDetail,
    TUM_ViewTravelDetail,
} TUM_View;

static const NotificationSequence tum_sequence_blink_start_cyan = {
    &message_blink_start_10,
    &message_blink_set_color_cyan,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence tum_sequence_blink_start_green = {
    &message_blink_start_10,
    &message_blink_set_color_green,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence tum_sequence_query_ok = {
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
