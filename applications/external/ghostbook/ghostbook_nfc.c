/*
 * GhostBook v0.6.0
 * Secure encrypted contacts for Flipper Zero
 * 
 * Original Author: Digi
 * Created: December 2025
 * License: MIT
 * 
 * If you fork this, keep this header intact.
 * https://github.com/digitard/ghostbook
 */

#include "ghostbook.h"
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_listener.h>

// ============================================================================
// NTAG215 CONSTANTS
// ============================================================================
// NTAG215 has 135 pages of 4 bytes each = 540 bytes total
// Pages 0-3: UID, internal, lock bytes, capability container
// Pages 4-129: User data (504 bytes) - we use this
// Pages 130-134: Dynamic lock, config, password

#define GHOST_NFC_MAGIC       "GHST" // Magic bytes to identify our cards
#define GHOST_NFC_VERSION     1
#define GHOST_DATA_START_PAGE 4
#define GHOST_DATA_PAGES      126 // Pages 4-129
#define GHOST_MAX_DATA        (GHOST_DATA_PAGES * 4) // 504 bytes

// ============================================================================
// CARD SERIALIZATION FOR NFC
// ============================================================================

static size_t ghost_card_to_nfc_data(GhostCard* card, uint8_t* buffer, size_t max_len) {
    if(max_len < 8) return 0;

    // Header: GHST + version + length (2 bytes) + reserved
    buffer[0] = 'G';
    buffer[1] = 'H';
    buffer[2] = 'S';
    buffer[3] = 'T';
    buffer[4] = GHOST_NFC_VERSION;
    // bytes 5-6 will be length, filled later
    buffer[7] = 0; // reserved

    // Build payload starting at byte 8
    FuriString* str = furi_string_alloc();

    furi_string_printf(str, "h=%s\n", card->handle);
    if(strlen(card->name)) furi_string_cat_printf(str, "n=%s\n", card->name);
    if(strlen(card->email)) furi_string_cat_printf(str, "e=%s\n", card->email);
    if(strlen(card->discord)) furi_string_cat_printf(str, "d=%s\n", card->discord);
    if(strlen(card->signal)) furi_string_cat_printf(str, "s=%s\n", card->signal);
    if(strlen(card->telegram)) furi_string_cat_printf(str, "g=%s\n", card->telegram);
    if(strlen(card->notes)) furi_string_cat_printf(str, "t=%s\n", card->notes);
    if(card->flair > 0) furi_string_cat_printf(str, "f=%d\n", card->flair);

    size_t payload_len = furi_string_size(str);
    if(payload_len > max_len - 8) payload_len = max_len - 8;

    memcpy(buffer + 8, furi_string_get_cstr(str), payload_len);
    furi_string_free(str);

    // Fill in length
    size_t total_len = 8 + payload_len;
    buffer[5] = (payload_len >> 8) & 0xFF;
    buffer[6] = payload_len & 0xFF;

    FURI_LOG_I(TAG, "Serialized card: %zu bytes", total_len);
    return total_len;
}

static bool ghost_card_from_nfc_data(const uint8_t* data, size_t len, GhostCard* card) {
    // Check minimum length and magic
    if(len < 8) {
        FURI_LOG_W(TAG, "Data too short: %zu", len);
        return false;
    }

    if(data[0] != 'G' || data[1] != 'H' || data[2] != 'S' || data[3] != 'T') {
        FURI_LOG_W(TAG, "Invalid magic: %c%c%c%c", data[0], data[1], data[2], data[3]);
        return false;
    }

    if(data[4] != GHOST_NFC_VERSION) {
        FURI_LOG_W(TAG, "Unknown version: %d", data[4]);
        return false;
    }

    uint16_t payload_len = (data[5] << 8) | data[6];
    if(payload_len > len - 8) {
        FURI_LOG_W(TAG, "Payload length mismatch: %d vs %zu", payload_len, len - 8);
        payload_len = len - 8;
    }

    // Parse payload
    char* str = malloc(payload_len + 1);
    if(!str) return false;

    memcpy(str, data + 8, payload_len);
    str[payload_len] = '\0';

    // Initialize card
    ghost_card_init(card);

    // Parse fields (compact format: h=handle, n=name, e=email, d=discord, s=signal, g=telegram, t=notes, f=flair)
    char* line = str;
    char* next;

    while(line && *line) {
        next = strchr(line, '\n');
        if(next) *next++ = '\0';

        if(strlen(line) >= 2 && line[1] == '=') {
            char* value = line + 2;
            switch(line[0]) {
            case 'h':
                strncpy(card->handle, value, GHOST_HANDLE_LEN - 1);
                break;
            case 'n':
                strncpy(card->name, value, GHOST_NAME_LEN - 1);
                break;
            case 'e':
                strncpy(card->email, value, GHOST_EMAIL_LEN - 1);
                break;
            case 'd':
                strncpy(card->discord, value, GHOST_DISCORD_LEN - 1);
                break;
            case 's':
                strncpy(card->signal, value, GHOST_SIGNAL_LEN - 1);
                break;
            case 'g':
                strncpy(card->telegram, value, GHOST_TELEGRAM_LEN - 1);
                break;
            case 't':
                strncpy(card->notes, value, GHOST_NOTES_LEN - 1);
                break;
            case 'f':
                card->flair = atoi(value);
                if(card->flair >= GhostFlairNum) card->flair = GhostFlairNone;
                break;
            }
        }
        line = next;
    }

    free(str);

    bool valid = strlen(card->handle) > 0;
    FURI_LOG_I(TAG, "Parsed card: @%s valid=%d", card->handle, valid);
    return valid;
}

// ============================================================================
// NFC LISTENER (SHARE MODE) - Emulate NTAG215 with our data
// ============================================================================

static MfUltralightData* ghost_create_ntag215_data(GhostApp* app) {
    MfUltralightData* data = mf_ultralight_alloc();

    // Set type to NTAG215
    data->type = MfUltralightTypeNTAG215;

    // Generate random UID (7 bytes for NTAG)
    uint8_t uid[7];
    furi_hal_random_fill_buf(uid, 7);
    uid[0] = 0x04; // NXP manufacturer

    // Page 0-1: UID
    data->page[0].data[0] = uid[0];
    data->page[0].data[1] = uid[1];
    data->page[0].data[2] = uid[2];
    data->page[0].data[3] = uid[0] ^ uid[1] ^ uid[2] ^ 0x88; // BCC0

    data->page[1].data[0] = uid[3];
    data->page[1].data[1] = uid[4];
    data->page[1].data[2] = uid[5];
    data->page[1].data[3] = uid[6];

    // Page 2: BCC1, internal, lock bytes
    data->page[2].data[0] = uid[3] ^ uid[4] ^ uid[5] ^ uid[6]; // BCC1
    data->page[2].data[1] = 0x48; // Internal
    data->page[2].data[2] = 0x00; // Lock byte 0
    data->page[2].data[3] = 0x00; // Lock byte 1

    // Page 3: Capability container
    data->page[3].data[0] = 0xE1; // Magic for NDEF
    data->page[3].data[1] = 0x10; // Version 1.0
    data->page[3].data[2] = 0x3E; // Size (504/8 = 63 = 0x3F, minus 1)
    data->page[3].data[3] = 0x00; // Read/write access

    // Serialize our card data
    uint8_t card_data[GHOST_MAX_DATA];
    memset(card_data, 0, GHOST_MAX_DATA);
    size_t card_len = ghost_card_to_nfc_data(&app->my_card, card_data, GHOST_MAX_DATA);

    // Write card data to pages 4-129
    for(int page = 0; page < GHOST_DATA_PAGES && (page * 4) < (int)card_len; page++) {
        for(int i = 0; i < 4; i++) {
            size_t offset = page * 4 + i;
            data->page[GHOST_DATA_START_PAGE + page].data[i] =
                (offset < card_len) ? card_data[offset] : 0;
        }
    }

    // Set pages count
    data->pages_read = 135;
    data->pages_total = 135;

    FURI_LOG_I(TAG, "Created NTAG215 with %zu bytes of card data", card_len);

    return data;
}

typedef struct {
    GhostApp* app;
    MfUltralightData* mfu_data;
} GhostNfcShareContext;

static GhostNfcShareContext* share_ctx = NULL;

static NfcCommand ghost_nfc_share_callback(NfcGenericEvent event, void* context) {
    GhostNfcShareContext* ctx = context;
    GhostApp* app = ctx->app;

    MfUltralightListenerEvent* mfu_event = event.event_data;

    if(mfu_event->type == MfUltralightListenerEventTypeAuth) {
        // Someone is reading us!
        notification_message(app->notif, &sequence_blink_cyan_10);
        FURI_LOG_I(TAG, "NFC: Auth event - card being read");
    }

    return app->nfc_active ? NfcCommandContinue : NfcCommandStop;
}

void ghost_nfc_share_start(GhostApp* app) {
    FURI_LOG_I(TAG, "Starting NFC share (NTAG215 emulation)...");

    // Allocate NFC
    app->nfc = nfc_alloc();

    // Create context
    share_ctx = malloc(sizeof(GhostNfcShareContext));
    share_ctx->app = app;
    share_ctx->mfu_data = ghost_create_ntag215_data(app);

    // Start listener with NTAG215 data
    app->nfc_listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, share_ctx->mfu_data);

    nfc_listener_start(app->nfc_listener, ghost_nfc_share_callback, share_ctx);

    app->nfc_active = true;

    FURI_LOG_I(TAG, "NFC share active - emulating NTAG215");
}

void ghost_nfc_share_stop(GhostApp* app) {
    FURI_LOG_I(TAG, "Stopping NFC share...");

    app->nfc_active = false;

    if(app->nfc_listener) {
        nfc_listener_stop(app->nfc_listener);
        nfc_listener_free(app->nfc_listener);
        app->nfc_listener = NULL;
    }

    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    if(share_ctx) {
        if(share_ctx->mfu_data) {
            mf_ultralight_free(share_ctx->mfu_data);
        }
        free(share_ctx);
        share_ctx = NULL;
    }

    FURI_LOG_I(TAG, "NFC share stopped");
}

// ============================================================================
// NFC POLLER (RECEIVE MODE) - Read NTAG215 tags
// ============================================================================

typedef struct {
    GhostApp* app;
    bool done;
} GhostNfcReceiveContext;

static GhostNfcReceiveContext* receive_ctx = NULL;

static NfcCommand ghost_nfc_receive_callback(NfcGenericEvent event, void* context) {
    GhostNfcReceiveContext* ctx = context;
    GhostApp* app = ctx->app;

    MfUltralightPollerEvent* mfu_event = event.event_data;

    if(mfu_event->type == MfUltralightPollerEventTypeReadSuccess) {
        FURI_LOG_I(TAG, "NFC: Read success!");
        notification_message(app->notif, &sequence_blink_green_10);

        // Get the read data
        const MfUltralightData* mfu_data = nfc_poller_get_data(app->nfc_poller);

        if(mfu_data && mfu_data->pages_read >= GHOST_DATA_START_PAGE + 2) {
            // Extract data from pages 4+
            uint8_t card_data[GHOST_MAX_DATA];
            size_t data_len = 0;

            for(int page = GHOST_DATA_START_PAGE; page < (int)mfu_data->pages_read &&
                                                  page < GHOST_DATA_START_PAGE + GHOST_DATA_PAGES;
                page++) {
                for(int i = 0; i < 4; i++) {
                    card_data[data_len++] = mfu_data->page[page].data[i];
                }
            }

            FURI_LOG_I(TAG, "Read %zu bytes from tag", data_len);

            // Try to parse as GhostCard
            if(ghost_card_from_nfc_data(card_data, data_len, &app->received_card)) {
                app->card_received = true;
                FURI_LOG_I(TAG, "Successfully received card: @%s", app->received_card.handle);
            } else {
                FURI_LOG_W(TAG, "Tag is not a GhostBook card");
            }
        }

        ctx->done = true;
        return NfcCommandStop;
    } else if(mfu_event->type == MfUltralightPollerEventTypeReadFailed) {
        FURI_LOG_W(TAG, "NFC: Read failed");
        notification_message(app->notif, &sequence_blink_red_10);
        // Continue scanning
    }

    return (app->nfc_active && !ctx->done) ? NfcCommandContinue : NfcCommandStop;
}

void ghost_nfc_receive_start(GhostApp* app) {
    FURI_LOG_I(TAG, "Starting NFC receive (NTAG reader)...");

    // Allocate NFC
    app->nfc = nfc_alloc();
    app->card_received = false;

    // Create context
    receive_ctx = malloc(sizeof(GhostNfcReceiveContext));
    receive_ctx->app = app;
    receive_ctx->done = false;

    // Start poller for MfUltralight (includes NTAG)
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->nfc_poller, ghost_nfc_receive_callback, receive_ctx);

    app->nfc_active = true;

    FURI_LOG_I(TAG, "NFC receive active - scanning for NTAG");
}

void ghost_nfc_receive_stop(GhostApp* app) {
    FURI_LOG_I(TAG, "Stopping NFC receive...");

    app->nfc_active = false;

    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }

    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    if(receive_ctx) {
        free(receive_ctx);
        receive_ctx = NULL;
    }

    FURI_LOG_I(TAG, "NFC receive stopped");
}
