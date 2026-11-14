#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"

// Shown mid-write when the gen2 backdoor left the UID unchanged (not a gen2 magic card, or not magic
// at all). Offers the destructive gen1 fallback as an explicit opt-in. Nothing
// has been written to the card yet, so declining leaves it untouched; accepting re-runs the write in
// gen1 mode (NfcMagicSceneWrite reads iso15693_force_gen1 and iso15693_mode on enter).
//
// Reached from two flows, distinguished by this scene's state (NfcMagicIso15693Gen1OptinSource): a
// clone, which goes on to write every data block, and a bare Write-UID, which writes only the four
// gen1 registers. They consent to different things, so the body text differs; both resume in the
// same write scene.
static void nfc_magic_scene_iso15693_gen1_optin_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_iso15693_gen1_optin_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    widget_add_string_element(
        widget, 3, 0, AlignLeft, AlignTop, FontPrimary, "Not gen2 magic card");

    const bool from_write_uid =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneIso15693Gen1Optin) ==
        NfcMagicIso15693Gen1OptinFromWriteUid;

    FuriString* body = furi_string_alloc();
    // NEITHER body says how well gen1 is validated, and that is deliberate -- do not add it. What
    // the user consents to here is the write and its blast radius, which both bodies state. How
    // well the path is validated is not something they can act on at this moment, and on a consent
    // screen the one thing a reassurance can change is whether they say yes.
    if(from_write_uid) {
        // A bare Write-UID has no source data to write, so gen1 touches only the four registers --
        // but on a plain tag those are ordinary data blocks, which is the whole risk here.
        furi_string_cat_str(
            body,
            "Might be gen1, or not magic at all. Gen1 sets the UID by writing blocks 56/57/62/63 "
            "with an ordinary write, which any writable tag accepts -- so on a tag that isn't magic "
            "this overwrites whatever those 4 blocks hold. Nothing else is written.");
    } else {
        furi_string_cat_str(
            body,
            "Might be gen1, or not magic at all. Gen1 writes blocks 56/57/62/63 first to set the "
            "UID, then the rest of the data only if that UID takes. A non-magic tag loses at most "
            "those 4 blocks.");
        // If the source itself stores data in those backdoor blocks, gen1 can't reproduce it -- warn
        // at the decision point rather than on the up-front confirm, which a clone never reaches.
        // Clone only: a Write-UID has no source file.
        const Iso15693_3Data* source =
            nfc_device_get_data(instance->source_dev, NfcProtocolIso15693_3);
        if(iso15693_poller_source_uses_gen1_blocks(source)) {
            furi_string_cat_str(
                body, "\n\nYour file's data in 56/57/62/63 can't be cloned by gen1.");
        }
    }
    // Scrolling body (Up/Down) so the full warning always fits alongside the button row.
    widget_add_text_scroll_element(widget, 0, 14, 128, 37, furi_string_get_cstr(body));
    furi_string_free(body);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Back",
        nfc_magic_scene_iso15693_gen1_optin_button_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Try gen1",
        nfc_magic_scene_iso15693_gen1_optin_button_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_gen1_optin_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            // Opt in: re-run the write in gen1 mode. The gen2 attempt wrote nothing, so this is the
            // first thing to touch the card.
            instance->iso15693_force_gen1 = true;
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            // Decline: leave the card untouched, back to the ISO15693 menu.
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneIso15693);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneIso15693);
    }
    return consumed;
}

void nfc_magic_scene_iso15693_gen1_optin_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
