#include <gui/icon_i.h>

#include "../disn3y_toolbox_app.h"
#include "disn3y_toolbox_icons.h"

static const uint8_t kyber_success_animation_length = 5;
static const uint8_t kyber_success_animation_mod = 2;
static const Icon* kyber_success_animation[] = {
    &I_lightsaber_frame_0,
    &I_lightsaber_frame_1,
    &I_lightsaber_frame_2,
    &I_lightsaber_frame_3,
    &I_lightsaber_frame_4,
};

void disn3y_toolbox_app_scene_kyber_write_success_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Popup* popup = app->popup;

    popup_set_header(popup, "Success!", 75, 10, AlignLeft, AlignTop);
    popup_set_icon(
        popup,
        10,
        9,
        kyber_success_animation[app->animation_counter / kyber_success_animation_mod]);
    popup_set_context(popup, app);
    popup_set_callback(popup, disn3y_toolbox_app_popup_timeout_callback);
    popup_set_timeout(popup, 1500);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewPopup);
    notification_message_block(app->notifications, &sequence_set_green_255);
}

bool disn3y_toolbox_app_scene_kyber_write_success_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack || event.type == SceneManagerEventTypeCustom) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager,
            app->kyber_series == 2 ? Disn3yToolboxAppSceneKyberSelectorS2 :
                                     Disn3yToolboxAppSceneKyberSelectorS1);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->animation_counter == 0) {
            app->animation_counter++;
            app->animation_counter_direction = false;
        } else if(
            app->animation_counter ==
            ((kyber_success_animation_length * kyber_success_animation_mod) - 1)) {
            app->animation_counter_direction = true;
        } else {
            app->animation_counter++;
        }
        popup_set_icon(
            app->popup,
            10,
            9,
            kyber_success_animation[app->animation_counter / kyber_success_animation_mod]);
    }

    return consumed;
}

void disn3y_toolbox_app_scene_kyber_write_success_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    notification_message_block(app->notifications, &sequence_reset_green);
    popup_reset(app->popup);

    app->animation_counter = 0;
    app->animation_counter_direction = false;
}
