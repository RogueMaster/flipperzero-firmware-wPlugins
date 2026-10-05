/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The keys the read derived.
 *
 * Everything the chip sent after it was opened travelled encrypted under
 * KSenc and signed under KSmac, and both come out of a password the holder
 * typed and a nonce the holder's own document chose. They are the holder's
 * data in the plainest sense, so the reader shows them rather than using them
 * silently and throwing them away.
 *
 * They are shown and nowhere else. Nothing on this screen reaches the export,
 * the trace or the settings file: an APDU trace on the card next to the keys
 * that decrypt it would turn an export into the session in the clear.
 */
#include "../emrtd_i.h"

#include "../crypto/emrtd_crypto.h"

/*
 * Eight bytes to the line. The lines carry \e*, which is what puts them in the
 * monospaced font, and twenty of those characters is what fits across the
 * screen - the same budget the file preview works to.
 */
#define EMRTD_KEYS_PER_LINE 8

/** One key as hex, under its own heading, in lines the screen can hold. */
static void emrtd_scene_result_keys_append(
    FuriString* body,
    const char* heading,
    const uint8_t* bytes,
    size_t len) {
    furi_string_cat_printf(body, "\e#%s\n", heading);

    for(size_t offset = 0; offset < len; offset += EMRTD_KEYS_PER_LINE) {
        furi_string_cat_str(body, "\e*");
        for(size_t i = 0; i < EMRTD_KEYS_PER_LINE && offset + i < len; i++) {
            furi_string_cat_printf(body, "%02X", bytes[offset + i]);
        }
        furi_string_cat_str(body, "\n");
    }
}

void emrtd_scene_result_keys_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdSessionKeys* keys = &app->result.keys;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    /*
     * The cipher comes from the session and not from what EF.CardAccess
     * announced: a chip may advertise PACE and still be opened by the BAC
     * fallback, and the keys on this screen belong to whichever one ran.
     */
    const size_t key_size = emrtd_cipher_key_size(keys->cipher);
    const size_t block_size = emrtd_cipher_block_size(keys->cipher);

    furi_string_cat_printf(
        body,
        "\e#Session\n%s, %s\n",
        app->result.access.protocol[0] != '\0' ? app->result.access.protocol : "opened",
        emrtd_cipher_name(keys->cipher));

    emrtd_scene_result_keys_append(body, "KSenc", keys->ks_enc, key_size);
    emrtd_scene_result_keys_append(body, "KSmac", keys->ks_mac, key_size);
    emrtd_scene_result_keys_append(body, "SSC at the start", keys->ssc, block_size);

    furi_string_cat_str(
        body,
        "\e#What these are\nEverything the chip sent\nafter it opened was\n"
        "encrypted under KSenc and\nsigned under KSmac. Both\ncome from what you typed\n"
        "and this chip's answer to\nit, so they are yours.\n"
        "\nThe counter is the one the\nsession started from; it\nmoved with every command.\n"
        "\n\e#Where they are not\nThey are not written to the\ncard - not in the report,\n"
        "not in the trace - and they\ngo when this app closes.\n");

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Keys");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_keys_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_keys_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);

    /*
     * furi_string_reset() frees the buffer holding the keys as hex, and so
     * does widget_reset() with the widget's copy. On every firmware this is
     * built for, the allocator clears a block when it is freed
     * (docs/platform.md, item 23). The overwrite below repeats that for this
     * buffer only. On a build without the clear, the widget's copy and the
     * smaller buffers this string outgrew while it was being built would
     * stay.
     */
    const size_t len = furi_string_size(app->text_box_store);
    for(size_t i = 0; i < len; i++) {
        furi_string_set_char(app->text_box_store, i, '0');
    }
    furi_string_reset(app->text_box_store);
}
