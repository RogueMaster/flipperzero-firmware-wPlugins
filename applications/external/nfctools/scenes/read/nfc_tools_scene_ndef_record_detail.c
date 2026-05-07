#include "../../include/nfc_tools_i.h"

// ── TNF labels ────────────────────────────────────────────────────────────────

static const char* tnf_label(uint8_t tnf) {
    switch(tnf) {
    case 0x00: return "Empty";
    case 0x01: return "NFC Well-Known";
    case 0x02: return "MIME";
    case 0x03: return "Absolute URI";
    case 0x04: return "External";
    case 0x05: return "Unknown";
    case 0x06: return "Unchanged";
    case 0x07: return "Reserved";
    default:   return "?";
    }
}

static const char* type_label(NfcToolsNdefType t) {
    switch(t) {
    case NfcToolsNdefTypeUri:         return "URI";
    case NfcToolsNdefTypeText:        return "Text";
    case NfcToolsNdefTypeWifi:        return "WiFi Config";
    case NfcToolsNdefTypeVcard:       return "vCard";
    case NfcToolsNdefTypeSmartPoster: return "Smart Poster";
    case NfcToolsNdefTypeMime:        return "MIME";
    case NfcToolsNdefTypeEmpty:       return "Empty";
    case NfcToolsNdefTypeExternal:    return "External";
    default:                          return "Unknown";
    }
}

// ── Detail text construction ──────────────────────────────────────────────────

static void build_record_detail(NfcToolsApp* app) {
    const NfcToolsNdefRecord* rec = &app->ndef_records[app->ndef_selected_record];
    furi_string_reset(app->info_str);

    // Record number
    furi_string_cat_printf(
        app->info_str,
        "Record %u / %u\n",
        (unsigned)(app->ndef_selected_record + 1),
        (unsigned)app->ndef_record_count);

    // Type
    furi_string_cat_printf(app->info_str, "Type: %s\n", type_label(rec->type));

    // Format (TNF)
    furi_string_cat_printf(app->info_str, "Format: %s\n", tnf_label(rec->tnf));

    // Type string brut
    if(rec->type_str[0]) {
        furi_string_cat_printf(app->info_str, "ID: %s\n", rec->type_str);
    }

    // Value
    if(rec->value[0]) {
        furi_string_cat_printf(app->info_str, "Value:\n%s\n", rec->value);
    }

    // Payload size
    furi_string_cat_printf(
        app->info_str, "Payload: %u bytes\n", (unsigned)rec->payload_len);

    // Raw hex (full payload, 8 bytes per line)
    furi_string_cat_str(app->info_str, NTS_LBL_RAW);
    size_t show = rec->payload_len < NFC_TOOLS_NDEF_PAYLOAD_MAX
                      ? rec->payload_len
                      : NFC_TOOLS_NDEF_PAYLOAD_MAX;
    for(size_t i = 0; i < show; i++) {
        furi_string_cat_printf(app->info_str, "%02X ", rec->payload[i]);
        if((i + 1) % 8 == 0) furi_string_cat_str(app->info_str, "\n");
    }
}

// ── Callback bouton Widget ────────────────────────────────────────────────────

static void nfc_tools_record_detail_btn_cb(GuiButtonType result, InputType type, void* context) {
    NfcToolsApp* app = context;
    if(result == GuiButtonTypeRight && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
    }
}

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_ndef_record_detail_on_enter(void* context) {
    NfcToolsApp* app = context;
    const NfcToolsNdefRecord* rec = &app->ndef_records[app->ndef_selected_record];

    build_record_detail(app);

    if(rec->has_qr) {
        // Widget: scrollable text + "QR" button on the right
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget, 0, 0, 128, 52,
            furi_string_get_cstr(app->info_str));
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            NTS_BTN_QR_CODE,
            nfc_tools_record_detail_btn_cb,
            app);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewWidget);
    } else {
        // Simple TextBox: no button needed
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_text(app->text_box, furi_string_get_cstr(app->info_str));
        text_box_set_focus(app->text_box, TextBoxFocusStart);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
    }
}

bool nfc_tools_scene_ndef_record_detail_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            // QR button pressed
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdNdefQrCode);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_ndef_record_detail_on_exit(void* context) {
    NfcToolsApp* app = context;
    const NfcToolsNdefRecord* rec = &app->ndef_records[app->ndef_selected_record];
    if(rec->has_qr) {
        widget_reset(app->widget);
    } else {
        text_box_reset(app->text_box);
    }
}
