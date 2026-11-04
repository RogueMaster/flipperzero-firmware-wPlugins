#include "../sniffer_card_app.h"

static void sniffer_card_app_scene_write_result_button_callback(
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

/** What to say about each way the write can end.
 *
 * One table rather than a chain of branches, because every row needs the same three decisions -
 * headline, explanation, and whether going on from here makes any sense - and a chain lets one
 * of those go missing for one outcome without anything noticing.
 */
static const struct {
    SnifferCardPollerResult result;
    const char* header;
    const char* text;
    bool can_continue;
} sniffer_card_write_outcomes[] = {
    {SnifferCardPollerResultSuccess, "UID set", NULL, true},
    // Text built at the screen rather than stored here, because the useful thing to say is both
    // lengths - which card you are holding and which UID you picked. See the NULL case below.
    {SnifferCardPollerResultUidLenMismatch, "Wrong sniffer card", NULL, false},
    {SnifferCardPollerResultWriteMismatch,
     "Did not take",
     "The card still answers as\nsomething else. Try again,\nholding it still.",
     false},
    // Worth going on from: the write itself reported success and only the read-back is missing,
    // which is what a card lifted off the Flipper too early looks like. Reading the card is how
    // the user finds out either way, and that is the next screen anyway.
    {SnifferCardPollerResultWriteUnverified,
     "Not confirmed",
     "The card did not come back\nto be checked. It may still\nhave worked.",
     true},
    // The write never issues a trace read, so it cannot know whether the tag would answer one.
    // All it has is the SAK, and that is what the text says.
    {SnifferCardPollerResultNotClassic,
     "Not a sniffer card",
     "This tag is not a MIFARE\nClassic. A sniffer card\nemulates one.",
     false},
    // Produced by a trace read rather than a write, so it cannot land here - listed so the table
    // covers the enum and the check below stays true.
    {SnifferCardPollerResultNotSniffer,
     "Not a sniffer card",
     "This tag does not answer\nan unauthenticated read.",
     false},
    {SnifferCardPollerResultError,
     "Failed",
     "The card stopped answering\npart way through.",
     false},
    // Never a real outcome - the screen is only entered once a session has a verdict - but
    // present so the table covers every value and the static assert below can say so.
    {SnifferCardPollerResultWaiting, "Failed", "No result from the card.", false},
};

// The table's whole point is that an outcome cannot go missing unnoticed, and until now the
// lookup fell off the end into the last row instead - rendering an unlisted verdict as whatever
// happened to be last. Checked here so a verdict added later fails the build rather than the user.
_Static_assert(
    COUNT_OF(sniffer_card_write_outcomes) == SnifferCardPollerResultNum,
    "every SnifferCardPollerResult needs a row");

void sniffer_card_app_scene_write_result_on_enter(void* context) {
    SnifferCardApp* app = context;

    size_t row = 0;
    while(row < COUNT_OF(sniffer_card_write_outcomes) &&
          sniffer_card_write_outcomes[row].result != app->result) {
        row++;
    }
    furi_check(row < COUNT_OF(sniffer_card_write_outcomes));

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        sniffer_card_write_outcomes[row].header);

    if(sniffer_card_write_outcomes[row].text) {
        widget_add_string_multiline_element(
            app->widget,
            64,
            SNIFFER_CARD_TEXT_Y,
            AlignCenter,
            AlignTop,
            FontSecondary,
            sniffer_card_write_outcomes[row].text);
    } else {
        const SnifferCardIdentity* identity = sniffer_card_poller_get_identity(app->poller);

        if(app->result == SnifferCardPollerResultUidLenMismatch) {
            // Both lengths, because either one alone leaves the user guessing which to change.
            // These cards are a 4-byte or a 7-byte part and cannot be made into the other, so the
            // answer is always to reach for a different card or a different UID.
            furi_string_printf(
                app->text,
                "This card takes a %u-byte\nUID. The one you chose is\n%u bytes.",
                identity->uid_len,
                app->target.uid_len);
        } else {
            // On success the useful thing to show is the UID the card now answers as, read back
            // off it rather than echoed from what was asked for.
            sniffer_card_app_uid_text(identity->uid, identity->uid_len, app->text);
            // Three lines total, no blank one: a fourth would start under the button row and
            // be painted over by it.
            furi_string_cat_str(app->text, "\nThe sniffer card now\nanswers as the original");
        }

        widget_add_string_multiline_element(
            app->widget,
            64,
            SNIFFER_CARD_TEXT_Y,
            AlignCenter,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(app->text));
    }

    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Retry",
        sniffer_card_app_scene_write_result_button_callback,
        app);

    if(sniffer_card_write_outcomes[row].can_continue) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            "Done",
            sniffer_card_app_scene_write_result_button_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewWidget);
}

bool sniffer_card_app_scene_write_result_on_event(void* context, SceneManagerEvent event) {
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
        // The card's next stop is a reader, which is nothing this app can take part in, so the
        // job ends at the menu rather than leading anywhere.
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SnifferCardAppSceneStart);
        return true;
    }

    return false;
}

void sniffer_card_app_scene_write_result_on_exit(void* context) {
    SnifferCardApp* app = context;
    widget_reset(app->widget);
}
