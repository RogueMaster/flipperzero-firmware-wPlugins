#include "../sniffer_card_app.h"

/** Carries the action the button was built for, which the screen decided when it chose the label.
 *
 * The right button is Save on some readings of a card and Run MFKey on others, so a handler told
 * only "right was pressed" has to work out which - and working it out means re-pairing every
 * capture and reading the whole nonce log again, to reach the answer this screen already had.
 */
static void sniffer_card_app_scene_read_result_button_callback(
    GuiButtonType type,
    InputType input,
    void* context) {
    SnifferCardApp* app = context;

    if(input != InputTypeShort) return;

    if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SnifferCardAppCustomEventRetry);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            scene_manager_get_scene_state(app->scene_manager, SnifferCardAppSceneReadResult));
    }
}

/** Line prefixes the text scroll element understands, each applying to the line it opens.
 *
 * `\e#` switches that line to FontPrimary and `\e*` to FontKeyboard, which is the only monospaced
 * font available - and the only way to get columns that line up, since the body font is
 * proportional (`widget_element_text_scroll_process_ctrl_symbols()` in the firmware).
 */
#define SNIFFER_CARD_LINE_BOLD "\e#"
#define SNIFFER_CARD_LINE_MONO "\e*"

/** Spell out what was read: the counts, then the captures as a table.
 *
 * Each row carries its tag nonce. Sector and key type are not enough to tell two captures apart -
 * a reader working one sector produces a column of identical-looking rows, and a reader that
 * tries several keys against that sector, real and not, produces them with different keys behind
 * them. The nonce is what differs, and it is also what shows the list is not repeating itself.
 *
 * The UID is a row of its own rather than a column, because every capture of one read normally
 * shares it - and when it does not, because the card was reprogrammed between taps, a change of
 * UID is worth more than repeating thirty-two identical bits down the side.
 */
static void sniffer_card_app_scene_read_result_build_text(
    SnifferCardApp* app,
    const SnifferCardNonceLogResult* pairs) {
    // Two bold lines: what came off the card, and where that leaves the log.
    furi_string_printf(
        app->text,
        SNIFFER_CARD_LINE_BOLD "%u capture%s\n",
        app->trace_count,
        app->trace_count == 1 ? "" : "s");

    if(pairs->pairs_written > 0 && pairs->pairs_duplicate > 0) {
        furi_string_cat_printf(
            app->text,
            SNIFFER_CARD_LINE_BOLD "%u new, %u in log\n",
            pairs->pairs_written,
            pairs->pairs_duplicate);
    } else if(pairs->pairs_written > 0) {
        furi_string_cat_printf(
            app->text,
            SNIFFER_CARD_LINE_BOLD "%u new pair%s\n",
            pairs->pairs_written,
            pairs->pairs_written == 1 ? "" : "s");
    } else if(pairs->pairs_duplicate > 0) {
        // Said as a state rather than as a zero, because "0 new pairs" read as a failure.
        furi_string_cat_printf(
            app->text,
            SNIFFER_CARD_LINE_BOLD "All %u pair%s in log\n",
            pairs->pairs_duplicate,
            pairs->pairs_duplicate == 1 ? "" : "s");
    } else {
        furi_string_cat_str(app->text, SNIFFER_CARD_LINE_BOLD "No pairs yet\n");
    }

    if(app->read_truncated) {
        // Captures were left on the card. Said before the unpaired advice, because the fix is
        // the opposite one: read again rather than tap a reader again.
        furi_string_cat_str(app->text, "Read cut short, read again\n");
    }

    if(pairs->log_unchecked) {
        // The counts above were worked out without seeing the log, so "new" is a guess.
        furi_string_cat_str(app->text, "Log unreadable, may repeat\n");
    }

    if(pairs->traces_unpaired > 0) {
        // The one shortfall another tap on the reader fixes: a capture whose key nothing else
        // here shares cannot be cracked, however good it is.
        furi_string_cat_printf(app->text, "%u unpaired, tap again\n", pairs->traces_unpaired);
    }

    // No column header. Two digits, a letter and eight hex digits say what they are, and on a
    // 64-pixel screen the row it would cost is the difference between a short card fitting
    // without scrolling and not.
    furi_string_cat_str(app->text, "\n");

    uint32_t shown_cuid = 0;
    bool have_shown_cuid = false;

    for(size_t i = 0; i < app->trace_count; i++) {
        const SnifferTrace* trace = &app->traces[i];
        const uint32_t cuid = sniffer_trace_get_cuid(trace);

        if(!have_shown_cuid || cuid != shown_cuid) {
            furi_string_cat_printf(app->text, "UID: %08lX\n", cuid);
            shown_cuid = cuid;
            have_shown_cuid = true;
        }

        furi_string_cat_printf(
            app->text,
            SNIFFER_CARD_LINE_MONO "%02u  %c   %08lX\n",
            sniffer_trace_get_sector(trace),
            trace->key_type == SNIFFER_CARD_AUTH_KEY_A ? 'A' : 'B',
            trace->nt);
    }
}

void sniffer_card_app_scene_read_result_on_enter(void* context) {
    SnifferCardApp* app = context;

    widget_reset(app->widget);

    if(app->result != SnifferCardPollerResultSuccess) {
        // Two reasons a tag is not one of these cards, and they call for different cards: one is
        // not a Classic at all, the other is an ordinary Classic. Saying "does not answer an
        // unauthenticated read" about an Ultralight is simply false - it does answer, which is
        // why the SAK is tested first.
        const bool not_classic = app->result == SnifferCardPollerResultNotClassic;
        widget_add_string_element(
            app->widget, 64, 12, AlignCenter, AlignTop, FontPrimary, "Not a sniffer card");
        widget_add_string_multiline_element(
            app->widget,
            64,
            26,
            AlignCenter,
            AlignTop,
            FontSecondary,
            not_classic ? "This tag is not a MIFARE\nClassic. A sniffer card\nemulates one." :
                          "This tag does not answer\nan unauthenticated read");
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Retry",
            sniffer_card_app_scene_read_result_button_callback,
            app);
    } else if(app->trace_count == 0) {
        // A read that broke off has left captures on the card unread, and the usual advice -
        // go and collect more - would overwrite them, since the card keeps captures only until
        // the next reader writes over them. The app's own misreading would be what destroys the
        // evidence, so a truncated read must never reach that sentence.
        const bool truncated = app->read_truncated;
        widget_add_string_element(
            app->widget,
            64,
            12,
            AlignCenter,
            AlignTop,
            FontPrimary,
            truncated ? "Read cut short" : "Nothing captured");
        widget_add_string_multiline_element(
            app->widget,
            64,
            26,
            AlignCenter,
            AlignTop,
            FontSecondary,
            truncated ?
                "The card stopped answering.\nHold it still and read\nagain - do not re-tap." :
                "Hold the card on a reader\n2-3 times, then read again");
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Retry",
            sniffer_card_app_scene_read_result_button_callback,
            app);
    } else {
        SnifferCardNonceLogResult pairs;
        sniffer_card_nonce_log_count(app->traces, app->trace_count, &pairs);
        sniffer_card_app_scene_read_result_build_text(app, &pairs);

        // Scrollable, because eight captures and their headline do not fit on four lines.
        widget_add_text_scroll_element(
            app->widget, 0, 0, 128, 52, furi_string_get_cstr(app->text));

        // Saving is only offered when there is something to save. With everything already in the
        // log the useful move is the next one, so the button becomes that instead of a Save that
        // would report "nothing new to add" - and when there is nothing to run either, no button
        // is better than one that does nothing.
        if(pairs.pairs_written > 0) {
            scene_manager_set_scene_state(
                app->scene_manager,
                SnifferCardAppSceneReadResult,
                SnifferCardAppCustomEventSaveNonces);
            widget_add_button_element(
                app->widget,
                GuiButtonTypeRight,
                "Save",
                sniffer_card_app_scene_read_result_button_callback,
                app);
        } else if(pairs.pairs_duplicate > 0 && app->mfkey_installed) {
            scene_manager_set_scene_state(
                app->scene_manager,
                SnifferCardAppSceneReadResult,
                SnifferCardAppCustomEventRunMfkey);
            widget_add_button_element(
                app->widget,
                GuiButtonTypeRight,
                "Run MFKey",
                sniffer_card_app_scene_read_result_button_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewWidget);
}

bool sniffer_card_app_scene_read_result_on_event(void* context, SceneManagerEvent event) {
    SnifferCardApp* app = context;

    // Back leaves for the menu rather than falling into the read screen behind this one. Landing
    // there would silently start another read, which is not what Back means anywhere else - and
    // it would leave the explicit Retry button with nothing of its own to do.
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SnifferCardAppSceneStart);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == SnifferCardAppCustomEventRetry) {
        // Back to the read screen rather than forward to another copy of it, so a run of retries
        // does not grow the scene stack.
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.event == SnifferCardAppCustomEventSaveNonces) {
        scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneSaved);
        return true;
    }

    if(event.event == SnifferCardAppCustomEventRunMfkey) {
        sniffer_card_app_run_mfkey(app);
        return true;
    }

    return false;
}

void sniffer_card_app_scene_read_result_on_exit(void* context) {
    SnifferCardApp* app = context;
    widget_reset(app->widget);
}
