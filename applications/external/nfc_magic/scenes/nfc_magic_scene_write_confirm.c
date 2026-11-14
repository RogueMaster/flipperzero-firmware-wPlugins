#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_info.h"
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>

void nfc_magic_scene_write_confirm_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_write_confirm_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    const bool iso15693 = (instance->protocol == NfcMagicProtocolIso15693);
    const bool iso15693_wipe = iso15693 && instance->iso15693_mode == NfcMagicIso15693ModeWipe;
    const bool iso15693_write_uid = iso15693 &&
                                    instance->iso15693_mode == NfcMagicIso15693ModeWriteUid;

    // Write UID is the one variant whose body is computed rather than constant. The widget takes a copy
    // of the string it is given, so this is freed before the view switch.
    FuriString* uid_str = furi_string_alloc();

    const char* title = instance->uscuid_ul_is_wipe_mode ? "Wipe card?" : "Risky operation";
    const char* confirm_label = "Continue";
    uint8_t text_height = 54;

    const char* text;
    if(iso15693_write_uid) {
        // 8 spaced bytes overflow the 128px width and wrap, which would push the warning off-screen,
        // so show the UID compactly (two 4-byte groups) on one line. Keep the whole box to 3 short
        // lines (UID + 2 warning): a 4th line gets squashed into the ~38px the buttons leave below
        // the title.
        iso15693_info_cat_uid(uid_str, instance->iso15693_target_uid, Iso15693UidFormatGrouped);
        furi_string_cat_str(uid_str, "\nOnly magic ISO15693\ntags accept this.");
        title = "Write UID?";
        confirm_label = "Write";
        text_height = 38;
        text = furi_string_get_cstr(uid_str);
    } else if(iso15693_wipe) {
        // "Every" is literal: 56/57/62/63 are cleared too, because on a gen2 card they are ordinary
        // user data and sparing them would leave data behind on the common card. On gen1 those same
        // blocks are the backdoor registers, so the string does not promise the UID survives -- the
        // wipe re-reads the UID afterwards and reports a move when it sees one. It cannot report
        // the absence of a move: a card bricked past inventorying goes unreported, which is why
        // uid_verified is a separate field. See the open question in iso15693_poller_wipe_blocks.
        //
        // The gen3 line is the only warning that names the COST; the title above it warns about scope
        // only. The wipe performs NO magic detection -- menu, confirm, sweep -- so this cannot say
        // "your card is gen3", only what a gen3 card would cost, which per 0x6r1an0y (who wrote
        // proxmark's ISO15693 magic V3 support) is the card itself, permanently -- reported on
        // their authority, not tried in testing. A pre-flight probe would go in #255; until then
        // the static line is the whole mitigation.
        //
        // FontSecondary is u8g2_font_haxrcorp4089_tr, and two things follow. No `m` in the suffix means
        // it is PROPORTIONAL (profont11_mr / FontKeyboard is the monospace one), so WHAT BINDS IS PIXEL
        // WIDTH, NOT CHARACTER COUNT -- the three lines below run to 29, 23 and 27 characters against
        // the ~24 an average mix allows, and canvas_string_width is the only way to check one. And the
        // `_r` is u8g2's restricted set, ASCII 32-127, so there is no warning GLYPH to use: the
        // smallest icon, Warning_30x23, is two text lines tall and 30 of the 128px wide. Hence "!".
        //
        // Hard line breaks: elements_text_box wraps on its own, and left to itself it split "including"
        // mid-word.
        //
        // THREE lines is the budget, and that part IS fixed -- the font advances 11px regardless of
        // glyph, so they land at y=13/24/35 and a fourth reaches the button box at rows 52-63.
        // text_height is 38 for the same reason: at the default 54 the box runs to y=67, past the
        // screen, so an over-long string would draw UNDER the button instead of clipping.
        //
        // Set HERE, not from is_wipe: a USCUID-UL wipe is an Ultralight operation, so an ISO15693
        // generation qualifier says nothing about the card in hand.
        title = "Wipe? (gen1/gen2 only)";
        text_height = 38;
        text =
            "Zeroes every block, including\ngen1 magic 56/57/62/63.\nThis can \e#brick\e# a gen3 card!";
    } else if(instance->uscuid_ul_is_wipe_mode) {
        text = "Blank factory dump: config &\npassword cleared, UID zeroed.";
    } else {
        text =
            "Writing to this card will change manufacturer block. On some cards it may not be rewritten";
    }

    widget_add_string_element(widget, 3, 0, AlignLeft, AlignTop, FontPrimary, title);
    widget_add_text_box_element(widget, 0, 13, 128, text_height, AlignLeft, AlignTop, text, false);
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        confirm_label,
        nfc_magic_scene_write_confirm_widget_callback,
        instance);
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", nfc_magic_scene_write_confirm_widget_callback, instance);

    furi_string_free(uid_str);

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_write_confirm_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
            consumed = true;
        }
    }
    return consumed;
}

void nfc_magic_scene_write_confirm_on_exit(void* context) {
    NfcMagicApp* instance = context;

    widget_reset(instance->widget);
}
