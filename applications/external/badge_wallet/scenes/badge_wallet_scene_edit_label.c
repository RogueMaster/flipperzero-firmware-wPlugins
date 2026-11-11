#include "../badge_wallet.h"

static void badge_wallet_scene_edit_label_callback(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    /* Commit the edited label to the in-memory card and persist to the sidecar.
     * The .nfc file itself is never touched. */
    BadgeCard* card = &app->cards[app->selected_index];
    furi_string_set(card->label, app->label_buf);
    badge_wallet_save_sidecar(app);

    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void badge_wallet_scene_edit_label_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    BadgeCard* card = &app->cards[app->selected_index];
    strncpy(app->label_buf, furi_string_get_cstr(card->label), sizeof(app->label_buf) - 1);
    app->label_buf[sizeof(app->label_buf) - 1] = '\0';

    text_input_set_header_text(app->text_input, "Enter badge label");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        badge_wallet_scene_edit_label_callback,
        app,
        app->label_buf,
        sizeof(app->label_buf),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewTextInput);
}

bool badge_wallet_scene_edit_label_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        /* Label saved: return to the card menu, which rebuilds with the new
         * label in its header. */
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void badge_wallet_scene_edit_label_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    text_input_reset(app->text_input);
}
