#include "../badge_wallet.h"

static void badge_wallet_uid_to_str(const uint8_t* uid, size_t len, char* out, size_t out_size) {
    size_t pos = 0;
    out[0] = '\0';
    for(size_t i = 0; i < len; i++) {
        /* Need room for "XX", a possible separator, and the terminator. */
        if(pos + 3 >= out_size) break;
        pos += snprintf(out + pos, out_size - pos, "%02X", uid[i]);
        if(i + 1 < len && pos + 1 < out_size) {
            out[pos++] = ' ';
            out[pos] = '\0';
        }
    }
}

void badge_wallet_scene_info_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    BadgeCard* card = &app->cards[app->selected_index];

    widget_reset(app->widget);

    /* Title: the user's label. */
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(card->label));

    /* Parse the .nfc file into a throwaway device to read UID + protocol.
     * No Nfc/HAL is allocated here, so the radio is never engaged. */
    NfcDevice* device = nfc_device_alloc();
    if(nfc_device_load(device, furi_string_get_cstr(card->path))) {
        NfcProtocol protocol = nfc_device_get_protocol(device);
        const char* proto_name =
            (protocol != NfcProtocolInvalid) ? nfc_device_get_protocol_name(protocol) : "Unknown";

        widget_add_string_element(
            app->widget, 4, 22, AlignLeft, AlignTop, FontSecondary, "Protocol:");
        widget_add_string_element(
            app->widget, 4, 32, AlignLeft, AlignTop, FontSecondary, proto_name);

        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(device, &uid_len);
        char uid_str[BADGE_WALLET_UID_MAX_LEN * 3 + 1];
        if(uid && uid_len > 0) {
            if(uid_len > BADGE_WALLET_UID_MAX_LEN) uid_len = BADGE_WALLET_UID_MAX_LEN;
            badge_wallet_uid_to_str(uid, uid_len, uid_str, sizeof(uid_str));
        } else {
            snprintf(uid_str, sizeof(uid_str), "n/a");
        }

        widget_add_string_element(app->widget, 4, 46, AlignLeft, AlignTop, FontSecondary, "UID:");
        widget_add_string_element(app->widget, 4, 56, AlignLeft, AlignTop, FontSecondary, uid_str);
    } else {
        widget_add_string_element(
            app->widget, 64, 34, AlignCenter, AlignTop, FontSecondary, "Failed to read file");
    }
    nfc_device_free(device);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewWidget);
}

bool badge_wallet_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void badge_wallet_scene_info_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    widget_reset(app->widget);
}
