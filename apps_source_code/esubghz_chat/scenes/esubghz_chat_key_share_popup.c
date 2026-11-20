#include "../esubghz_chat_i.h"
#include "../helpers/nfc_helpers.h"
#include <nfc/helpers/nfc_data_generator.h>
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

    /* Let the firmware build the card. Its own generator is what the NFC app
     * emulates when a card is added by hand, so this is a card the NFC stack
     * is known to read: every fixed page, the version bytes, the counters and
     * the configuration pages come out exactly as the stack expects, instead
     * of being guessed at here. */
    NfcDevice* generated = nfc_device_alloc();
    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, generated);
    mf_ultralight_copy(dev_data, nfc_device_get_data(generated, NfcProtocolMfUltralight));
    nfc_device_free(generated);

    /* Set the page count rather than trusting the copy to carry it: it arrives
     * as zero, and a card claiming no pages is refused on every page read. */
    dev_data->pages_total = mf_ultralight_get_pages_total(MfUltralightTypeNTAG215);
    dev_data->pages_read = dev_data->pages_total;

    /* Then put the payload in user memory, leaving every other page of the
     * generated card alone. */
    uint8_t* mem = nfc_dev_data_bytes(dev_data);
    size_t payload_len = 0;

    /* write key */
    crypto_ctx_get_key(state->crypto_ctx, mem + NFC_KEY_OFFSET);
    payload_len += (KEY_BITS / 8);

    /* write frequency */
    struct FreqNfcEntry* freq_entry = (struct FreqNfcEntry*)(mem + NFC_FREQ_OFFSET);
    freq_entry->frequency = __htonl(state->frequency);
    freq_entry->magic = __htonl(NFC_FORMAT_MAGIC);
    freq_entry->payload_len = 0;
    freq_entry->unused = 0;
    payload_len += sizeof(struct FreqNfcEntry);

    /* write the replay dict */
    struct ReplayDictNfcWriterContext wr_ctx = {
        .cur = mem + NFC_KEY_OFFSET + payload_len, .max = mem + NFC_KEY_OFFSET + NFC_MAX_BYTES};

    size_t n_entries =
        crypto_ctx_dump_replay_dict(state->crypto_ctx, replay_dict_nfc_writer, &wr_ctx);
    payload_len += n_entries * sizeof(struct ReplayDictNfcEntry);

    freq_entry->payload_len = __htonl((uint32_t)payload_len);
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
