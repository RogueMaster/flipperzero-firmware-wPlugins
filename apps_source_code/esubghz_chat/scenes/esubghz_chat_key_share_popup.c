#include "../esubghz_chat_i.h"
#include "../helpers/nfc_helpers.h"
#include <machine/endian.h>

struct ReplayDictNfcWriterContext {
    uint8_t* cur;
    uint8_t* max;
};

static bool replay_dict_nfc_writer(uint64_t run_id, uint32_t counter, void* context) {
    struct ReplayDictNfcWriterContext* ctx = (struct ReplayDictNfcWriterContext*)context;

    struct ReplayDictNfcEntry entry = {.run_id = run_id, .counter = __htonl(counter), .unused = 0};

    if(ctx->cur + sizeof(entry) > ctx->max) {
        return false;
    }

    memcpy(ctx->cur, &entry, sizeof(entry));
    ctx->cur += sizeof(entry);

    return true;
}

/* The MIFARE Ultralight listener does not need anything from us, but it is
 * given a callback rather than a NULL pointer so an unexpected event cannot
 * dereference one. */
static NfcCommand key_share_listener_cb(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);

    return NfcCommandContinue;
}

static void prepare_nfc_dev_data(ESubGhzChatState* state) {
    MfUltralightData* dev_data = state->nfc_dev_data;

    nfc_dev_data_clear(dev_data);

    Iso14443_3aData* iso14443_3a_data = dev_data->iso14443_3a_data;
    furi_hal_random_fill_buf(iso14443_3a_data->uid, ISO14443_3A_UID_7_BYTES);
    iso14443_3a_data->uid_len = ISO14443_3A_UID_7_BYTES;
    iso14443_3a_data->atqa[0] = 0x44;
    iso14443_3a_data->atqa[1] = 0x00;
    iso14443_3a_data->sak = 0x00;

    dev_data->type = MfUltralightTypeNTAG215;
    dev_data->version.header = 0x00;
    dev_data->version.vendor_id = 0x04;
    dev_data->version.prod_type = 0x04;
    dev_data->version.prod_subtype = 0x02;
    dev_data->version.prod_ver_major = 0x01;
    dev_data->version.prod_ver_minor = 0x00;
    dev_data->version.storage_size = 0x11;
    dev_data->version.protocol_type = 0x03;

    uint8_t* mem = nfc_dev_data_bytes(dev_data);
    size_t data_written = 0;

    /* write key */
    crypto_ctx_get_key(state->crypto_ctx, mem + NFC_KEY_OFFSET);
    data_written += (KEY_BITS / 8);

    /* write frequency */
    struct FreqNfcEntry* freq_entry = (struct FreqNfcEntry*)(mem + data_written);
    freq_entry->frequency = __htonl(state->frequency);
    freq_entry->unused1 = 0;
    freq_entry->unused2 = 0;
    freq_entry->unused3 = 0;
    data_written += sizeof(struct FreqNfcEntry);

    /* write the replay dict */
    struct ReplayDictNfcWriterContext wr_ctx = {
        .cur = mem + data_written, .max = mem + NFC_MAX_BYTES};

    size_t n_entries =
        crypto_ctx_dump_replay_dict(state->crypto_ctx, replay_dict_nfc_writer, &wr_ctx);
    data_written += n_entries * sizeof(struct ReplayDictNfcEntry);

    /* The reading side strips NFC_CONFIG_PAGES trailing pages, so expose the
     * payload plus that many empty pages and nothing else. The poller stops
     * once the listener NAKs the page after them. */
    dev_data->pages_total = (data_written / MF_ULTRALIGHT_PAGE_SIZE) + NFC_CONFIG_PAGES;
    dev_data->pages_read = dev_data->pages_total;
}

/* Prepares the key share popup scene. */
void scene_on_enter_key_share_popup(void* context) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_enter_key_share_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    popup_reset(state->nfc_popup);

    popup_disable_timeout(state->nfc_popup);

    popup_set_header(state->nfc_popup, "Sharing...", 67, 13, AlignLeft, AlignTop);
    popup_set_icon(state->nfc_popup, 0, 3, &I_NFC_dolphin_emulation_47x61);
    popup_set_text(state->nfc_popup, "Sharing\nKey via\nNFC", 90, 28, AlignCenter, AlignTop);

    prepare_nfc_dev_data(state);

    state->nfc = nfc_alloc();
    /* the listener keeps its own copy of the card data */
    state->nfc_listener =
        nfc_listener_alloc(state->nfc, NfcProtocolMfUltralight, state->nfc_dev_data);
    nfc_listener_start(state->nfc_listener, key_share_listener_cb, state);

    /* our copy of the key is not needed any more */
    nfc_dev_data_clear(state->nfc_dev_data);

    notification_message(state->notification, &sequence_blink_start_magenta);

    view_dispatcher_switch_to_view(state->view_dispatcher, ESubGhzChatView_NfcPopup);
}

/* Handles scene manager events for the key share popup scene. */
bool scene_on_event_key_share_popup(void* context, SceneManagerEvent event) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_event_key_share_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    UNUSED(state);
    UNUSED(event);

    return false;
}

/* Cleans up the key share popup scene. */
void scene_on_exit_key_share_popup(void* context) {
    FURI_LOG_T(APPLICATION_NAME, "scene_on_exit_key_share_popup");

    furi_assert(context);
    ESubGhzChatState* state = context;

    if(state->nfc_listener != NULL) {
        nfc_listener_stop(state->nfc_listener);
        nfc_listener_free(state->nfc_listener);
        state->nfc_listener = NULL;
    }

    if(state->nfc != NULL) {
        nfc_free(state->nfc);
        state->nfc = NULL;
    }

    popup_reset(state->nfc_popup);

    notification_message(state->notification, &sequence_blink_stop);

    nfc_dev_data_clear(state->nfc_dev_data);
}
