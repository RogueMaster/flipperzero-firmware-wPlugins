#include "../sniffer_card_app.h"

static void sniffer_card_app_scene_confirm_uid_button_callback(
    GuiButtonType type,
    InputType input,
    void* context) {
    SnifferCardApp* app = context;

    if(input != InputTypeShort) return;

    if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventRetry);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventNext);
    }
}

void sniffer_card_app_scene_confirm_uid_on_enter(void* context) {
    SnifferCardApp* app = context;

    const bool is_classic = sniffer_card_identity_get_type(&app->target) != MfClassicTypeNum;

    sniffer_card_app_uid_text(app->target.uid, app->target.uid_len, app->text);
    furi_string_cat_printf(
        app->text,
        "\n%s, %u-byte UID",
        sniffer_card_identity_get_type_name(&app->target),
        app->target.uid_len);

    // A third body line and no blank one: three body lines fit above the buttons,
    // and the blank line this used to carry pushed the advice under them, where it was drawn and
    // then painted over.
    if(is_classic) {
        // The UID length is named because it decides which sniffer card can be used at all: these
        // cards are a 4-byte or a 7-byte part, and the write refuses the other.
        furi_string_cat_printf(app->text, "\nUse a %u-byte sniffer card", app->target.uid_len);
    } else {
        furi_string_cat_str(app->text, "\nSniffing needs a Classic");
    }

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        is_classic ? "Original card" : "Wrong card type");
    widget_add_string_multiline_element(
        app->widget,
        64,
        SNIFFER_CARD_TEXT_Y,
        AlignCenter,
        AlignTop,
        FontSecondary,
        furi_string_get_cstr(app->text));

    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Retry",
        sniffer_card_app_scene_confirm_uid_button_callback,
        app);

    if(is_classic) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            "Set UID",
            sniffer_card_app_scene_confirm_uid_button_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewWidget);
}

bool sniffer_card_app_scene_confirm_uid_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SnifferCardAppSceneStart);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == SnifferCardAppCustomEventRetry) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.event == SnifferCardAppCustomEventNext) {
        sniffer_card_poller_set_uid(app->poller, app->target.uid, app->target.uid_len);
        scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneWriteUid);
        return true;
    }

    return false;
}

void sniffer_card_app_scene_confirm_uid_on_exit(void* context) {
    SnifferCardApp* app = context;
    widget_reset(app->widget);
}
