#include "../sniffer_card_app.h"

// The two lengths these cards are made in. Nothing else is offered, because the write refuses a
// UID of a different length from the sniffer card's own and a 5-byte entry could only ever fail.
static const uint8_t sniffer_card_uid_lengths[] = {4, 7};

static void sniffer_card_app_scene_uid_length_callback(void* context, uint32_t index) {
    SnifferCardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sniffer_card_app_scene_uid_length_on_enter(void* context) {
    SnifferCardApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "UID length");

    for(size_t i = 0; i < COUNT_OF(sniffer_card_uid_lengths); i++) {
        furi_string_printf(app->text, "%u bytes", sniffer_card_uid_lengths[i]);
        // submenu_add_item copies the label, so the shared string can be reused each time round.
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(app->text),
            i,
            sniffer_card_app_scene_uid_length_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewSubmenu);
}

bool sniffer_card_app_scene_uid_length_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= COUNT_OF(sniffer_card_uid_lengths)) return false;

    const uint8_t uid_len = sniffer_card_uid_lengths[event.event];

    // Start the editor from whatever is in hand when it is the right length, so coming back to
    // correct a digit does not mean typing the whole UID again.
    if(app->uid_input_len != uid_len) {
        memset(app->uid_input, 0, sizeof(app->uid_input));
    }
    app->uid_input_len = uid_len;

    scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneEnterUid);

    return true;
}

void sniffer_card_app_scene_uid_length_on_exit(void* context) {
    SnifferCardApp* app = context;
    submenu_reset(app->submenu);
}
