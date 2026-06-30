#include <gui/view.h>
#include <gui/modules/submenu.h>
#include "../../main.h"
#include "main.h"
#include <f0_mtp_icons.h>
#include "usb.h"
#include "usb_desc.h"

#define THIS_SCENE MTP

AppMTP* tmp;

void MTP_on_draw(Canvas* canvas, void* context);
AppMTP* MTP_alloc() {
    AppMTP* mtp = (AppMTP*)malloc(sizeof(AppMTP));
    mtp->view = view_alloc();
    view_set_context(mtp->view, mtp);
    view_set_draw_callback(mtp->view, MTP_on_draw);

    mtp->storage = furi_record_open(RECORD_STORAGE);
    tmp = mtp;

    return mtp;
}

void MTP_on_draw(Canvas* canvas, void* context) {
    AppMTP* mtp = (AppMTP*)context;

    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);

    bool usb_connected = false;
    if(mtp == NULL) {
        // facepalm
        if(tmp != NULL) {
            usb_connected = tmp->usb_connected;
        }
    } else {
        usb_connected = mtp->usb_connected;
    }

    if(usb_connected) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 0, 14, &I_DFU_128x50);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 22, "Disconnect or");
        canvas_draw_icon(canvas, 28, 23, &I_Pin_back_arrow_10x8);
        canvas_draw_str(canvas, 3, 31, "Press");
    } else {
        canvas_draw_icon(canvas, 1, 31, &I_Connect_me_62x31);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_draw_str(canvas, 10, 25, "Plug me into computer!");
        canvas_draw_icon(canvas, 2, 2, &I_Pin_back_arrow_10x8);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 15, 10, "Exit");
        canvas_draw_str(canvas, 61, 41, "Waiting for USB");
        canvas_draw_str(canvas, 72, 50, "Connection...");
    }
}

void MTP_free(void* ptr) {
    AppMTP* home = (AppMTP*)ptr;
    FURI_LOG_I("DemoApp", "Triggering Free for view");

    view_free(home->view);
    home->view = NULL;

    free(home);
}

View* MTP_get_view(void* ptr) {
    AppMTP* home = (AppMTP*)ptr;
    return home->view;
}

void MTP_on_enter(void* context) {
    App* app = (App*)context;

    view_dispatcher_switch_to_view(app->view_dispatcher, THIS_SCENE);
    furi_assert(app->allocated_scenes != NULL, "App allocated scenes is NULL");

    AppMTP* mtp = app->allocated_scenes[THIS_SCENE];
    if(mtp != NULL) {
        mtp->old_usb = furi_hal_usb_get_config();

        // copy serial number
        usb_mtp_interface.str_serial_descr = mtp->old_usb->str_serial_descr;

        // set new usb mode for MTP mode
        if(!furi_hal_usb_set_config(&usb_mtp_interface, mtp)) {
            FURI_LOG_E("MTP", "Failed to set MTP mode");
            return;
        }
    }
}

bool MTP_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }

    return true;
}

void MTP_on_exit(void* context) {
    App* app = (App*)context;
    if(app == NULL) {
        return;
    }

    if(app->allocated_scenes != NULL) {
        AppMTP* mtp = app->allocated_scenes[THIS_SCENE];
        if(mtp != NULL) {
            // free all handles
            FileHandle* handle = mtp->handles;
            mtp->handles = NULL;

            while(handle != NULL) {
                FileHandle* next = handle->next;

                if(handle->path != NULL) free(handle->path);
                free(handle);

                handle = next;
            }
        }
    }

    furi_record_close(RECORD_STORAGE);

    // revert to old usb mode
    furi_hal_usb_set_config(tmp->old_usb, NULL);

    // if(app->view_dispatcher) view_dispatcher_switch_to_view(app->view_dispatcher, Home);
    // if(app->scene_manager) scene_manager_previous_scene(app->scene_manager);
}
