#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_i.h"
#include <datetime.h>
#include <dolphin/dolphin.h>
#include <notification/notification_messages.h>
#include <locale/locale.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#define TAG "Metroflip:Scene:Calypso"

bool beginning = true;

// SELECT APPLICATION by partial AID (Calypso RID) - used when CLA 0x94 is rejected
static const uint8_t calypso_aid_select[] = {
    0x00,
    0xA4,
    0x04,
    0x00,
    0x05, // CLA=00 INS=A4 P1=04(by name) P2=00 Lc=05
    0xA0,
    0x00,
    0x00,
    0x04,
    0x04 // Calypso RID
};

// SELECT APPLICATION by full Navigo AID - for new Navigo variants that reject partial AID
static const uint8_t calypso_navigo_aid_select[] = {
    0x00,
    0xA4,
    0x04,
    0x00,
    0x0A, // CLA=00 INS=A4 P1=04(by name) P2=00 Lc=0A
    0xA0,
    0x00,
    0x00,
    0x04,
    0x04, // Calypso RID
    0x01,
    0x25,
    0x09,
    0x01,
    0x01 // Navigo PIX
};

char* build_hex_string(BitBuffer* rx_buffer) {
    static char output[29 * 3 + 1]; // 3 chars per byte + null terminator
    uint8_t byte;
    char* p = output;

    // Only read bytes that actually exist in the response, but always emit
    // exactly 29 byte-pairs (zero-padded) so the save-file record width
    // stays fixed (read_calypso_data expects 29 bytes per record).
    size_t available = bit_buffer_get_size_bytes(rx_buffer);
    if(available > 29) available = 29;

    for(size_t i = 0; i < 29; i++) {
        byte = (i < available) ? bit_buffer_get_byte(rx_buffer, i) : 0x00;
        snprintf(p, 4, "%02X ", byte); // 2 chars + null terminator
        p += 3;
    }

    *p = '\0'; // just being extra careful (should already be null-terminated)
    return output;
}

void prepare_file_data(
    Metroflip* app,
    const char* app_id,
    const char* file_id,
    BitBuffer* rx_buffer) {
    if(beginning) {
        furi_string_reset(app->calypso_file_data);
        furi_string_cat_printf(
            app->calypso_file_data, "Version: 1\nDevice Type: Calypso\nCard Type: calypso\n");
        beginning = false;
    }
    char* hex_string = build_hex_string(rx_buffer);
    furi_string_cat_printf(
        app->calypso_file_data, "AID %s FID %s: %s\n", app_id, file_id, hex_string);
}

FlipperFormat* load_file(Metroflip* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FURI_LOG_I(TAG, "path: %s", app->delete_file_path);
    if(!flipper_format_file_open_existing(ff, app->delete_file_path)) {
        FURI_LOG_I(TAG, "error opening file");
    }
    return ff;
}

void close_file(FlipperFormat* ff) {
    // Balance load_file(): it furi_record_open(RECORD_STORAGE)s and
    // flipper_format_file_alloc()s on every call. Without freeing the format
    // and closing the record here, file-replay (which calls load_file once per
    // record read) leaks a FlipperFormat per record and imbalances the storage
    // record refcount.
    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

int select_new_app(
    int new_app_directory,
    int new_app,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4bPoller* iso14443_4b_poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    if(!app->data_loaded) {
        select_app[5] = new_app_directory;
        select_app[6] = new_app;

        bit_buffer_reset(tx_buffer);
        bit_buffer_reset(rx_buffer);
        bit_buffer_append_bytes(tx_buffer, select_app, sizeof(select_app));
        FURI_LOG_D(
            TAG,
            "SEND %02x %02x %02x %02x %02x %02x %02x %02x",
            select_app[0],
            select_app[1],
            select_app[2],
            select_app[3],
            select_app[4],
            select_app[5],
            select_app[6],
            select_app[7]);
        int error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4bErrorNone) {
            FURI_LOG_I(TAG, "Select File: iso14443_4b_poller_send_block error %d", error);
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            return error;
        }
    }
    return 0;
}

int read_new_file(
    const char* app_id,
    const char* FID,
    int new_file,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4bPoller* iso14443_4b_poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    if(!app->data_loaded) {
        FURI_LOG_I(TAG, "No data loaded");
        read_file[2] = new_file;
        bit_buffer_reset(tx_buffer);
        bit_buffer_reset(rx_buffer);
        bit_buffer_append_bytes(tx_buffer, read_file, sizeof(read_file));
        FURI_LOG_D(
            TAG,
            "SEND %02x %02x %02x %02x %02x",
            read_file[0],
            read_file[1],
            read_file[2],
            read_file[3],
            read_file[4]);
        Iso14443_4bError error =
            iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4bErrorNone) {
            FURI_LOG_I(TAG, "Read File: iso14443_4b_poller_send_block error %d", error);
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            return error;
        }
        // Only persist the record once the status word is known-good. Failed
        // reads (e.g. a 2-byte 6A83 response) must not be hex-dumped into the
        // save file as record data. The caller's check_response() still runs
        // afterwards and handles the failure path (stage + event) as before.
        size_t length = bit_buffer_get_size_bytes(rx_buffer);
        if(length >= 2 && bit_buffer_get_byte(rx_buffer, length - 2) == apdu_success[0] &&
           bit_buffer_get_byte(rx_buffer, length - 1) == apdu_success[1]) {
            prepare_file_data(app, app_id, FID, rx_buffer);
        }
        return 0;
    } else {
        FlipperFormat* ff = load_file(app);

        uint8_t* file_data = read_calypso_data(ff, app_id, FID);
        FURI_LOG_I(TAG, "reading calypso data..");
        if(!file_data) {
            close_file(ff);
            FURI_LOG_E(TAG, "error reading");
            return 1;
        }
        bit_buffer_reset(rx_buffer);
        bit_buffer_append_bytes(rx_buffer, file_data, 29);
        close_file(ff);
        free(file_data);
        return 0;
    }
}

int check_response(
    BitBuffer* rx_buffer,
    Metroflip* app,
    MetroflipPollerEventType* stage,
    size_t* response_length) {
    *response_length = bit_buffer_get_size_bytes(rx_buffer);
    if(!app->data_loaded) { // automatic success
        if(*response_length < 2) {
            FURI_LOG_E(TAG, "Response too short: %zu byte(s)", *response_length);
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            return 1;
        }
        if(bit_buffer_get_byte(rx_buffer, *response_length - 2) != apdu_success[0] ||
           bit_buffer_get_byte(rx_buffer, *response_length - 1) != apdu_success[1]) {
            int error_code_1 = bit_buffer_get_byte(rx_buffer, *response_length - 2);
            int error_code_2 = bit_buffer_get_byte(rx_buffer, *response_length - 1);
            FURI_LOG_E(
                TAG, "Select profile app/file failed: %02x%02x", error_code_1, error_code_2);
            if(error_code_1 == 0x6a && error_code_2 == 0x82) {
                FURI_LOG_E(TAG, "Wrong parameter(s) P1-P2 - File not found");
            } else if(error_code_1 == 0x69 && error_code_2 == 0x82) {
                FURI_LOG_E(TAG, "Command not allowed - Security status not satisfied");
            }
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFileNotFound);
            return 1;
        }
    }
    return 0;
}

/* ── Card view helpers ── */

/* Add every "Label: value" line of parsed text as a card-view field; lines
 * without a separator become plain text fields. Leading spaces are trimmed
 * and Balance lines are bolded. */
static void calypso_add_text_as_fields(View* view, uint8_t page, FuriString* text) {
    const char* s = furi_string_get_cstr(text);
    while(*s) {
        const char* nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) : strlen(s);
        while(len > 0 && *s == ' ') {
            s++;
            len--;
        }
        if(len > 0) {
            char line[64];
            if(len > sizeof(line) - 1) len = sizeof(line) - 1;
            memcpy(line, s, len);
            line[len] = '\0';
            char* sep = strstr(line, ": ");
            if(sep) {
                *sep = '\0';
                bool highlight = (strcmp(line, "Balance") == 0);
                metroflip_card_view_add_field(view, page, line, sep + 2, highlight);
            } else {
                metroflip_card_view_add_field(view, page, "", line, false);
            }
        }
        if(!nl) break;
        s = nl + 1;
    }
}

/* Build and show the card view from the parsed card data in
 * app->calypso_context. Must run on the main thread (called from on_enter
 * in file-replay mode or from the PollerSuccess handler in live mode). */
static void calypso_display_card_view(Metroflip* app) {
    CalypsoContext* ctx = app->calypso_context;
    if(!ctx || !ctx->card) return;
    CalypsoCardData* card = ctx->card;

    View* view = metroflip_card_view_alloc(app);
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];
    char header[METROFLIP_CARD_VIEW_HEADER_LEN];

    switch(card->card_type) {
    case CALYPSO_CARD_NAVIGO: {
        if(!card->navigo) break;
        metroflip_card_view_set_title(view, "Navigo");

        /* Page: Environment */
        uint8_t p = metroflip_card_view_add_page(view, "Environment");
        if(card->card_number != 0) {
            snprintf(val, sizeof(val), "%u", card->card_number);
            metroflip_card_view_add_field(view, p, "Number", val, false);
        } else {
            /* Paper / anonymous ticket - no ICC serial */
            metroflip_card_view_add_field(view, p, "", "Anonymous ticket", false);
        }
        FuriString* str = furi_string_alloc();
        show_navigo_environment_info(&card->navigo->environment, &card->navigo->holder, str);
        calypso_add_text_as_fields(view, p, str);
        furi_string_free(str);

        /* Pages: one per present contract (absent slots are skipped) */
        for(int i = 0; i < 4; i++) {
            if(!card->navigo->contracts[i].present) continue;
            snprintf(header, sizeof(header), "Contract %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            /* show_navigo_contract_info renders the remaining-tickets
             * counter info (Remaining Tickets / Last load) when present */
            show_navigo_contract_info(&card->navigo->contracts[i], str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }

        /* Pages: one per parsed event */
        int events = card->events_count;
        if(events > 3) events = 3;
        for(int i = 0; i < events; i++) {
            snprintf(header, sizeof(header), "Event %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_navigo_event_info(&card->navigo->events[i], card->navigo->contracts, str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }

        /* Pages: one per special event */
        int special = card->special_events_count;
        if(special > 3) special = 3;
        for(int i = 0; i < special; i++) {
            snprintf(header, sizeof(header), "Special Ev. %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_navigo_special_event_info(&card->navigo->special_events[i], str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }
        break;
    }
    case CALYPSO_CARD_OPUS: {
        if(!card->opus) break;
        metroflip_card_view_set_title(view, "Opus");

        /* Page: Environment */
        uint8_t p = metroflip_card_view_add_page(view, "Environment");
        if(card->card_number != 0) {
            snprintf(val, sizeof(val), "%u", card->card_number);
            metroflip_card_view_add_field(view, p, "Number", val, false);
        }
        FuriString* str = furi_string_alloc();
        show_opus_environment_info(&card->opus->environment, &card->opus->holder, str);
        calypso_add_text_as_fields(view, p, str);
        furi_string_free(str);

        /* Pages: one per present contract */
        for(int i = 0; i < 4; i++) {
            if(!card->opus->contracts[i].present) continue;
            snprintf(header, sizeof(header), "Contract %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_opus_contract_info(&card->opus->contracts[i], str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }

        /* Pages: one per parsed event */
        int events = card->events_count;
        if(events > 3) events = 3;
        for(int i = 0; i < events; i++) {
            snprintf(header, sizeof(header), "Event %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_opus_event_info(&card->opus->events[i], card->opus->contracts, str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }
        break;
    }
    case CALYPSO_CARD_RAVKAV: {
        if(!card->ravkav) break;
        metroflip_card_view_set_title(view, "RavKav");

        /* Page: Environment (incl. stored-value balance) */
        uint8_t p = metroflip_card_view_add_page(view, "Environment");
        if(card->card_number == 0) {
            metroflip_card_view_add_field(view, p, "", "Anonymous Rav-Kav", false);
        } else {
            snprintf(val, sizeof(val), "%u", card->card_number);
            metroflip_card_view_add_field(view, p, "Number", val, false);
        }
        if(card->ravkav->contracts[0].present && card->ravkav->contracts[0].balance != 0.0f) {
            uint32_t agorot = (uint32_t)(card->ravkav->contracts[0].balance * 100.0f + 0.5f);
            snprintf(
                val,
                sizeof(val),
                "%lu.%02lu ILS",
                (unsigned long)(agorot / 100),
                (unsigned long)(agorot % 100));
            metroflip_card_view_add_field(view, p, "Balance", val, true);
        }
        FuriString* str = furi_string_alloc();
        show_ravkav_environment_info(&card->ravkav->environment, str);
        calypso_add_text_as_fields(view, p, str);
        furi_string_free(str);

        /* Pages: one per present contract (only slot 1 is ever read) */
        for(int i = 0; i < 4; i++) {
            if(!card->ravkav->contracts[i].present) continue;
            snprintf(header, sizeof(header), "Contract %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_ravkav_contract_info(&card->ravkav->contracts[i], str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }

        /* Pages: events (the parse keeps the historical fixed count of 3) */
        int events = card->events_count;
        if(events > 3) events = 3;
        for(int i = 0; i < events; i++) {
            snprintf(header, sizeof(header), "Event %d", i + 1);
            p = metroflip_card_view_add_page(view, header);
            str = furi_string_alloc();
            show_ravkav_event_info(&card->ravkav->events[i], str);
            calypso_add_text_as_fields(view, p, str);
            furi_string_free(str);
        }
        break;
    }
    default: {
        /* Mobib / Viva / KorriGo / other or unknown Calypso networks:
         * single page mirroring the old one-page view. */
        const char* network = get_network_string(card->card_type);
        metroflip_card_view_set_title(view, network);

        uint8_t p = metroflip_card_view_add_page(view, "Card Info");
        snprintf(val, sizeof(val), "%u", card->card_number);
        metroflip_card_view_add_field(view, p, "Number", val, false);
        metroflip_card_view_add_field(
            view, p, "Country", get_country_string(card->country_num), false);
        if(card->card_type != CALYPSO_CARD_UNKNOWN) {
            metroflip_card_view_add_field(view, p, "Network", network, false);
        } else {
            snprintf(val, sizeof(val), "%d", card->network_num);
            metroflip_card_view_add_field(view, p, "Network", val, false);
        }
        break;
    }
    }

    /* Buttons: OK saves a live read, deletes a loaded file */
    if(app->data_loaded) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
}

/* Run the full Calypso read/parse flow. In live mode (iso14443_4b_poller
 * != NULL) APDUs are exchanged with the card; in file-replay mode
 * (app->data_loaded, poller == NULL) every send is skipped and records are
 * read back from the saved file instead. On success the parsed card is
 * stored in app->calypso_context. Runs on the poller thread in live mode,
 * so it must never touch the UI. */
static bool calypso_read_card(Metroflip* app, Iso14443_4bPoller* iso14443_4b_poller) {
    MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    BitBuffer* tx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);

    if(!app->data_loaded) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolIso14443_4b, nfc_poller_get_data(app->poller));
    }

    Iso14443_4bError error;
    size_t response_length = 0;
    CalypsoCardData* card = NULL;

    do {
        // Initialize the card data
        card = malloc(sizeof(CalypsoCardData));
        if(!card) {
            stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            break;
        }
        // Zero everything so the section counters (contracts/events/
        // special_events) start at 0 - the display loops read them, and
        // a card that never increments one would otherwise render garbage
        // pages. Also NULLs the sub-allocs for safe cleanup.
        memset(card, 0, sizeof(CalypsoCardData));
        card->navigo = NULL;
        card->opus = NULL;
        card->ravkav = NULL;

        // Try selecting Calypso application by AID first (must be first command
        // sent to card - a prior failed command can corrupt card session state)
        if(!app->data_loaded) {
            bool aid_selected = false;

            // 1. Try partial AID (RID only) - works for most Calypso cards
            bit_buffer_reset(tx_buffer);
            bit_buffer_reset(rx_buffer);
            bit_buffer_append_bytes(tx_buffer, calypso_aid_select, sizeof(calypso_aid_select));
            error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
            if(error == Iso14443_4bErrorNone) {
                response_length = bit_buffer_get_size_bytes(rx_buffer);
                if(response_length >= 2 &&
                   bit_buffer_get_byte(rx_buffer, response_length - 2) == apdu_success[0] &&
                   bit_buffer_get_byte(rx_buffer, response_length - 1) == apdu_success[1]) {
                    FURI_LOG_I(TAG, "Calypso AID selected, using ISO 7816 mode");
                    aid_selected = true;
                }
            }

            // 2. If RID select failed, try full Navigo AID for new card variants
            if(!aid_selected) {
                FURI_LOG_I(TAG, "RID select failed, trying full Navigo AID");
                bit_buffer_reset(tx_buffer);
                bit_buffer_reset(rx_buffer);
                bit_buffer_append_bytes(
                    tx_buffer, calypso_navigo_aid_select, sizeof(calypso_navigo_aid_select));
                error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
                if(error == Iso14443_4bErrorNone) {
                    response_length = bit_buffer_get_size_bytes(rx_buffer);
                    if(response_length >= 2 &&
                       bit_buffer_get_byte(rx_buffer, response_length - 2) == apdu_success[0] &&
                       bit_buffer_get_byte(rx_buffer, response_length - 1) == apdu_success[1]) {
                        FURI_LOG_I(TAG, "Full Navigo AID selected, using ISO 7816 mode");
                        aid_selected = true;
                    }
                }
            }

            if(aid_selected) {
                select_app[0] = 0x00;
                select_app[2] = 0x09; // P1: select from current DF
                read_file[0] = 0x00;
            }
        }

        int start = 0, end = 0;

        /* Read the ICC file (0x0002) for the card serial number. Paper
                 * and anonymous Navigo tickets have no ICC file (SELECT returns
                 * 6A82 "file not found"); that is NOT fatal - we leave the
                 * serial unknown (0) and continue to the ticketing/environment
                 * files that hold the real card data. The status word is
                 * checked inline instead of via check_response() so a missing
                 * ICC file does not post a spurious "wrong card" failure event.
                 */
        card->card_number = 0;
        error = select_new_app(0x00, 0x02, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
        if(error != 0) {
            break; // transport-level failure (e.g. card removed)
        }

        bool icc_present = app->data_loaded; // file replay: assume present
        if(!app->data_loaded) {
            response_length = bit_buffer_get_size_bytes(rx_buffer);
            icc_present =
                (response_length >= 2 &&
                 bit_buffer_get_byte(rx_buffer, response_length - 2) == apdu_success[0] &&
                 bit_buffer_get_byte(rx_buffer, response_length - 1) == apdu_success[1]);
        }

        if(icc_present) {
            // Send the read command for ICC
            error = read_new_file(
                "0002", "01", 0x01, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                // Live: transport failure -> give up. File replay: the
                // 0002 record is simply absent -> skip, serial stays 0.
                if(!app->data_loaded) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }
            } else {
                response_length = bit_buffer_get_size_bytes(rx_buffer);
                bool icc_read_ok =
                    app->data_loaded ||
                    (response_length >= 2 &&
                     bit_buffer_get_byte(rx_buffer, response_length - 2) == apdu_success[0] &&
                     bit_buffer_get_byte(rx_buffer, response_length - 1) == apdu_success[1]);
                if(icc_read_ok) {
                    char icc_bit_representation[29 * 8 + 1];
                    icc_bit_representation[0] = '\0';
                    size_t icc_copy_length = response_length > 29 ? 29 : response_length;
                    for(size_t i = 0; i < icc_copy_length; i++) {
                        char bits[9];
                        uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                        byte_to_binary(byte, bits);
                        strlcat(icc_bit_representation, bits, sizeof(icc_bit_representation));
                    }
                    icc_bit_representation[icc_copy_length * 8] = '\0';

                    start = 128;
                    end = 159;
                    card->card_number = bit_slice_to_dec(icc_bit_representation, start, end);
                }
            }
        } else {
            FURI_LOG_I(TAG, "No ICC file (paper/anonymous ticket) - serial unknown");
        }

        // Select the ticketing DF (0x2000). Cards selected via the full
        // Navigo AID are already positioned inside this DF, so a relative
        // SELECT for it returns 6A82 ("file not found") - that is NOT
        // fatal: we just proceed to the environment file (a child EF that
        // selects fine from the current DF). The SW is checked inline so a
        // missing child does not post a spurious "wrong card" event.
        error = select_new_app(0x20, 0x00, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
        if(error != 0) {
            FURI_LOG_E(TAG, "Failed to select app for ticketing");
            break; // transport-level failure (e.g. card removed)
        }
        if(!app->data_loaded) {
            response_length = bit_buffer_get_size_bytes(rx_buffer);
            bool ticketing_ok =
                (response_length >= 2 &&
                 bit_buffer_get_byte(rx_buffer, response_length - 2) == apdu_success[0] &&
                 bit_buffer_get_byte(rx_buffer, response_length - 1) == apdu_success[1]);
            if(!ticketing_ok) {
                FURI_LOG_I(TAG, "Ticketing DF not a child (already inside it) - continuing");
            }
        }

        // Select app for environment
        error = select_new_app(0x20, 0x1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
        if(error != 0) {
            break;
        }

        // Check the response after selecting app
        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
            break;
        }

        // read file 1
        error =
            read_new_file("2001", "01", 1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);

        if(error != 0) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            break;
        }

        // Check the response after reading the file
        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
            break;
        }
        char environment_bit_representation[29 * 8 + 1];
        environment_bit_representation[0] = '\0';
        size_t env_copy_length = response_length > 29 ? 29 : response_length;
        for(size_t i = 0; i < env_copy_length; i++) {
            char bits[9];
            uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
            byte_to_binary(byte, bits);
            strlcat(environment_bit_representation, bits, sizeof(environment_bit_representation));
        }
        //FURI_LOG_I(
        //     TAG, "Environment bit_representation: %s", environment_bit_representation);
        start = 13;
        end = 16;
        card->country_num =
            bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
            bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
            bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
        start = 25;
        end = 28;
        card->network_num =
            bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
            bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
            bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
        card->card_type = guess_card_type(card->country_num, card->network_num);
        switch(card->card_type) {
        case CALYPSO_CARD_NAVIGO: {
            card->navigo = malloc(sizeof(NavigoCardData));
            if(!card->navigo) {
                stage = MetroflipPollerEventTypeFail;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MetroflipCustomEventPollerFail);
                break;
            }
            /* Zero unread contract/event slots so the display (which
                     * iterates by .present / *_count) never reads heap garbage. */
            memset(card->navigo, 0, sizeof(NavigoCardData));

            card->navigo->environment.country_num = card->country_num;
            card->navigo->environment.network_num = card->network_num;

            CalypsoApp* IntercodeEnvHolderStructure = get_intercode_structure_env_holder();

            // EnvApplicationVersionNumber
            const char* env_key = "EnvApplicationVersionNumber";
            int positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, IntercodeEnvHolderStructure);
            int start = positionOffset,
                end = positionOffset +
                      get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
            card->navigo->environment.app_version =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // EnvApplicationValidityEndDate
            env_key = "EnvApplicationValidityEndDate";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, IntercodeEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
            float decimal_value = bit_slice_to_dec(environment_bit_representation, start, end);
            uint64_t end_validity_timestamp = (decimal_value * 24 * 3600) + (float)epoch + 3600;
            datetime_timestamp_to_datetime(
                end_validity_timestamp, &card->navigo->environment.end_dt);

            // HolderDataCardStatus
            env_key = "HolderDataCardStatus";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, IntercodeEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
            card->navigo->holder.card_status =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // HolderDataCommercialID
            env_key = "HolderDataCommercialID";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, IntercodeEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
            card->navigo->holder.commercial_id =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // Free the calypso structure
            free_calypso_structure(IntercodeEnvHolderStructure);

            // Select app for contracts
            error =
                select_new_app(0x20, 0x20, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                FURI_LOG_E(TAG, "Failed to select app for contracts");
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                FURI_LOG_E(TAG, "Failed to check response after selecting app for contracts");
                break;
            }

            // Prepare calypso structure
            CalypsoApp* IntercodeContractStructure = get_intercode_structure_contract();
            if(!IntercodeContractStructure) {
                FURI_LOG_E(TAG, "Failed to load Intercode Contract structure");
                break;
            }

            // Now send the read command for contracts

            for(size_t i = 1; i < 5; i++) {
                char FID_buf[3];
                snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                const char* FID = FID_buf;
                error = read_new_file(
                    "2020", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    FURI_LOG_E(TAG, "Failed to read contract %d", i);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after reading contract %d", i);
                    break;
                }

                char bit_representation[29 * 8 + 1];
                bit_representation[0] = '\0';
                size_t contract_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < contract_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(bit_representation, bits, sizeof(bit_representation));
                }
                bit_representation[contract_copy_length * 8] = '\0';

                if(bit_slice_to_dec(
                       bit_representation,
                       0,
                       IntercodeContractStructure->container->elements[0].bitmap->size - 1) == 0) {
                    break;
                }

                card->navigo->contracts[i - 1].present = 1;
                card->contracts_count++;

                // 2. ContractTariff
                const char* contract_key = "ContractTariff";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].tariff =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // 3. ContractSerialNumber
                contract_key = "ContractSerialNumber";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].serial_number =
                        bit_slice_to_dec(bit_representation, start, end);
                    card->navigo->contracts[i - 1].serial_number_available = true;
                }

                // 8. ContractPayMethod
                contract_key = "ContractPayMethod";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].pay_method =
                        bit_slice_to_dec(bit_representation, start, end);
                    card->navigo->contracts[i - 1].pay_method_available = true;
                }

                // 10. ContractPriceAmount
                contract_key = "ContractPriceAmount";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].price_amount =
                        bit_slice_to_dec(bit_representation, start, end) / 100.0;
                    card->navigo->contracts[i - 1].price_amount_available = true;
                }

                // 13.0. ContractValidityStartDate
                contract_key = "ContractValidityStartDate";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                    uint64_t start_validity_timestamp = (decimal_value + (float)epoch) + 3600;
                    datetime_timestamp_to_datetime(
                        start_validity_timestamp, &card->navigo->contracts[i - 1].start_date);
                }

                // 13.2. ContractValidityEndDate
                contract_key = "ContractValidityEndDate";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                    uint64_t end_validity_timestamp = (decimal_value + (float)epoch) + 3600;
                    datetime_timestamp_to_datetime(
                        end_validity_timestamp, &card->navigo->contracts[i - 1].end_date);
                    card->navigo->contracts[i - 1].end_date_available = true;
                }

                // 13.6. ContractValidityZones
                contract_key = "ContractValidityZones";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int start = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    // binary form is 00011111 for zones 5, 4, 3, 2, 1
                    for(int j = 0; j < 5; j++) {
                        card->navigo->contracts[i - 1].zones[j] =
                            bit_slice_to_dec(bit_representation, start + 3 + j, start + 3 + j);
                    }
                    card->navigo->contracts[i - 1].zones_available = true;
                }

                // 13.7. ContractValidityJourneys
                contract_key = "ContractValidityJourneys";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    int decimal_value = bit_slice_to_dec(bit_representation, start, end);
                    // first 5 bits -> CounterStructureNumber
                    // last 8 bits -> CounterLastLoad
                    // other bits -> RFU
                    card->navigo->contracts[i - 1].counter.struct_number = decimal_value >> 11;
                    card->navigo->contracts[i - 1].counter.last_load = decimal_value & 0xFF;
                    card->navigo->contracts[i - 1].counter_present = true;
                }

                // 15.0. ContractValiditySaleDate
                contract_key = "ContractValiditySaleDate";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                    uint64_t sale_timestamp = (decimal_value + (float)epoch) + 3600;
                    datetime_timestamp_to_datetime(
                        sale_timestamp, &card->navigo->contracts[i - 1].sale_date);
                }

                // 15.2. ContractValiditySaleAgent - FIX NEEDED
                contract_key = "ContractValiditySaleAgent";
                /* if(is_calypso_node_present(
                           bit_representation, contract_key, NavigoContractStructure)) { */
                int positionOffset = get_calypso_node_offset(
                    bit_representation, contract_key, IntercodeContractStructure);
                int start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                card->navigo->contracts[i - 1].sale_agent =
                    bit_slice_to_dec(bit_representation, start, end);
                // }

                // 15.3. ContractValiditySaleDevice
                contract_key = "ContractValiditySaleDevice";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].sale_device =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // 16. ContractStatus  -- 0x1 ou 0xff
                contract_key = "ContractStatus";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].status =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // 18. ContractAuthenticator
                contract_key = "ContractAuthenticator";
                if(is_calypso_node_present(
                       bit_representation, contract_key, IntercodeContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, IntercodeContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, IntercodeContractStructure) - 1;
                    card->navigo->contracts[i - 1].authenticator =
                        bit_slice_to_dec(bit_representation, start, end);
                }
            }

            // Free the calypso structure
            free_calypso_structure(IntercodeContractStructure);

            // Select app for counters (remaining tickets on Navigo Easy)
            error =
                select_new_app(0x20, 0x69, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                break;
            }

            // read file 1
            error = read_new_file(
                "2069", "01", 1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MetroflipCustomEventPollerFail);
                break;
            }

            // Check the response after reading the file
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                break;
            }

            char counter_bit_representation[29 * 8 + 1];
            counter_bit_representation[0] = '\0';
            size_t counter_copy_length = response_length > 29 ? 29 : response_length;
            for(size_t i = 0; i < counter_copy_length; i++) {
                char bits[9];
                uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                byte_to_binary(byte, bits);
                strlcat(counter_bit_representation, bits, sizeof(counter_bit_representation));
            }
            // FURI_LOG_I(TAG, "Counter bit_representation: %s", counter_bit_representation);

            // Ticket counts (contracts 1-4)
            for(int i = 0; i < 4; i++) {
                if(card->navigo->contracts[i].present == 0) {
                    continue;
                }
                if(card->navigo->contracts[i].counter_present == 0) {
                    continue;
                }
                start = 0;
                end = 5;
                card->navigo->contracts[i].counter.count =
                    bit_slice_to_dec(counter_bit_representation, 24 * i + start, 24 * i + end);

                start = 6;
                end = 23;
                card->navigo->contracts[i].counter.relative_first_stamp_15mn =
                    bit_slice_to_dec(counter_bit_representation, 24 * i + start, 24 * i + end);
            }

            // Select app for events
            error =
                select_new_app(0x20, 0x10, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                break;
            }

            // Load the calypso structure for events
            CalypsoApp* IntercodeEventStructure = get_intercode_structure_event();
            if(!IntercodeEventStructure) {
                FURI_LOG_E(TAG, "Failed to load Intercode Event structure");
                break;
            }

            // furi_string_cat_printf(parsed_data, "\e#Events :\n");
            // Now send the read command for events
            for(size_t i = 1; i < 4; i++) {
                char FID_buf[3];
                snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                const char* FID = FID_buf;
                error = read_new_file(
                    "2010", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                char event_bit_representation[29 * 8 + 1];
                event_bit_representation[0] = '\0';
                size_t event_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < event_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(event_bit_representation, bits, sizeof(event_bit_representation));
                }

                // 2. EventCode
                const char* event_key = "EventCode";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].transport_type = decimal_value >> 4;
                    card->navigo->events[i - 1].transition = decimal_value & 15;
                }

                // 4. EventServiceProvider
                event_key = "EventServiceProvider";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].service_provider =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // 8. EventLocationId
                event_key = "EventLocationId";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].station_group_id = decimal_value >> 9;
                    card->navigo->events[i - 1].station_id = (decimal_value >> 4) & 31;
                    card->navigo->events[i - 1].station_sub_id = decimal_value & 15;
                }

                // 9. EventLocationGate
                event_key = "EventLocationGate";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].location_gate =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].location_gate_available = true;
                }

                // 10. EventDevice
                event_key = "EventDevice";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].device = decimal_value;
                    int bus_device = decimal_value >> 8;
                    card->navigo->events[i - 1].door = bus_device / 2 + 1;
                    card->navigo->events[i - 1].side = bus_device % 2;
                    card->navigo->events[i - 1].device_available = true;
                }

                // 11. EventRouteNumber
                event_key = "EventRouteNumber";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].route_number =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].route_number_available = true;
                }

                // 13. EventJourneyRun
                event_key = "EventJourneyRun";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].mission =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].mission_available = true;
                }

                // 14. EventVehicleId
                event_key = "EventVehicleId";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].vehicle_id =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].vehicle_id_available = true;
                }

                // 25. EventContractPointer
                event_key = "EventContractPointer";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->events[i - 1].used_contract =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->events[i - 1].used_contract_available = true;
                    if(card->navigo->events[i - 1].used_contract > 0) {
                        card->events_count++;
                    }
                }

                // EventDateStamp
                event_key = "EventDateStamp";
                int positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, IntercodeEventStructure);
                start = positionOffset,
                end =
                    positionOffset + get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                uint64_t date_timestamp = (decimal_value * 24 * 3600) + epoch + 3600;
                datetime_timestamp_to_datetime(date_timestamp, &card->navigo->events[i - 1].date);

                // EventTimeStamp
                event_key = "EventTimeStamp";
                positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, IntercodeEventStructure);
                start = positionOffset,
                end =
                    positionOffset + get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                card->navigo->events[i - 1].date.hour = (decimal_value * 60) / 3600;
                card->navigo->events[i - 1].date.minute = ((decimal_value * 60) % 3600) / 60;
                card->navigo->events[i - 1].date.second = ((decimal_value * 60) % 3600) % 60;
            }

            // Select app for special events
            error =
                select_new_app(0x20, 0x40, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                break;
            }

            // Now send the read command for special events
            for(size_t i = 1; i < 4; i++) {
                char FID_buf[3];
                snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                const char* FID = FID_buf;
                error = read_new_file(
                    "2040", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                char event_bit_representation[29 * 8 + 1];
                event_bit_representation[0] = '\0';
                size_t special_event_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < special_event_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(event_bit_representation, bits, sizeof(event_bit_representation));
                }

                if(bit_slice_to_dec(
                       event_bit_representation,
                       0,
                       IntercodeEventStructure->container->elements[0].bitmap->size - 1) == 0) {
                    break;
                } else {
                    card->special_events_count++;
                }

                // 2. EventCode
                const char* event_key = "EventCode";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->special_events[i - 1].transport_type = decimal_value >> 4;
                    card->navigo->special_events[i - 1].transition = decimal_value & 15;
                }

                // 3. EventResult
                event_key = "EventResult";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->special_events[i - 1].result =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // 4. EventServiceProvider
                event_key = "EventServiceProvider";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->special_events[i - 1].service_provider =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // 8. EventLocationId
                event_key = "EventLocationId";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->special_events[i - 1].station_group_id = decimal_value >> 9;
                    card->navigo->special_events[i - 1].station_id = (decimal_value >> 4) & 31;
                    card->navigo->special_events[i - 1].station_sub_id = decimal_value & 15;
                }

                // 10. EventDevice
                event_key = "EventDevice";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->special_events[i - 1].device = decimal_value;
                }

                // 11. EventRouteNumber
                event_key = "EventRouteNumber";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, IntercodeEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, IntercodeEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                    card->navigo->special_events[i - 1].route_number =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    card->navigo->special_events[i - 1].route_number_available = true;
                }

                // EventDateStamp
                event_key = "EventDateStamp";
                int positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, IntercodeEventStructure);
                int start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                uint64_t date_timestamp = (decimal_value * 24 * 3600) + epoch + 3600;
                datetime_timestamp_to_datetime(
                    date_timestamp, &card->navigo->special_events[i - 1].date);

                // EventTimeStamp
                event_key = "EventTimeStamp";
                positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, IntercodeEventStructure);
                start = positionOffset,
                end =
                    positionOffset + get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                card->navigo->special_events[i - 1].date.hour = (decimal_value * 60) / 3600;
                card->navigo->special_events[i - 1].date.minute =
                    ((decimal_value * 60) % 3600) / 60;
                card->navigo->special_events[i - 1].date.second =
                    ((decimal_value * 60) % 3600) % 60;
            }

            // Free the calypso structure
            free_calypso_structure(IntercodeEventStructure);
            break;
        }
        case CALYPSO_CARD_OPUS: {
            card->opus = malloc(sizeof(OpusCardData));
            if(!card->opus) {
                stage = MetroflipPollerEventTypeFail;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MetroflipCustomEventPollerFail);
                break;
            }
            /* Zero unread contract/event slots so the display (which
                     * iterates by .present / *_count) never reads heap garbage. */
            memset(card->opus, 0, sizeof(OpusCardData));

            card->opus->environment.country_num = card->country_num;
            card->opus->environment.network_num = card->network_num;

            CalypsoApp* OpusEnvHolderStructure = get_opus_env_holder_structure();

            // EnvApplicationVersionNumber
            const char* env_key = "EnvApplicationVersionNumber";
            int positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            int start = positionOffset,
                end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
            card->opus->environment.app_version =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // EnvApplicationIssuerId
            env_key = "EnvApplicationIssuerId";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
            card->opus->environment.issuer_id =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // EnvApplicationValidityEndDate
            env_key = "EnvApplicationValidityEndDate";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
            float decimal_value = bit_slice_to_dec(environment_bit_representation, start, end);
            uint64_t end_validity_timestamp = (decimal_value * 24 * 3600) + (float)epoch + 3600;
            datetime_timestamp_to_datetime(
                end_validity_timestamp, &card->opus->environment.end_dt);

            // EnvDataCardStatus
            env_key = "EnvDataCardStatus";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
            card->opus->environment.card_status =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // EnvData_CardUtilisation
            env_key = "EnvData_CardUtilisation";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            start = positionOffset,
            end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
            card->opus->environment.card_utilisation =
                bit_slice_to_dec(environment_bit_representation, start, end);

            // HolderBirthDate
            env_key = "HolderBirthDate";
            positionOffset = get_calypso_node_offset(
                environment_bit_representation, env_key, OpusEnvHolderStructure);
            start = positionOffset, end = positionOffset + 3;
            card->opus->holder.birth_date.year =
                bit_slice_to_dec(environment_bit_representation, start, end) * 1000 +
                bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 100 +
                bit_slice_to_dec(environment_bit_representation, start + 8, end + 8) * 10 +
                bit_slice_to_dec(environment_bit_representation, start + 12, end + 12);
            start += 16, end += 16;
            card->opus->holder.birth_date.month =
                bit_slice_to_dec(environment_bit_representation, start, end) * 10 +
                bit_slice_to_dec(environment_bit_representation, start + 4, end + 4);
            start += 8, end += 8;
            card->opus->holder.birth_date.day =
                bit_slice_to_dec(environment_bit_representation, start, end) * 10 +
                bit_slice_to_dec(environment_bit_representation, start + 4, end + 4);

            // Free the calypso structure
            free_calypso_structure(OpusEnvHolderStructure);

            // Select app for contracts
            error =
                select_new_app(0x20, 0x20, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                FURI_LOG_E(TAG, "Failed to select app for contracts");
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                FURI_LOG_E(TAG, "Failed to check response after selecting app for contracts");
                break;
            }

            // Prepare calypso structure
            CalypsoApp* OpusContractStructure = get_opus_contract_structure();
            if(!OpusContractStructure) {
                FURI_LOG_E(TAG, "Failed to load Opus Contract structure");
                break;
            }

            // Now send the read command for contracts
            for(size_t i = 1; i < 5; i++) {
                char FID_buf[3];
                snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                const char* FID = FID_buf;
                error = read_new_file(
                    "2020", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    FURI_LOG_E(TAG, "Failed to read contract %d", i);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after reading contract %d", i);
                    break;
                }

                char bit_representation[29 * 8 + 1];
                bit_representation[0] = '\0';
                size_t opus_contract_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < opus_contract_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(bit_representation, bits, sizeof(bit_representation));
                }
                bit_representation[opus_contract_copy_length * 8] = '\0';

                if(bit_slice_to_dec(
                       bit_representation,
                       0,
                       OpusContractStructure->container->elements[0].bitmap->size - 1) == 0) {
                    break;
                }

                card->opus->contracts[i - 1].present = 1;
                card->contracts_count++;

                // ContractProvider
                const char* contract_key = "ContractProvider";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    card->opus->contracts[i - 1].provider =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // ContractTariff
                contract_key = "ContractTariff";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    card->opus->contracts[i - 1].tariff =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // ContractValidityStartDate
                contract_key = "ContractValidityStartDate";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                    uint64_t start_validity_timestamp = (decimal_value + (float)epoch) + 3600;
                    datetime_timestamp_to_datetime(
                        start_validity_timestamp, &card->opus->contracts[i - 1].start_date);
                }

                // ContractValidityEndDate
                contract_key = "ContractValidityEndDate";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                    uint64_t end_validity_timestamp = (decimal_value + (float)epoch) + 3600;
                    datetime_timestamp_to_datetime(
                        end_validity_timestamp, &card->opus->contracts[i - 1].end_date);
                }

                // ContractDataSaleAgent
                contract_key = "ContractDataSaleAgent";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    card->opus->contracts[i - 1].sale_agent =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // ContractDataSaleDate + ContractDataSaleTime
                contract_key = "ContractDataSaleDate";
                int positionOffset = get_calypso_node_offset(
                    bit_representation, contract_key, OpusContractStructure);
                FURI_LOG_I(TAG, "ContractDataSaleDate positionOffset: %d", positionOffset);
                int start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                FURI_LOG_I(
                    TAG,
                    "ContractDataSaleDate: %d",
                    bit_slice_to_dec(bit_representation, start, end));
                uint64_t sale_date_timestamp =
                    ((bit_slice_to_dec(bit_representation, start, end) * 24 * 3600) +
                     (float)epoch) +
                    3600;
                ;
                datetime_timestamp_to_datetime(
                    sale_date_timestamp, &card->opus->contracts[i - 1].sale_date);

                contract_key = "ContractDataSaleTime";
                positionOffset = get_calypso_node_offset(
                    bit_representation, contract_key, OpusContractStructure);
                start = positionOffset,
                end = positionOffset + get_calypso_node_size(contract_key, OpusContractStructure) -
                      1;
                int decimal_value = bit_slice_to_dec(bit_representation, start, end);
                card->opus->contracts[i - 1].sale_date.hour = (decimal_value * 60) / 3600;
                card->opus->contracts[i - 1].sale_date.minute = ((decimal_value * 60) % 3600) / 60;
                card->opus->contracts[i - 1].sale_date.second = ((decimal_value * 60) % 3600) % 60;

                // ContractDataInhibition
                contract_key = "ContractDataInhibition";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    card->opus->contracts[i - 1].inhibition =
                        bit_slice_to_dec(bit_representation, start, end);
                }

                // ContractDataUsed
                contract_key = "ContractDataUsed";
                if(is_calypso_node_present(
                       bit_representation, contract_key, OpusContractStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, OpusContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                    card->opus->contracts[i - 1].used =
                        bit_slice_to_dec(bit_representation, start, end);
                }
            }

            // Free the calypso structure
            free_calypso_structure(OpusContractStructure);

            // Select app for events
            error =
                select_new_app(0x20, 0x10, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
            if(error != 0) {
                break;
            }

            // Check the response after selecting app
            if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                break;
            }

            // Load the calypso structure for events
            CalypsoApp* OpusEventStructure = get_opus_event_structure();
            if(!OpusEventStructure) {
                FURI_LOG_E(TAG, "Failed to load Opus Event structure");
                break;
            }

            // Now send the read command for events
            for(size_t i = 1; i < 4; i++) {
                char FID_buf[3];
                snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                const char* FID = FID_buf;
                error = read_new_file(
                    "2010", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                char event_bit_representation[29 * 8 + 1];
                event_bit_representation[0] = '\0';
                size_t opus_event_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < opus_event_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(event_bit_representation, bits, sizeof(event_bit_representation));
                }

                // EventResult
                const char* event_key = "EventResult";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].result =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventServiceProvider
                event_key = "EventServiceProvider";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].service_provider =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventLocationId
                event_key = "EventLocationId";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].location_id =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventRouteNumber
                event_key = "EventRouteNumber";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].route_number =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventContractPointer
                event_key = "EventContractPointer";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].used_contract =
                        bit_slice_to_dec(event_bit_representation, start, end);
                    if(card->opus->events[i - 1].used_contract > 0) {
                        card->events_count++;
                    }
                }

                // EventDataSimulation
                event_key = "EventDataSimulation";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].simulation =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventDataRouteDirection
                event_key = "EventDataRouteDirection";
                if(is_calypso_node_present(
                       event_bit_representation, event_key, OpusEventStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        event_bit_representation, event_key, OpusEventStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                    card->opus->events[i - 1].route_direction =
                        bit_slice_to_dec(event_bit_representation, start, end);
                }

                // EventDateStamp
                event_key = "EventDateStamp";
                int positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, OpusEventStructure);
                int start = positionOffset,
                    end =
                        positionOffset + get_calypso_node_size(event_key, OpusEventStructure) - 1;
                int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                uint64_t date_timestamp = (decimal_value * 24 * 3600) + epoch + 3600;
                datetime_timestamp_to_datetime(date_timestamp, &card->opus->events[i - 1].date);

                // EventTimeStamp
                event_key = "EventTimeStamp";
                positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, OpusEventStructure);
                start = positionOffset,
                end = positionOffset + get_calypso_node_size(event_key, OpusEventStructure) - 1;
                decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                card->opus->events[i - 1].date.hour = (decimal_value * 60) / 3600;
                card->opus->events[i - 1].date.minute = ((decimal_value * 60) % 3600) / 60;
                card->opus->events[i - 1].date.second = ((decimal_value * 60) % 3600) % 60;

                // EventDataDateFirstStamp
                event_key = "EventDataDateFirstStamp";
                positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, OpusEventStructure);
                start = positionOffset,
                end = positionOffset + get_calypso_node_size(event_key, OpusEventStructure) - 1;
                decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                uint64_t first_date_timestamp = (decimal_value * 24 * 3600) + epoch + 3600;
                datetime_timestamp_to_datetime(
                    first_date_timestamp, &card->opus->events[i - 1].first_stamp_date);

                // EventDataTimeFirstStamp
                event_key = "EventDataTimeFirstStamp";
                positionOffset = get_calypso_node_offset(
                    event_bit_representation, event_key, OpusEventStructure);
                start = positionOffset,
                end = positionOffset + get_calypso_node_size(event_key, OpusEventStructure) - 1;
                decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                card->opus->events[i - 1].first_stamp_date.hour = (decimal_value * 60) / 3600;
                card->opus->events[i - 1].first_stamp_date.minute =
                    ((decimal_value * 60) % 3600) / 60;
                card->opus->events[i - 1].first_stamp_date.second =
                    ((decimal_value * 60) % 3600) % 60;
            }

            // Free the calypso structure
            free_calypso_structure(OpusEventStructure);

            break;
        }
        case CALYPSO_CARD_UNKNOWN: {
            start = 3;
            end = 6;
            int country_num =
                bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
            start = 15;
            end = 18;
            int network_num =
                bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
            card->card_type = guess_card_type(country_num, network_num);
            FURI_LOG_I(TAG, "card type again: %d", card->card_type);
            if(card->card_type == CALYPSO_CARD_RAVKAV) {
                card->ravkav = malloc(sizeof(RavKavCardData));
                if(!card->ravkav) {
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }
                /* Zero it: only the contracts/events actually read get
                         * populated, and the display iterates contracts[0..3]
                         * by their .present flag and events[0..count]. Without
                         * this, unread slots hold heap garbage (non-zero after a
                         * save->load cycle), and get_ravkav_issuer() then indexes
                         * its list with a garbage provider -> MPU fault. */
                memset(card->ravkav, 0, sizeof(RavKavCardData));

                // Prepare calypso structure

                CalypsoApp* RavKavContractStructure = get_ravkav_contract_structure();
                if(!RavKavContractStructure) {
                    FURI_LOG_E(TAG, "Failed to load RavKav Contract structure");
                    break;
                }

                //get balance
                error = select_new_app(
                    0x20, 0x2A, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    FURI_LOG_E(TAG, "Failed to select app for contracts");
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after selecting app for counter");
                    break;
                }

                error = read_new_file(
                    "202A", "01", 1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    FURI_LOG_E(TAG, "Failed to read counter %d", 1);
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after reading counter %d", 1);
                    break;
                }

                uint32_t value = 0;
                for(uint8_t i = 0; i < 3; i++) {
                    value = (value << 8) | bit_buffer_get_byte(rx_buffer, i);
                }
                float result = value / 100.0f;
                FURI_LOG_I(TAG, "Value: %.2f ILS", (double)result);

                card->ravkav->contracts[0].balance = result;

                error = select_new_app(
                    0x20, 0x20, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    FURI_LOG_E(TAG, "Failed to select app for contracts");
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after selecting app for contracts");
                    break;
                }

                // Now send the read command for contracts
                for(size_t i = 1; i < 2; i++) {
                    char FID_buf[3];
                    snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                    const char* FID = FID_buf;
                    error = read_new_file(
                        "2020", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, MetroflipCustomEventPollerFail);
                        FURI_LOG_E(TAG, "Failed to read contract %d", i);
                        break;
                    }

                    // Check the response after reading the file
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        FURI_LOG_E(TAG, "Failed to check response after reading contract %d", i);
                        break;
                    }

                    char bit_representation[29 * 8 + 1];
                    bit_representation[0] = '\0';
                    size_t ravkav_contract_copy_length = response_length > 29 ? 29 :
                                                                                response_length;
                    for(size_t i = 0; i < ravkav_contract_copy_length; i++) {
                        char bits[9];
                        uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                        byte_to_binary(byte, bits);
                        strlcat(bit_representation, bits, sizeof(bit_representation));
                    }
                    bit_representation[ravkav_contract_copy_length * 8] = '\0';
                    card->ravkav->contracts[i - 1].present = 1;
                    card->contracts_count++;

                    // ContractVersion

                    const char* contract_key = "ContractVersion";

                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);

                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;

                        card->ravkav->contracts[i - 1].version =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractStartDate
                    contract_key = "ContractStartDate";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        int decimal_value = bit_slice_to_dec(bit_representation, start, end);
                        uint32_t invertedDays = decimal_value ^ 0x3FFF;

                        int start_validity_timestamp = (invertedDays * 3600 * 24) + epoch + 3600;

                        datetime_timestamp_to_datetime(
                            start_validity_timestamp, &card->ravkav->contracts[i - 1].start_date);
                    }

                    // ContractProvider
                    contract_key = "ContractProvider";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].provider =
                            bit_slice_to_dec(bit_representation, start, end);
                        FURI_LOG_I(
                            TAG, "issuer number: %d", card->ravkav->contracts[i - 1].provider);
                    }

                    // ContractTariff
                    contract_key = "ContractTariff";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].tariff =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractSaleDate
                    contract_key = "ContractSaleDate";
                    int positionOffset = get_calypso_node_offset(
                        bit_representation, contract_key, RavKavContractStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                    uint64_t sale_date_timestamp =
                        (bit_slice_to_dec(bit_representation, start, end) * 3600 * 24) +
                        (float)epoch + 3600;
                    datetime_timestamp_to_datetime(
                        sale_date_timestamp, &card->ravkav->contracts[i - 1].sale_date);

                    // ContractSaleDevice
                    contract_key = "ContractSaleDevice";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].sale_device =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractSaleNumber
                    contract_key = "ContractSaleNumber";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].sale_number =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractInterchange
                    contract_key = "ContractInterchange";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].interchange =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractInterchange
                    contract_key = "ContractRestrictCode";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].restrict_code_available = true;
                        card->ravkav->contracts[i - 1].restrict_code =
                            bit_slice_to_dec(bit_representation, start, end);
                    }

                    // ContractRestrictDuration
                    contract_key = "ContractRestrictDuration";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].restrict_duration_available = true;
                        if(card->ravkav->contracts[i - 1].restrict_code == 16) {
                            card->ravkav->contracts[i - 1].restrict_duration =
                                bit_slice_to_dec(bit_representation, start, end) * 5;
                        } else {
                            card->ravkav->contracts[i - 1].restrict_duration =
                                bit_slice_to_dec(bit_representation, start, end) * 30;
                        }
                    }

                    // ContractEndDate
                    contract_key = "ContractEndDate";
                    if(is_calypso_node_present(
                           bit_representation, contract_key, RavKavContractStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, RavKavContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, RavKavContractStructure) - 1;
                        card->ravkav->contracts[i - 1].end_date_available = true;
                        int end_date_timestamp =
                            (bit_slice_to_dec(bit_representation, start, end) * 3600 * 24) +
                            epoch + 3600;

                        datetime_timestamp_to_datetime(
                            end_date_timestamp, &card->ravkav->contracts[i - 1].end_date);
                    }
                }

                // Free the calypso structure
                free_calypso_structure(RavKavContractStructure);

                error = select_new_app(
                    0x20, 0x01, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    FURI_LOG_E(TAG, "Failed to select app for environment");
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(
                        TAG, "Failed to check response after selecting app for environment");
                    break;
                }

                // Prepare calypso structure

                CalypsoApp* RavKavEnvStructure = get_ravkav_env_holder_structure();
                if(!RavKavEnvStructure) {
                    FURI_LOG_E(TAG, "Failed to load RavKav environment structure");
                    break;
                }

                // Now send the read command for environment

                error = read_new_file(
                    "2001", "01", 1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    FURI_LOG_E(TAG, "Failed to read environment");
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after reading environment");
                    break;
                }

                char env_bit_representation[29 * 8 + 1];
                env_bit_representation[0] = '\0';
                size_t ravkav_env_copy_length = response_length > 29 ? 29 : response_length;
                for(size_t i = 0; i < ravkav_env_copy_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(env_bit_representation, bits, sizeof(env_bit_representation));
                }
                env_bit_representation[ravkav_env_copy_length * 8] = '\0';

                // EnvApplicationVersionNumber
                char* env_key = "EnvApplicationVersionNumber";
                if(is_calypso_node_present(env_bit_representation, env_key, RavKavEnvStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        env_bit_representation, env_key, RavKavEnvStructure);
                    int start = positionOffset,
                        end = positionOffset + get_calypso_node_size(env_key, RavKavEnvStructure) -
                              1;
                    card->ravkav->environment.app_num =
                        bit_slice_to_dec(env_bit_representation, start, end);
                }

                // EnvApplicationNumber
                env_key = "EnvApplicationNumber";
                if(is_calypso_node_present(env_bit_representation, env_key, RavKavEnvStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        env_bit_representation, env_key, RavKavEnvStructure);
                    int start = positionOffset,
                        end = positionOffset + get_calypso_node_size(env_key, RavKavEnvStructure) -
                              1;
                    card->ravkav->environment.app_num =
                        bit_slice_to_dec(env_bit_representation, start, end);
                }

                // EnvDateOfIssue
                env_key = "EnvDateOfIssue";
                if(is_calypso_node_present(env_bit_representation, env_key, RavKavEnvStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        env_bit_representation, env_key, RavKavEnvStructure);
                    int start = positionOffset,
                        end = positionOffset + get_calypso_node_size(env_key, RavKavEnvStructure) -
                              1;

                    uint64_t issue_date_timestamp =
                        (bit_slice_to_dec(env_bit_representation, start, end) * 3600 * 24) +
                        (float)epoch + 3600;
                    datetime_timestamp_to_datetime(
                        issue_date_timestamp, &card->ravkav->environment.issue_dt);
                }

                // EnvEndValidity
                env_key = "EnvEndValidity";
                if(is_calypso_node_present(env_bit_representation, env_key, RavKavEnvStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        env_bit_representation, env_key, RavKavEnvStructure);
                    int start = positionOffset,
                        end = positionOffset + get_calypso_node_size(env_key, RavKavEnvStructure) -
                              1;

                    uint64_t end_date_timestamp =
                        (bit_slice_to_dec(env_bit_representation, start, end) * 3600 * 24) +
                        (float)epoch + 3600;
                    datetime_timestamp_to_datetime(
                        end_date_timestamp, &card->ravkav->environment.end_dt);
                }

                // EnvPayMethod
                env_key = "EnvPayMethod";
                if(is_calypso_node_present(env_bit_representation, env_key, RavKavEnvStructure)) {
                    int positionOffset = get_calypso_node_offset(
                        env_bit_representation, env_key, RavKavEnvStructure);
                    int start = positionOffset,
                        end = positionOffset + get_calypso_node_size(env_key, RavKavEnvStructure) -
                              1;
                    card->ravkav->environment.pay_method =
                        bit_slice_to_dec(env_bit_representation, start, end);
                }

                free_calypso_structure(RavKavEnvStructure);

                // Select app for events
                error = select_new_app(
                    0x20, 0x10, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                // Load the calypso structure for events
                CalypsoApp* RavKavEventStructure = get_ravkav_event_structure();
                if(!RavKavEventStructure) {
                    FURI_LOG_E(TAG, "Failed to load Opus Event structure");
                    break;
                }

                // Now send the read command for events
                for(size_t i = 1; i < 4; i++) {
                    char FID_buf[3];
                    snprintf(FID_buf, sizeof(FID_buf), "%02X", i);
                    const char* FID = FID_buf;
                    error = read_new_file(
                        "2010", FID, i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, MetroflipCustomEventPollerFail);
                        break;
                    }

                    // Check the response after reading the file
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        break;
                    }

                    char event_bit_representation[29 * 8 + 1];
                    event_bit_representation[0] = '\0';
                    size_t ravkav_event_copy_length = response_length > 29 ? 29 : response_length;
                    for(size_t i = 0; i < ravkav_event_copy_length; i++) {
                        char bits[9];
                        uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                        byte_to_binary(byte, bits);
                        strlcat(event_bit_representation, bits, sizeof(event_bit_representation));
                    }
                    FURI_LOG_I(TAG, "event bit repr %s", event_bit_representation);
                    // EventVersion
                    const char* event_key = "EventVersion";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].event_version =
                            bit_slice_to_dec(event_bit_representation, start, end);
                    }

                    // EventServiceProvider
                    event_key = "EventServiceProvider";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        FURI_LOG_I(TAG, "service provider: start: %d, end %d", start, end);
                        card->ravkav->events[i - 1].service_provider =
                            bit_slice_to_dec(event_bit_representation, start, end);
                    }

                    // EventContractID
                    event_key = "EventContractID";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].contract_id =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        FURI_LOG_I(TAG, "2: start: %d, end %d", start, end);
                    }

                    // EventAreaID
                    event_key = "EventAreaID";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].area_id =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        FURI_LOG_I(TAG, "3: start: %d, end %d", start, end);
                    }

                    // EventType
                    event_key = "EventType";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].type =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        FURI_LOG_I(TAG, "4: start: %d, end %d", start, end);
                    }

                    // EventRouteNumber
                    event_key = "EventExtension";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        FURI_LOG_I(TAG, "event extension : start: %d, end %d", start, end);
                        FURI_LOG_I(
                            TAG,
                            "event extension bitmap: %d",
                            bit_slice_to_dec(event_bit_representation, start, end));
                    }

                    // EventTime
                    event_key = "EventTime";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        uint64_t event_timestamp =
                            bit_slice_to_dec(event_bit_representation, start, end) + (float)epoch +
                            3600;
                        datetime_timestamp_to_datetime(
                            event_timestamp, &card->ravkav->events[i - 1].time);
                        FURI_LOG_I(TAG, "5: start: %d, end %d", start, end);
                    }

                    // EventInterchangeFlag
                    event_key = "EventInterchangeFlag";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].interchange_flag =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        FURI_LOG_I(TAG, "6: start: %d, end %d", start, end);
                    }

                    // EventRouteNumber
                    event_key = "EventRouteNumber";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].route_number =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        card->ravkav->events[i - 1].route_number_available = true;
                        FURI_LOG_I(TAG, "7: start: %d, end %d", start, end);
                    }

                    // EventRouteNumber
                    event_key = "EventfareCode";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].fare_code =
                            bit_slice_to_dec(event_bit_representation, start, end);
                        card->ravkav->events[i - 1].fare_code = true;
                        FURI_LOG_I(TAG, "8: start: %d, end %d", start, end);
                    }

                    // EventRouteNumber
                    event_key = "EventDebitAmount";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        card->ravkav->events[i - 1].debit_amount =
                            bit_slice_to_dec(event_bit_representation, start, end) / 100.0;
                        card->ravkav->events[i - 1].debit_amount_available = true;
                        FURI_LOG_I(TAG, "9: start: %d, end %d", start, end);
                    }

                    // EventRouteNumber
                    event_key = "Location";
                    if(is_calypso_node_present(
                           event_bit_representation, event_key, RavKavEventStructure)) {
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, RavKavEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, RavKavEventStructure) - 1;
                        FURI_LOG_I(TAG, "location : start: %d, end %d", start, end);
                        FURI_LOG_I(
                            TAG,
                            "locatrion bitmap: %d",
                            bit_slice_to_dec(event_bit_representation, start, end));
                    }

                    /* This event slot (events[i-1]) is now populated -
                             * record the real count so the display doesn't show
                             * empty/garbage event pages. */
                    card->events_count = i;
                }

                // Free the calypso structure
                free_calypso_structure(RavKavEventStructure);

                break;
            }
        }
        default:
            break;
        }

        CalypsoContext* context = malloc(sizeof(CalypsoContext));
        if(!context) {
            stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            break;
        }
        context->card = card;
        context->page_id = 0;
        context->mutex = NULL;
        app->calypso_context = context;

        stage = MetroflipPollerEventTypeSuccess;
    } while(false);

    if(stage != MetroflipPollerEventTypeSuccess) {
        // Clean up card data on failure if it was allocated but not assigned to context
        if(card && !app->calypso_context) {
            free(card->navigo);
            free(card->opus);
            free(card->ravkav);
            free(card);
        }
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return stage == MetroflipPollerEventTypeSuccess;
}

static NfcCommand calypso_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4b);
    Metroflip* app = context;
    const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;

    if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
        /* Parse on the poller thread, then hand off to the main thread:
         * the UI must never be built from here. */
        if(calypso_read_card(app, event.instance)) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        }
        /* Failure paths already sent their event (PollerFail/FileNotFound). */
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void calypso_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);
    beginning = true;
    select_app[0] = 0x94;
    select_app[2] = 0x00;
    read_file[0] = 0x94;

    if(app->data_loaded) {
        /* File replay: parse synchronously from the saved file - no poller. */
        if(calypso_read_card(app, NULL)) {
            calypso_display_card_view(app);
        } else {
            FURI_LOG_E(TAG, "Failed to parse saved file: %s", app->delete_file_path);
            Widget* widget = app->widget;
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, "\e#Error\nFailed to parse\nsaved file.");
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
    } else {
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);

        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4b);
        nfc_poller_start(app->poller, calypso_poller_callback, app);
        metroflip_app_blink_start(app);
    }
}

static bool calypso_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Live read finished on the poller thread (NfcCommandStop was
             * returned) - tear the poller down and build the card view from
             * the main thread. */
            if(app->poller && !app->data_loaded) {
                nfc_poller_stop(app->poller);
                nfc_poller_free(app->poller);
                app->poller = NULL;
            }
            metroflip_app_blink_stop(app);
            calypso_display_card_view(app);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card found!\nDon't move...", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card lost!\nTry again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFileNotFound) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read Error,\n wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail && app->data_loaded) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Bad File.", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Error, try\n again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        furi_string_reset(app->calypso_file_data);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void calypso_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    if(app->calypso_context) {
        CalypsoContext* ctx = app->calypso_context;
        if(ctx->card) {
            free(ctx->card->navigo);
            free(ctx->card->opus);
            free(ctx->card->ravkav);
            free(ctx->card);
        }
        free(ctx);
        app->calypso_context = NULL;
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin calypso_plugin = {
    .card_name = "Calypso",
    .plugin_on_enter = calypso_on_enter,
    .plugin_on_event = calypso_on_event,
    .plugin_on_exit = calypso_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor calypso_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &calypso_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* calypso_plugin_ep(void) {
    return &calypso_plugin_descriptor;
}
