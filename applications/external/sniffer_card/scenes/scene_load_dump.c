#include "../sniffer_card_app.h"

#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

/** Where the NFC app keeps saved cards, and the extension it saves them under. */
#define TAG "SnifferCard"

#define SNIFFER_CARD_DUMP_FOLDER    SNIFFER_CARD_NONCE_LOG_FOLDER
#define SNIFFER_CARD_DUMP_EXTENSION ".nfc"

/** Take the UID, and the SAK if there is one, out of a saved card.
 *
 * nfc_device_get_uid() answers for any protocol, so a dump of something that is not a Classic
 * still yields a UID - and is still refused later, by the same check that refuses a tag which is
 * not a Classic. The SAK is only reachable through the protocol's own data, so a dump of another
 * protocol simply has none and reads as "not Classic" on the screen that follows.
 */
static bool sniffer_card_app_load_identity(const char* path, SnifferCardIdentity* identity) {
    NfcDevice* device = nfc_device_alloc();
    bool loaded = false;

    do {
        if(!nfc_device_load(device, path)) break;

        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(device, &uid_len);
        if(uid == NULL || uid_len == 0 || uid_len > sizeof(identity->uid)) break;

        memset(identity, 0, sizeof(*identity));
        memcpy(identity->uid, uid, uid_len);
        identity->uid_len = uid_len;

        if(nfc_device_get_protocol(device) == NfcProtocolMfClassic) {
            const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);
            if(data && data->iso14443_3a_data) {
                identity->sak = iso14443_3a_get_sak(data->iso14443_3a_data);
            }
        }

        loaded = true;
    } while(false);

    nfc_device_free(device);

    return loaded;
}

void sniffer_card_app_scene_load_dump_on_enter(void* context) {
    SnifferCardApp* app = context;

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, SNIFFER_CARD_DUMP_EXTENSION, NULL);
    options.base_path = SNIFFER_CARD_DUMP_FOLDER;
    options.hide_dot_files = true;

    FuriString* path = furi_string_alloc_set(SNIFFER_CARD_DUMP_FOLDER);
    bool picked = false;

    // Choosing a file that turns out to be unusable is not the same as choosing no file, and
    // collapsing the two into one silent exit leaves the user looking at the menu they just came
    // from with nothing to explain why - so they pick the same file again.
    if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
        picked = sniffer_card_app_load_identity(furi_string_get_cstr(path), &app->target);
        if(!picked) {
            FURI_LOG_E(TAG, "No usable UID in %s", furi_string_get_cstr(path));
            dialog_message_show_storage_error(app->dialogs, "No UID in that file");
        }
    }

    furi_string_free(path);

    // The browser is modal and has already returned, so this scene has nothing to draw. It either
    // moves on with a UID in hand or steps back to the source menu - either way it never becomes
    // the visible scene, which is why there is no view_dispatcher_switch_to_view here.
    if(picked) {
        scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneConfirmUid);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool sniffer_card_app_scene_load_dump_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void sniffer_card_app_scene_load_dump_on_exit(void* context) {
    UNUSED(context);
}
