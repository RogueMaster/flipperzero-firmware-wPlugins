#include "../uk_mbirth_sonicare.h"
#include <dolphin/dolphin.h>
//#include <nfc/helpers/protocol_support/nfc_protocol_support_common.h>

void sonicare_scene_read_on_enter(void* context) {
    Sonicare* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Hold brush stem next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    //popup_set_icon(app->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewPopup);
}

bool sonicare_scene_read_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    UNUSED(app);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void sonicare_scene_read_on_exit(void* context) {
    Sonicare* app = context;
    //notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
