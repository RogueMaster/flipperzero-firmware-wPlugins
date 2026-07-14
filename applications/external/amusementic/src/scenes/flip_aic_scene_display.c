#include "../helpers/aic.h"
#include "../helpers/hex.h"
#include "../helpers/cardio.h"
#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <protocols/felica/felica.h>

enum FlipAICSceneDisplayEvent {
    FlipAICSceneDisplayEventMenu,
};

static void flip_aic_scene_display_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    switch(result) {
    case DialogExResultLeft:
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipAICSceneDisplayEventMenu);
        break;

    case DialogExResultCenter:
        if(cardio_is_connected()) {
            const FelicaData* data = nfc_device_get_data(app->nfc_device, NfcProtocolFelica);
            cardio_send_report(CardioReportIdFeliCa, data->idm.data);
        }
        break;

    default:
        break;
    }
}

void flip_aic_scene_display_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    const FelicaData* data = nfc_device_get_data(app->nfc_device, NfcProtocolFelica);

    AICVendor vendor = aic_vendor(data);
    const char* vendor_name = aic_vendor_name_short(vendor);
    dialog_ex_set_header(app->dialog_ex, vendor_name, 0, 0, AlignLeft, AlignTop);

    FuriString* text = furi_string_alloc();

    uint8_t access_code[10];
    aic_access_code(data, access_code);
    furi_string_cat_str(text, "Access code:\n");
    furi_string_cat_hex(text, access_code, sizeof(access_code), " ", 2);

    furi_string_cat_str(text, "\nIDm: ");
    furi_string_cat_hex(text, data->idm.data, sizeof(data->idm.data), ":", 1);

    // TODO: make another page for this? (it overflows)
    // furi_string_cat_str(text, "\nPMm: ");
    // furi_string_cat_hex(text, data->pmm.data, sizeof(data->pmm.data), ":", 1);

    dialog_ex_set_text(app->dialog_ex, furi_string_get_cstr(text), 0, 12, AlignLeft, AlignTop);
    furi_string_free(text);

    dialog_ex_set_left_button_text(app->dialog_ex, "Menu");
    dialog_ex_set_center_button_text(app->dialog_ex, "CardIO");

    dialog_ex_set_result_callback(app->dialog_ex, flip_aic_scene_display_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewDialogEx);
}

bool flip_aic_scene_display_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FlipAICSceneDisplayEventMenu:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipAICSceneMenu);
            return true;
        }
    }

    return false;
}

void flip_aic_scene_display_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    dialog_ex_reset(app->dialog_ex);
}
