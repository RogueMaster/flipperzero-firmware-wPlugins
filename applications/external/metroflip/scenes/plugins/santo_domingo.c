/*
 * Santo Domingo integrated transit (Dominican Republic).
 *
 * Covers the MetroSD / SDGO MIFARE Classic 1K cards used on the Santo Domingo
 * Metro, OMSA buses and the Teleferico. A card carries two independent
 * products: a cash purse in Dominican pesos that is accepted network-wide, and
 * an optional bundle of Metro-only trips that counts down.
 *
 * Layout reverse engineered from a 29-dump differential set covering two cards
 * (issue #93, contributed by @DIVIADAR). The purse and the trip bundle are
 * confirmed against every dump; the per-tap log in sectors 8-11 is only partly
 * understood, so its entries are shown as raw values rather than invented
 * labels - see santo_domingo_add_taps().
 *
 * Keys: sector 0 is openable on every card seen so far (key A all zeroes, key B
 * 76 6E 87 3D 1A 3E), which is enough to recognise the card. Every other sector
 * is diversified per card, so reading the purse and trips needs a key file the
 * user has already recovered for that UID - same situation as SmartRider.
 */

#include "../../metroflip_i.h"

#include <bit_lib.h>
#include <flipper_application.h>
#include <furi.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <string.h>
#include <dolphin/dolphin.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <storage/storage.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:SantoDomingo"

/* Purse: two records that alternate on every transaction. Each carries a
 * 16-bit counter that only ever grows, so the live balance is the record with
 * the higher counter. */
#define SD_PURSE_BLOCK_A 58
#define SD_PURSE_BLOCK_B 60
#define SD_PURSE_SEQ_OFF 4
#define SD_PURSE_VAL_OFF 10

/* The stored amount is 32 units per peso. */
#define SD_PURSE_UNITS_PER_PESO 32

/* Trip bundles: three product slots in sector 6, each 4 bytes
 * (8C <type> <trips remaining> <bundle size * 4>). */
#define SD_SLOT_MAGIC      0x8C
#define SD_SLOT_TYPE_TRIPS 0x04
#define SD_SLOT_TYPE_EMPTY 0x08
#define SD_SLOT_COUNT      3

static const uint8_t sd_slot_blocks[SD_SLOT_COUNT] = {24, 25, 26};

/* Per-tap log: four rotating entries, newest = highest sequence number. */
#define SD_TAP_COUNT   4
#define SD_TAP_SEQ_OFF 14
#define SD_TAP_VAL_OFF 4

static const uint8_t sd_tap_blocks[SD_TAP_COUNT] = {34, 37, 40, 42};

typedef struct {
    bool purse_valid;
    uint16_t purse_seq;
    uint16_t purse_raw;

    bool bundle_valid;
    uint8_t trips_remaining;
    uint8_t bundle_size;

    uint8_t tap_count;
    uint16_t tap_seq[SD_TAP_COUNT];
    uint8_t tap_val[SD_TAP_COUNT][3];
} SantoDomingoData;

/* Sector 0 holds the same issuer signature on every card seen, including the
 * hardened SDGO batch whose other sectors stay locked. */
static bool santo_domingo_signature_ok(const MfClassicData* data) {
    static const uint8_t sig_b1[] = {
        0x35, 0x80, 0x00, 0x00, 0x04, 0xD0, 0xFC, 0x40, 0x10, 0x06, 0x84, 0x03};
    static const uint8_t sig_b2[] = {0x34, 0x3F, 0x10, 0x00};

    if(!mf_classic_is_block_read(data, 1) || !mf_classic_is_block_read(data, 2)) return false;
    if(memcmp(data->block[1].data, sig_b1, sizeof(sig_b1)) != 0) return false;
    /* Bytes 12-13 of block 1 are card specific; 14-15 are part of the signature. */
    if(data->block[1].data[14] != 0x43 || data->block[1].data[15] != 0xF1) return false;
    return memcmp(data->block[2].data, sig_b2, sizeof(sig_b2)) == 0;
}

static void santo_domingo_parse_purse(SantoDomingoData* sd, const MfClassicData* data) {
    const uint8_t blocks[2] = {SD_PURSE_BLOCK_A, SD_PURSE_BLOCK_B};
    for(size_t i = 0; i < COUNT_OF(blocks); i++) {
        if(!mf_classic_is_block_read(data, blocks[i])) continue;
        const uint8_t* b = data->block[blocks[i]].data;
        uint16_t seq = bit_lib_bytes_to_num_be(b + SD_PURSE_SEQ_OFF, 2);
        /* Keep the newer of the two records. */
        if(sd->purse_valid && seq <= sd->purse_seq) continue;
        sd->purse_valid = true;
        sd->purse_seq = seq;
        sd->purse_raw = bit_lib_bytes_to_num_be(b + SD_PURSE_VAL_OFF, 2);
    }
}

static void santo_domingo_parse_bundle(SantoDomingoData* sd, const MfClassicData* data) {
    for(size_t i = 0; i < SD_SLOT_COUNT; i++) {
        uint8_t blk = sd_slot_blocks[i];
        if(!mf_classic_is_block_read(data, blk)) continue;
        const uint8_t* b = data->block[blk].data;
        if(b[0] != SD_SLOT_MAGIC || b[1] != SD_SLOT_TYPE_TRIPS) continue;

        /* Two slots hold the same bundle and alternate on each tap, so the
         * stale one still shows the pre-tap count. Trips only ever count
         * down within a bundle, so the lower of the two is current. A reload
         * writes a fresh higher count to one slot first, and until the next
         * tap rewrites the other this will report the old, lower figure. */
        if(sd->bundle_valid && b[2] >= sd->trips_remaining) continue;
        sd->bundle_valid = true;
        sd->trips_remaining = b[2];
        sd->bundle_size = b[3] / 4;
    }
}

static void santo_domingo_parse_taps(SantoDomingoData* sd, const MfClassicData* data) {
    for(size_t i = 0; i < SD_TAP_COUNT; i++) {
        uint8_t blk = sd_tap_blocks[i];
        if(!mf_classic_is_block_read(data, blk)) continue;
        const uint8_t* b = data->block[blk].data;
        uint16_t seq = bit_lib_bytes_to_num_be(b + SD_TAP_SEQ_OFF, 2);
        if(seq == 0) continue;

        uint8_t pos = sd->tap_count;
        /* Insert newest first. */
        while(pos > 0 && sd->tap_seq[pos - 1] < seq) {
            if(pos < SD_TAP_COUNT) {
                sd->tap_seq[pos] = sd->tap_seq[pos - 1];
                memcpy(sd->tap_val[pos], sd->tap_val[pos - 1], 3);
            }
            pos--;
        }
        if(pos >= SD_TAP_COUNT) continue;
        sd->tap_seq[pos] = seq;
        memcpy(sd->tap_val[pos], b + SD_TAP_VAL_OFF, 3);
        if(sd->tap_count < SD_TAP_COUNT) sd->tap_count++;
    }
}

static bool santo_domingo_parse(SantoDomingoData* sd, const MfClassicData* data) {
    if(data->type != MfClassicType1k) {
        FURI_LOG_E(TAG, "Not a 1K card");
        return false;
    }
    if(!santo_domingo_signature_ok(data)) {
        FURI_LOG_E(TAG, "Sector 0 signature missing");
        return false;
    }

    memset(sd, 0, sizeof(*sd));
    santo_domingo_parse_purse(sd, data);
    santo_domingo_parse_bundle(sd, data);
    santo_domingo_parse_taps(sd, data);
    return true;
}

static void santo_domingo_add_taps(View* view, const SantoDomingoData* sd) {
    if(sd->tap_count == 0) return;

    /* Each entry carries a 3-byte field that changes on every tap but does not
     * track the dump dates in the sample set, so it is shown raw instead of
     * being presented as a date. */
    uint8_t p = metroflip_card_view_add_page(view, "Recent Taps");
    if(p == UINT8_MAX) return;

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];
    char label[METROFLIP_CARD_VIEW_HEADER_LEN];
    for(uint8_t i = 0; i < sd->tap_count; i++) {
        snprintf(label, sizeof(label), "Tap %u", (unsigned)(i + 1));
        snprintf(
            val,
            sizeof(val),
            "#%u  %02X%02X%02X",
            (unsigned)(sd->tap_seq[i] / 16),
            sd->tap_val[i][0],
            sd->tap_val[i][1],
            sd->tap_val[i][2]);
        metroflip_card_view_add_field(view, p, label, val, false);
    }
}

static bool
    santo_domingo_display_card_view(const MfClassicData* data, Metroflip* app, bool from_file) {
    SantoDomingoData sd;
    if(!santo_domingo_parse(&sd, data)) return false;

    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Santo Domingo");

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");

    metroflip_card_view_add_field(view, p, "Network", "Santo Domingo", false);

    if(data->iso14443_3a_data && data->iso14443_3a_data->uid_len > 0) {
        size_t uid_len = data->iso14443_3a_data->uid_len;
        const uint8_t* uid = data->iso14443_3a_data->uid;
        size_t written = 0;
        for(size_t i = 0; i < uid_len && written + 2 < sizeof(val); i++) {
            written += snprintf(val + written, sizeof(val) - written, "%02X", uid[i]);
        }
        metroflip_card_view_add_field(view, p, "Serial", val, false);
    }

    if(sd.purse_valid) {
        /* 32 units per peso, shown to the centavo. */
        uint32_t centavos = ((uint32_t)sd.purse_raw * 100) / SD_PURSE_UNITS_PER_PESO;
        snprintf(
            val,
            sizeof(val),
            "RD$ %lu.%02lu",
            (unsigned long)(centavos / 100),
            (unsigned long)(centavos % 100));
        metroflip_card_view_add_field(view, p, "Balance", val, true);
    } else {
        metroflip_card_view_add_field(view, p, "Balance", "Locked", false);
    }

    if(sd.bundle_valid) {
        snprintf(
            val, sizeof(val), "%u of %u", (unsigned)sd.trips_remaining, (unsigned)sd.bundle_size);
        metroflip_card_view_add_field(view, p, "Metro Trips", val, true);
    }

    /* Fares, so a balance means something without looking them up. */
    p = metroflip_card_view_add_page(view, "Fares");
    metroflip_card_view_add_field(view, p, "Metro", "RD$ 20.00", false);
    metroflip_card_view_add_field(view, p, "OMSA Bus", "RD$ 35.00", false);
    metroflip_card_view_add_field(view, p, "Teleferico", "RD$ 35.00", false);
    if(sd.purse_valid) {
        uint32_t centavos = ((uint32_t)sd.purse_raw * 100) / SD_PURSE_UNITS_PER_PESO;
        snprintf(val, sizeof(val), "%lu", (unsigned long)(centavos / 2000));
        metroflip_card_view_add_field(view, p, "Metro rides", val, false);
    }

    santo_domingo_add_taps(view, &sd);

    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static uint8_t santo_domingo_sector_num = 0;

static NfcCommand santo_domingo_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &uid_len);

        char uid_str[uid_len * 2 + 1];
        uid_to_string(uid, uid_len, uid_str, sizeof(uid_str));
        /* Sectors 0 (signature), 6 (trip bundles), 14 and 15 (purse). */
        uint64_t santo_domingo_key_mask_a_required = 0xC041;
        KeyfileManager manage = manage_keyfiles(
            uid_str, uid, uid_len, app->mfc_key_cache, santo_domingo_key_mask_a_required, 0);

        char card_type[] = "Santo Domingo";

        switch(manage) {
        case MISSING_KEYFILE: {
            FuriString* parsed_data = furi_string_alloc();
            handle_keyfile_case(app, "No keys found", "Missing keyfile", parsed_data, card_type);
            furi_string_free(parsed_data);
            command = NfcCommandStop;
            break;
        }
        case INCOMPLETE_KEYFILE: {
            FuriString* parsed_data = furi_string_alloc();
            handle_keyfile_case(
                app, "Incomplete keyfile", "incomplete keyfile", parsed_data, card_type);
            furi_string_free(parsed_data);
            command = NfcCommandStop;
            break;
        }
        case SUCCESSFUL:
            mf_classic_key_cache_load(app->mfc_key_cache, uid, uid_len);
            break;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicKey key = {};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        if(mf_classic_key_cache_get_next_key(
               app->mfc_key_cache, &santo_domingo_sector_num, &key, &key_type)) {
            mfc_event->data->read_sector_request_data.sector_num = santo_domingo_sector_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
        } else {
            mfc_event->data->read_sector_request_data.key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        /* Building the card view has to happen on the main thread. */
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void santo_domingo_show_unknown(Metroflip* app) {
    Widget* widget = app->widget;
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, "\e#Unknown card\n");
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
}

static void santo_domingo_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    mf_classic_key_cache_reset(app->mfc_key_cache);
    santo_domingo_sector_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            if(!santo_domingo_display_card_view(mfc_data, app, true)) {
                santo_domingo_show_unknown(app);
            }
            mf_classic_free(mfc_data);
        } else {
            FURI_LOG_E(TAG, "Failed to open saved file: %s", app->file_path);
            Widget* widget = app->widget;
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, "\e#Error\nFailed to open\nsaved file.");
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
    } else {
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, santo_domingo_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool santo_domingo_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            metroflip_app_blink_stop(app);
            dolphin_deed(DolphinDeedNfcReadSuccess);
            const MfClassicData* mfc_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
            if(!santo_domingo_display_card_view(mfc_data, app, false)) {
                santo_domingo_show_unknown(app);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card found!\nDon't move...", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card lost!\nTry again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void santo_domingo_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin santo_domingo_plugin = {
    .card_name = "Santo Domingo",
    .plugin_on_enter = santo_domingo_on_enter,
    .plugin_on_event = santo_domingo_on_event,
    .plugin_on_exit = santo_domingo_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor santo_domingo_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &santo_domingo_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* santo_domingo_plugin_ep(void) {
    return &santo_domingo_plugin_descriptor;
}
