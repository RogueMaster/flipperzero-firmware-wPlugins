#include "../esubghz_chat_i.h"
#include "../helpers/nfc_helpers.h"
#include <machine/endian.h>

typedef enum {
    KeyReadPopupState_Idle,
    KeyReadPopupState_Detecting,
    KeyReadPopupState_Reading,
    KeyReadPopupState_Fail,
    KeyReadPopupState_Success,
} KeyReadPopupState;

/* Callback for the MIFARE Ultralight poller. This runs on the NFC worker
 * thread, so it only copies the card data over and hands control back to the
 * scene through the view dispatcher. */
static NfcCommand key_read_poller_cb(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolMfUltralight);
    furi_assert(event.event_data);

    ESubGhzChatState* state = context;
    MfUltralightPollerEvent* mfu_event = event.event_data;

    NfcCommand cmd = NfcCommandContinue;

    switch(mfu_event->type) {
    /* a card was activated, we only want to read it */
    case MfUltralightPollerEventTypeRequestMode:
        mfu_event->data->poller_mode = MfUltralightPollerModeRead;
        view_dispatcher_send_custom_event(
            state->view_dispatcher, ESubGhzChatEvent_NfcCardDetected);
        break;

    /* the sharing Flipper never sets a password, don't burn auth attempts */
    case MfUltralightPollerEventTypeAuthRequest:
        mfu_event->data->auth_context.skip_auth = true;
        break;

    case MfUltralightPollerEventTypeReadSuccess:
        mf_ultralight_copy(state->nfc_dev_data, nfc_poller_get_data(state->nfc_poller));
        view_dispatcher_send_custom_event(state->view_dispatcher, ESubGhzChatEvent_NfcReadSuccess);
        cmd = NfcCommandStop;
        break;

    case MfUltralightPollerEventTypeReadFailed:
        view_dispatcher_send_custom_event(state->view_dispatcher, ESubGhzChatEvent_NfcReadFailed);
        cmd = NfcCommandStop;
        break;

    default:
        break;
    }

    return cmd;
}

static void key_read_popup_timeout_cb(void* context) {
    furi_assert(context);
    ESubGhzChatState* state = context;

    uint32_t cur_state =
        scene_manager_get_scene_state(state->scene_manager, ESubGhzChatScene_KeyReadPopup);

    /* done displaying our failure */
    if(cur_state == KeyReadPopupState_Fail) {
        view_dispatcher_send_custom_event(
            state->view_dispatcher, ESubGhzChatEvent_KeyReadPopupFailed);
        /* done displaying our success */
    } else if(cur_state == KeyReadPopupState_Success) {
        view_dispatcher_send_custom_event(
            state->view_dispatcher, ESubGhzChatEvent_KeyReadPopupSucceeded);
    }
}

struct ReplayDictNfcReaderContext {
    uint8_t* cur;
    uint8_t* max;
};

static bool replay_dict_nfc_reader(uint64_t* run_id, uint32_t* counter, void* context) {
    struct ReplayDictNfcReaderContext* ctx = (struct ReplayDictNfcReaderContext*)context;

    if(ctx->cur + sizeof(struct ReplayDictNfcEntry) > ctx->max) {
        return false;
    }

    struct ReplayDictNfcEntry* entry = (struct ReplayDictNfcEntry*)ctx->cur;
    *run_id = entry->run_id;
    *counter = __ntohl(entry->counter);

    ctx->cur += sizeof(struct ReplayDictNfcEntry);

    return true;
}

static bool key_read_popup_handle_key_read(ESubGhzChatState* state) {
    MfUltralightData* dev_data = state->nfc_dev_data;

    uint8_t* mem = nfc_dev_data_bytes(dev_data);
    size_t bytes_read = nfc_dev_data_bytes_read(dev_data);

    /* check for config pages */
    if(bytes_read < NFC_CONFIG_PAGES * 4) {
        return false;
    }

    size_t data_read = bytes_read - (NFC_CONFIG_PAGES * 4);

    /* check if key was transmitted */
    if(data_read < KEY_BITS / 8) {
        return false;
    }

    /* initiate the crypto context */
    bool ret = crypto_ctx_set_key(
        state->crypto_ctx, mem + NFC_KEY_OFFSET, state->name_prefix, furi_get_tick());

    /* cleanup */
    crypto_explicit_bzero(mem + NFC_KEY_OFFSET, KEY_BITS / 8);

    if(!ret) {
        crypto_ctx_clear(state->crypto_ctx);
        return false;
    }

    /* read the frequency */
    if(data_read >= NFC_DICT_OFFSET) {
        struct FreqNfcEntry* freq_entry = (struct FreqNfcEntry*)(mem + NFC_FREQ_OFFSET);
        state->frequency = __ntohl(freq_entry->frequency);
    }

    /* read the replay dict */
    struct ReplayDictNfcReaderContext rd_ctx = {
        .cur = mem + NFC_DICT_OFFSET,
        .max = mem + (data_read < NFC_MAX_BYTES ? data_read : NFC_MAX_BYTES)};

    crypto_ctx_read_replay_dict(state->crypto_ctx, replay_dict_nfc_reader, &rd_ctx);

    /* set encrypted flag */
    state->encrypted = true;

    return true;
}

static void key_read_popup_set_state(ESubGhzChatState* state, KeyReadPopupState new_state) {
    uint32_t cur_state =
        scene_manager_get_scene_state(state->scene_manager, ESubGhzChatScene_KeyReadPopup);
    if(cur_state == new_state) {
        return;
    }

    if(new_state == KeyReadPopupState_Detecting) {
        popup_reset(state->nfc_popup);
        popup_disable_timeout(state->nfc_popup);
        popup_set_text(state->nfc_popup, "Tap Flipper\n to sender", 97, 24, AlignCenter, AlignTop);
        popup_set_icon(state->nfc_popup, 0, 8, &I_NFC_manual_60x50);
        notification_message(state->notification, &sequence_blink_start_cyan);
    } else if(new_state == KeyReadPopupState_Reading) {
        popup_reset(state->nfc_popup);
        popup_disable_timeout(state->nfc_popup);
        popup_set_header(
            state->nfc_popup,
            "Reading key\nDon't "
            "move...",
            85,
            24,
            AlignCenter,
            AlignTop);
        popup_set_icon(state->nfc_popup, 12, 23, &I_Loading_24);
        notification_message(state->notification, &sequence_blink_start_yellow);
    } else if(new_state == KeyReadPopupState_Fail) {
        popup_reset(state->nfc_popup);
        popup_set_header(state->nfc_popup, "Failure!", 64, 2, AlignCenter, AlignTop);
        popup_set_text(state->nfc_popup, "Failed\nto read\nkey.", 78, 16, AlignLeft, AlignTop);
        popup_set_icon(state->nfc_popup, 21, 13, &I_Cry_dolph_55x52);

        popup_set_timeout(state->nfc_popup, KEY_READ_POPUP_MS);
        popup_set_context(state->nfc_popup, state);
        popup_set_callback(state->nfc_popup, key_read_popup_timeout_cb);
        popup_enable_timeout(state->nfc_popup);

        notification_message(state->notification, &sequence_blink_stop);
    } else if(new_state == KeyReadPopupState_Success) {
        popup_reset(state->nfc_popup);
        popup_set_header(state->nfc_popup, "Key\nread!", 13, 22, AlignLeft, AlignBottom);
        popup_set_icon(state->nfc_popup, 32, 5, &I_DolphinNice_96x59);

        popup_set_timeout(state->nfc_popup, KEY_READ_POPUP_MS);
        popup_set_context(state->nfc_popup, state);
        popup_set_callback(state->nfc_popup, key_read_popup_timeout_cb);
        popup_enable_timeout(state->nfc_popup);

        notification_message(state->notification, &sequence_success);
        notification_message(state->notification, &sequence_blink_stop);
    }

    scene_manager_set_scene_state(state->scene_manager, ESubGhzChatScene_KeyReadPopup, new_state);

    view_dispatcher_switch_to_view(state->view_dispatcher, ESubGhzChatView_NfcPopup);
}

/* Prepares the key share read scene. */
void scene_on_enter_key_read_popup(void* context) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_enter_key_read_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    nfc_dev_data_clear(state->nfc_dev_data);

    key_read_popup_set_state(state, KeyReadPopupState_Detecting);

    state->nfc = nfc_alloc();
    state->nfc_poller = nfc_poller_alloc(state->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(state->nfc_poller, key_read_poller_cb, state);
}

/* Handles scene manager events for the key read popup scene. */
bool scene_on_event_key_read_popup(void* context, SceneManagerEvent event) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_event_key_read_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        /* card detected */
        case ESubGhzChatEvent_NfcCardDetected:
            key_read_popup_set_state(state, KeyReadPopupState_Reading);
            consumed = true;
            break;

        /* key probably read */
        case ESubGhzChatEvent_NfcReadSuccess:
            if(key_read_popup_handle_key_read(state)) {
                key_read_popup_set_state(state, KeyReadPopupState_Success);
            } else {
                key_read_popup_set_state(state, KeyReadPopupState_Fail);
            }
            consumed = true;
            break;

        /* close the popup and go back */
        case ESubGhzChatEvent_KeyReadPopupFailed:
            if(!scene_manager_previous_scene(state->scene_manager)) {
                view_dispatcher_stop(state->view_dispatcher);
            }
            consumed = true;
            break;

        /* success, go to frequency input */
        case ESubGhzChatEvent_KeyReadPopupSucceeded:
            scene_manager_next_scene(state->scene_manager, ESubGhzChatScene_FreqInput);
            consumed = true;
            break;

        /* reading failed or something else happened, treat as failure */
        case ESubGhzChatEvent_NfcReadFailed:
        default:
            key_read_popup_set_state(state, KeyReadPopupState_Fail);
            consumed = true;
            break;
        }

        break;

    default:
        consumed = false;
        break;
    }

    return consumed;
}

/* Cleans up the key read popup scene. */
void scene_on_exit_key_read_popup(void* context) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_exit_key_read_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    if(state->nfc_poller != NULL) {
        nfc_poller_stop(state->nfc_poller);
        nfc_poller_free(state->nfc_poller);
        state->nfc_poller = NULL;
    }

    if(state->nfc != NULL) {
        nfc_free(state->nfc);
        state->nfc = NULL;
    }

    popup_reset(state->nfc_popup);
    scene_manager_set_scene_state(
        state->scene_manager, ESubGhzChatScene_KeyReadPopup, KeyReadPopupState_Idle);

    notification_message(state->notification, &sequence_blink_stop);

    nfc_dev_data_clear(state->nfc_dev_data);
}
