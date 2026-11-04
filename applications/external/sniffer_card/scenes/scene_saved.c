#include "../sniffer_card_app.h"

static void sniffer_card_app_scene_saved_button_callback(
    GuiButtonType type,
    InputType input,
    void* context) {
    SnifferCardApp* app = context;

    if(input != InputTypeShort || type != GuiButtonTypeRight) return;

    view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventNext);
}

void sniffer_card_app_scene_saved_on_enter(void* context) {
    SnifferCardApp* app = context;

    SnifferCardNonceLogResult result = {0};
    const bool saved = sniffer_card_nonce_log_save(app->traces, app->trace_count, &result);
    const bool have_mfkey = app->mfkey_installed;

    const char* header;
    widget_reset(app->widget);

    if(!saved) {
        // Which failure it was, because they call for different things: one is usually a missing
        // or full SD card, the other a write that went wrong once the log was already open.
        header = "Not saved";
        furi_string_printf(
            app->text,
            result.open_failed ? "Could not open the log.\nCheck the SD card." :
                                 "The write failed part way.\nNothing was added.");
        notification_message(app->notifications, &sequence_error);

    } else if(result.pairs_written == 0) {
        // Everything this card holds is already waiting for MFKey. Said plainly, because these
        // cards keep their captures until a reader overwrites them, so reading one twice is easy
        // to do by accident and would otherwise look like a failure.
        header = "Already saved";
        furi_string_printf(
            app->text,
            "%u pair%s already in the\nMFKey nonce log. Nothing\nnew to add.",
            result.pairs_duplicate,
            result.pairs_duplicate == 1 ? "" : "s");
        notification_message(app->notifications, &sequence_success);

    } else {
        header = "Saved";
        furi_string_printf(
            app->text,
            "%u pair%s added to the\nMFKey nonce log.\n",
            result.pairs_written,
            result.pairs_written == 1 ? "" : "s");
        // qUnleashed is named either way. On the Flipper a pair takes minutes, and the same work
        // on a PC is seconds - so it is the better option often enough that hiding it behind "no
        // MFKey installed" would be hiding the faster route from the people who have both.
        furi_string_cat_str(
            app->text,
            have_mfkey ? "Or qUnleashed, much faster." :
                         "Recover with MFKey, or\nqUnleashed on a PC.");
        notification_message(app->notifications, &sequence_success);
    }

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_string_multiline_element(
        app->widget,
        64,
        SNIFFER_CARD_TEXT_Y,
        AlignCenter,
        AlignTop,
        FontSecondary,
        furi_string_get_cstr(app->text));

    // Offered whenever there is anything in the log worth running it on, which includes the case
    // where this read added nothing because the pairs were already there.
    if(have_mfkey && saved) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            "Run MFKey",
            sniffer_card_app_scene_saved_button_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewWidget);
}

bool sniffer_card_app_scene_saved_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    // The nonces are written and the next step is in another app, so there is nothing left to go
    // back through: Back returns to the menu rather than to the result screen and the read screen
    // under it.
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SnifferCardAppSceneStart);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == SnifferCardAppCustomEventNext) {
        sniffer_card_app_run_mfkey(app);
        return true;
    }

    return false;
}

void sniffer_card_app_scene_saved_on_exit(void* context) {
    SnifferCardApp* app = context;
    widget_reset(app->widget);
}
