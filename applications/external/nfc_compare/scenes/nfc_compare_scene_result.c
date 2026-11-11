#include "../nfc_compare.h"

static void nfc_compare_uid_to_str(const uint8_t* uid, size_t len, char* out, size_t out_size) {
    size_t pos = 0;
    out[0] = '\0';
    for(size_t i = 0; i < len; i++) {
        // Need room for "XX" plus a possible separator and the terminator.
        if(pos + 3 >= out_size) break;
        pos += snprintf(out + pos, out_size - pos, "%02X", uid[i]);
        if(i + 1 < len && pos + 1 < out_size) {
            out[pos++] = ' ';
            out[pos] = '\0';
        }
    }
}

void nfc_compare_scene_result_on_enter(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;

    bool match = (app->saved_uid_len > 0) && (app->saved_uid_len == app->read_uid_len) &&
                 (memcmp(app->saved_uid, app->read_uid, app->saved_uid_len) == 0);
    app->match = match;

    char saved_str[NFC_COMPARE_UID_MAX_LEN * 3 + 1];
    char read_str[NFC_COMPARE_UID_MAX_LEN * 3 + 1];
    nfc_compare_uid_to_str(app->saved_uid, app->saved_uid_len, saved_str, sizeof(saved_str));
    nfc_compare_uid_to_str(app->read_uid, app->read_uid_len, read_str, sizeof(read_str));

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, match ? "MATCH" : "NO MATCH");

    char line[48];
    snprintf(line, sizeof(line), "Saved: %s", saved_str);
    widget_add_string_element(app->widget, 2, 28, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "Card:  %s", read_str);
    widget_add_string_element(app->widget, 2, 42, AlignLeft, AlignTop, FontSecondary, line);
    widget_add_string_element(
        app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary, "Back to choose again");

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcCompareViewWidget);

    notification_message(
        app->notifications, match ? &sequence_set_green_255 : &sequence_set_red_255);
}

bool nfc_compare_scene_result_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_compare_scene_result_on_exit(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;
    notification_message(app->notifications, &sequence_reset_rgb);
    widget_reset(app->widget);
}
