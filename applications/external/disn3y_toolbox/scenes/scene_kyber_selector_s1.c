#include <disn3y_toolbox_icons.h>
#include <furi.h>

#include "../data/kyber_crystals.h"
#include "../disn3y_toolbox_app.h"

static void disn3y_toolbox_app_scene_kyber_selector_s1_confirm_dialog_callback(
    DialogExResult result,
    void* context) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void kyber_selector_s1_update_status_text(Disn3yToolboxApp* app) {
    DialogEx* dialog_ex = app->dialog_ex;

    dialog_ex_set_header(dialog_ex, "Kyber Crystal Series 1", 0, 0, AlignLeft, AlignTop);

    FuriString* selection = app->selection_string;
    furi_string_reset(selection);

    CrystalSeries1 crystal = CrystalsSeries1[app->selected_s1];

    furi_string_cat_printf(selection, "\n");
    if(strcmp(crystal.hilt_color, crystal.blade_color) == 0) {
        furi_string_cat_printf(selection, "Color: %s\n", crystal.hilt_color);
    } else {
        furi_string_cat_printf(
            selection, "Blade: %s   Hilt: %s\n", crystal.blade_color, crystal.hilt_color);
    }
    furi_string_cat_printf(selection, "Jedi: %s\n", crystal.jedi_voice);
    furi_string_cat_printf(selection, "Sith: %s\n", crystal.sith_voice);
    furi_string_cat_printf(selection, "Wayfinder Location: %s\n", crystal.wayfinder_location);

    dialog_ex_set_text(dialog_ex, furi_string_get_cstr(selection), 0, 25, AlignLeft, AlignCenter);

    dialog_ex_set_left_button_text(
        dialog_ex, app->selected_s1 > CrystalSeries1_0C00 ? "Prev" : NULL);
    dialog_ex_set_right_button_text(
        dialog_ex, app->selected_s1 < CrystalSeries1_MAX - 1 ? "Next" : NULL);
    dialog_ex_set_center_button_text(dialog_ex, "Write");

    dialog_ex_set_result_callback(
        dialog_ex, disn3y_toolbox_app_scene_kyber_selector_s1_confirm_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);
}

void disn3y_toolbox_app_scene_kyber_selector_s1_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    app->kyber_series = 1;

    kyber_selector_s1_update_status_text(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewDialog);
}

bool disn3y_toolbox_app_scene_kyber_selector_s1_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            if(app->selected_s1 > CrystalSeries1_0C00) {
                app->selected_s1--;
                kyber_selector_s1_update_status_text(app);
            }
            return true;
        } else if(event.event == DialogExResultRight) {
            if(app->selected_s1 < CrystalSeries1_MAX - 1) {
                app->selected_s1++;
                kyber_selector_s1_update_status_text(app);
            }
            return true;
        } else if(event.event == DialogExResultCenter) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberWrite);
            return true;
        }
    }
    return false;
}

void disn3y_toolbox_app_scene_kyber_selector_s1_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
