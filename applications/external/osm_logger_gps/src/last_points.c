// Vue "Last points" : liste scrollable des 10 derniers saves + actions (undo, clear).
// Architecture : Submenu Flipper dont les items sont générés à chaque entrée
// (read_last_points). En-dessous, deux items d'action.

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>

#include "app.h"
#include "last_points.h"
#include "point_detail.h"
#include "storage_helpers.h"

#define MAX_DISPLAY 10
#define LINE_BUF_SIZE 56

// Chaque action a un ID distinct en dehors de la plage [0..MAX_DISPLAY-1]
#define ACTION_DELETE_LAST 100
#define ACTION_CLEAR_ALL   101
#define ACTION_ARCHIVE     102

static char g_lines[MAX_DISPLAY][LINE_BUF_SIZE];

static void last_points_item_callback(void* ctx, uint32_t index) {
    App* app = (App*)ctx;
    if(index == ACTION_DELETE_LAST) {
        if(storage_delete_last_point()) {
            if(app->total_count > 0) app->total_count--;
            if(app->session_count > 0) app->session_count--;
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
        }
        app_start_last_points(app);
    } else if(index == ACTION_CLEAR_ALL) {
        storage_clear_all_points();
        app->total_count = 0;
        app->session_count = 0;
        notification_message(app->notification, &sequence_success);
        app_start_last_points(app);
    } else if(index == ACTION_ARCHIVE) {
        char err[48];
        if(storage_archive_session(err, sizeof(err))) {
            app->total_count = 0;
            app->session_count = 0;
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
        }
        app_start_last_points(app);
    } else if(index < MAX_DISPLAY) {
        // Clic sur un point individuel -> détails
        char raw[320];
        if(storage_get_point_raw((uint8_t)index, raw, sizeof(raw))) {
            app_show_point_detail(app, raw);
        }
    }
}

static uint32_t last_points_previous(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

static void build_list(App* app) {
    submenu_reset(app->last_points_menu);
    submenu_set_header(app->last_points_menu, "Last points");

    uint8_t n = storage_read_last_points((char*)g_lines, MAX_DISPLAY, LINE_BUF_SIZE);

    if(n == 0) {
        submenu_add_item(
            app->last_points_menu, "(no points yet)", 0, last_points_item_callback, app);
    } else {
        for(uint8_t i = 0; i < n; i++) {
            submenu_add_item(
                app->last_points_menu, g_lines[i], i, last_points_item_callback, app);
        }
    }

    submenu_add_item(
        app->last_points_menu,
        "--- Delete last ---",
        ACTION_DELETE_LAST,
        last_points_item_callback,
        app);
    submenu_add_item(
        app->last_points_menu,
        "--- Archive session ---",
        ACTION_ARCHIVE,
        last_points_item_callback,
        app);
    submenu_add_item(
        app->last_points_menu,
        "--- Clear all ---",
        ACTION_CLEAR_ALL,
        last_points_item_callback,
        app);
}

void app_start_last_points(App* app) {
    if(!app->last_points_menu) {
        app->last_points_menu = submenu_alloc();
        View* v = submenu_get_view(app->last_points_menu);
        view_set_previous_callback(v, last_points_previous);
        view_dispatcher_add_view(app->dispatcher, AppViewLastPoints, v);
    }
    build_list(app);
    view_dispatcher_switch_to_view(app->dispatcher, AppViewLastPoints);
}

void app_stop_last_points(App* app) {
    if(app->last_points_menu) {
        view_dispatcher_remove_view(app->dispatcher, AppViewLastPoints);
        submenu_free(app->last_points_menu);
        app->last_points_menu = NULL;
    }
}
