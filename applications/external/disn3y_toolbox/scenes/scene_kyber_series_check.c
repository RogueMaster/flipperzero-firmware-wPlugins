#include <gui/icon_i.h>

#include "../disn3y_toolbox_app.h"
#include "disn3y_toolbox_icons.h"

static bool worker_running;

static const uint8_t kyber_read_animation_length = 4;
static const uint8_t kyber_read_animation_mod = 2;
static const Icon* kyber_read_animation[] = {
    &I_crystal_write_frame_0,
    &I_crystal_write_frame_1,
    &I_crystal_write_frame_2,
    &I_crystal_write_frame_3,
};

static void kyber_series_check_read_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    Disn3yToolboxApp* app = context;
    if(result == LFRFIDWorkerReadDone) {
        app->protocol_id = protocol;
        view_dispatcher_send_custom_event(app->view_dispatcher, Disn3yToolboxEventReadDone);
    }
}

void disn3y_toolbox_app_scene_kyber_series_check_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Popup* popup = app->popup;

    popup_set_icon(
        popup, 5, 2, kyber_read_animation[app->animation_counter / kyber_read_animation_mod]);
    popup_set_header(popup, "Reading", 97, 12, AlignCenter, AlignTop);
    popup_set_text(popup, "Place\ncrystal\non Flipper", 97, 25, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewPopup);

    worker_running = true;
    lfrfid_worker_start_thread(app->lfworker);
    lfrfid_worker_read_start(
        app->lfworker, LFRFIDWorkerReadTypeAuto, kyber_series_check_read_callback, app);

    notification_message(app->notifications, &sequence_blink_start_cyan);
}

bool disn3y_toolbox_app_scene_kyber_series_check_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Disn3yToolboxEventReadDone) {
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_success);

            // Extract EM4100 data (5 bytes)
            uint8_t read_data[5] = {0};
            protocol_dict_get_data(app->protocol_dict, app->protocol_id, read_data, 5);

            uint8_t em_hi = read_data[3];
            uint8_t em_lo = read_data[4];

            FuriString* result = furi_string_alloc();
            furi_string_printf(result, "\e#Crystal Checker\nEM4100: %02X %02X\n", em_hi, em_lo);

            // Find Series 1 match
            bool s1_found = false;
            for(size_t i = 0; i < CrystalSeries1_MAX; i++) {
                if(CrystalsSeries1[i].em4100[0] == em_hi &&
                   CrystalsSeries1[i].em4100[1] == em_lo) {
                    furi_string_cat(result, "\n\e#Series 1 Match:\n");
                    if(strcmp(CrystalsSeries1[i].hilt_color, CrystalsSeries1[i].blade_color) ==
                       0) {
                        furi_string_cat_printf(
                            result, "Color: %s\n", CrystalsSeries1[i].hilt_color);
                    } else {
                        furi_string_cat_printf(
                            result,
                            "Blade: %s   Hilt: %s\n",
                            CrystalsSeries1[i].blade_color,
                            CrystalsSeries1[i].hilt_color);
                    }
                    furi_string_cat_printf(result, "%s\n", CrystalsSeries1[i].jedi_voice);
                    s1_found = true;
                    break;
                }
            }
            if(!s1_found) {
                furi_string_cat(result, "\n\e#Series 1 Match:\nNo matches found\n");
            }

            // Find Series 2 matches (multiple possible)
            bool s2_found = false;
            for(size_t i = 0; i < CrystalSeries2_MAX; i++) {
                if(CrystalsSeries2[i].em4100[0] == em_hi &&
                   CrystalsSeries2[i].em4100[1] == em_lo) {
                    if(!s2_found) {
                        furi_string_cat(result, "\n\e#Series 2 Options:");
                        s2_found = true;
                    }
                    furi_string_cat_printf(
                        result,
                        "\n- %s (%s)",
                        CrystalsSeries2[i].crystal_color,
                        CrystalsSeries2[i].voice);
                }
            }
            if(!s2_found) {
                furi_string_cat(result, "\n\e#Series 2 Options:\nNo matches found");
            }

            // Stop the worker before switching views
            lfrfid_worker_stop(app->lfworker);
            lfrfid_worker_stop_thread(app->lfworker);
            worker_running = false;

            // Switch to widget view with results
            Widget* widget = app->widget;
            widget_reset(widget);
            widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(result));
            furi_string_free(result);

            view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewWidget);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick && worker_running) {
        Popup* popup = app->popup;
        if(app->animation_counter == 0) {
            app->animation_counter++;
            app->animation_counter_direction = false;
        } else if(
            app->animation_counter ==
            ((kyber_read_animation_length * kyber_read_animation_mod) - 1)) {
            app->animation_counter--;
            app->animation_counter_direction = true;
        } else {
            app->animation_counter += app->animation_counter_direction ? -1 : 1;
        }
        popup_set_icon(
            popup, 5, 2, kyber_read_animation[app->animation_counter / kyber_read_animation_mod]);
    }

    return consumed;
}

void disn3y_toolbox_app_scene_kyber_series_check_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    widget_reset(app->widget);
    app->animation_counter = 0;
    app->animation_counter_direction = false;
    if(worker_running) {
        lfrfid_worker_stop(app->lfworker);
        lfrfid_worker_stop_thread(app->lfworker);
        worker_running = false;
    }
}
