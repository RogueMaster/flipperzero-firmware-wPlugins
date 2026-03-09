#include "../seader_i.h"

static const char* seader_scene_read_card_type_label(SeaderCredentialType type) {
    switch(type) {
    case SeaderCredentialType14A:
        return "Read as 14443A";
    case SeaderCredentialTypeMifareClassic:
        return "Read as Mifare Classic";
    case SeaderCredentialTypePicopass:
        return "Read as Picopass";
    default:
        return "Read";
    }
}

void seader_scene_read_card_type_submenu_callback(void* context, uint32_t index) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, index);
}

void seader_scene_read_card_type_on_enter(void* context) {
    Seader* seader = context;
    Submenu* submenu = seader->submenu;

    submenu_reset(submenu);
    for(size_t i = 0; i < seader->detected_card_type_count; i++) {
        const SeaderCredentialType type = seader->detected_card_types[i];
        submenu_add_item(
            submenu,
            seader_scene_read_card_type_label(type),
            type,
            seader_scene_read_card_type_submenu_callback,
            seader);
    }

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewMenu);
}

bool seader_scene_read_card_type_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const SeaderCredentialType type = event.event;
        if(type == SeaderCredentialType14A || type == SeaderCredentialTypeMifareClassic ||
           type == SeaderCredentialTypePicopass) {
            seader->selected_read_type = type;
            seader->detected_card_type_count = 0;
            memset(seader->detected_card_types, 0, sizeof(seader->detected_card_types));
            scene_manager_next_scene(seader->scene_manager, SeaderSceneRead);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        seader->selected_read_type = SeaderCredentialTypeNone;
        seader->detected_card_type_count = 0;
        memset(seader->detected_card_types, 0, sizeof(seader->detected_card_types));
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }

    return consumed;
}

void seader_scene_read_card_type_on_exit(void* context) {
    Seader* seader = context;
    submenu_reset(seader->submenu);
}
