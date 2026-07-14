#include "src/core/cfs_app_i.h"

// Diagnostic view of the current read/loaded tag: authoritative catalog match
// plus the raw field bytes. Lets the user confirm the filament-id prefix rule
// against known physical spools.
void cfs_scene_read_raw_on_enter(void* context) {
    CfsApp* app = context;
    const CfsTagData* d = &app->tag_data;
    Widget* widget = app->widget;
    widget_reset(widget);

    const CfsFilamentInfo* info = cfs_filament_lookup(d->filament_id);
    const char* name = info ? info->name : "(unknown id)";
    const char* type = info ? info->material_type : "?";
    const char* brand = cfs_resolve_brand_name(d);

    // Date: opaque Creality header, shown raw (see cfs_data.h).
    char date_line[32];
    snprintf(date_line, sizeof(date_line), "Date: [%s]", d->date_raw);

    // UID hex.
    char uidstr[24];
    cfs_format_uid_hex(app->uid, app->uid_len, uidstr, sizeof(uidstr));
    if(uidstr[0] == '\0') snprintf(uidstr, sizeof(uidstr), "(none)");

    // Raw decrypted 48-byte payload. For a live read this is the exact tag
    // plaintext (incl. bytes the field parser drops); for a saved record we
    // re-encode the stored fields as a representative payload.
    uint8_t plainbuf[CFS_PLAINTEXT_SIZE];
    const uint8_t* plain;
    if(app->has_tag_plain) {
        plain = app->tag_plain;
    } else {
        cfs_data_encode(&app->tag_data, plainbuf);
        plain = plainbuf;
    }
    char raw_ascii[CFS_PLAINTEXT_SIZE + 1];
    char raw_hex[CFS_PLAINTEXT_SIZE * 2 + 1];
    for(int i = 0; i < CFS_PLAINTEXT_SIZE; i++) {
        char c = (char)plain[i];
        raw_ascii[i] = (c >= 0x20 && c < 0x7F) ? c : '.';
        snprintf(raw_hex + i * 2, 3, "%02X", plain[i]);
    }
    raw_ascii[CFS_PLAINTEXT_SIZE] = '\0';

    char text[512];
    snprintf(
        text,
        sizeof(text),
        "Name: %s\n"
        "Brand: %s\n"
        "Type: %s\n"
        "FilID: %s\n"
        "Vendor: %s\n"
        "Color: %02X%02X%02X\n"
        "Length: %um\n"
        "Serial: %s\n"
        "%s\n"
        "UID: %s\n"
        "Raw%s:\n"
        "%s\n"
        "Hex:\n"
        "%s",
        name,
        brand,
        type,
        d->filament_id,
        d->vendor,
        d->color_r,
        d->color_g,
        d->color_b,
        d->length_m,
        d->serial,
        date_line,
        uidstr,
        app->has_tag_plain ? "" : " (re-encoded)",
        raw_ascii,
        raw_hex);

    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Raw / Details");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, text);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_read_raw_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_read_raw_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}
