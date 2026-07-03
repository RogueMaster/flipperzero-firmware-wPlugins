/* ccid_handler.c — USB CCID protocol handler for CCID Emulator.
 * Processes CCID bulk transfers and dispatches APDU commands to card profiles. */

#include "ccid_handler.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_ccid.h>

#include <stdio.h>
#include <string.h>

/* USB CCID spec §6.1: maximum data block length is 261 bytes */
#define CCID_MAX_DATA_BLOCK_LEN 261

/* ---------------------------------------------------------------------------
 * USB VID/PID presets (keep in sync with the settings menu)
 * --------------------------------------------------------------------------- */

const CcidUsbPreset ccid_usb_presets[] = {
    {.label = "Default (1234:5678)", .vid = 0x1234, .pid = 0x5678},
    {.label = "Generic Reader", .vid = 0x076B, .pid = 0x3021},
    {.label = "Yubikey", .vid = 0x1050, .pid = 0x0407},
};

const uint8_t ccid_usb_preset_count = sizeof(ccid_usb_presets) / sizeof(ccid_usb_presets[0]);

/* ---------------------------------------------------------------------------
 * Utility: format byte array as space-separated hex string
 * --------------------------------------------------------------------------- */

static void bytes_to_hex_str(const uint8_t* data, uint32_t len, char* out, size_t out_max) {
    size_t pos = 0;
    for(uint32_t i = 0; i < len; i++) {
        if(pos >= out_max - 1) break;
        if(i > 0) {
            if(pos >= out_max - 1) break;
            out[pos++] = ' ';
        }
        if(pos >= out_max - 1) break;
        int written = snprintf(out + pos, out_max - pos, "%02X", data[i]);
        if(written > 0) {
            pos += (size_t)written;
        }
        if(pos >= out_max - 1) break;
    }
    if(pos < out_max) {
        out[pos] = '\0';
    } else {
        out[out_max - 1] = '\0';
    }
}

/* ---------------------------------------------------------------------------
 * Rule matching
 * --------------------------------------------------------------------------- */

/**
 * Try to match an incoming APDU against the loaded card's rules.
 *
 * Returns the index of the first matching rule, or -1 if no match.
 */
static int match_rule(const CcidCard* card, const uint8_t* cmd, uint32_t cmd_len) {
    for(uint16_t i = 0; i < card->rule_count; i++) {
        const CcidRule* rule = &card->rules[i];

        if(rule->command_len != (uint16_t)cmd_len) continue;

        bool match = true;
        for(uint16_t j = 0; j < rule->command_len; j++) {
            if(rule->mask[j] == 0x00) continue; /* wildcard -- skip */
            if(rule->command[j] != cmd[j]) {
                match = false;
                break;
            }
        }
        if(match) return (int)i;
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Logging helper
 * --------------------------------------------------------------------------- */

/* log_apdu_exchange() removed — logging is now done inline in
 * ccid_xfr_datablock() with a volatile dirty flag polled by a FuriTimer,
 * avoiding the furi_check crash from calling view_dispatcher_send_custom_event
 * on the USB callback thread under heavy APDU load. */

/* ---------------------------------------------------------------------------
 * CCID Callbacks
 * --------------------------------------------------------------------------- */

static void ccid_icc_power_on(uint8_t* dataBlock, uint32_t* dataBlockLen, void* context) {
    CcidEmulatorApp* app = context;
    furi_assert(app);
    /* card pointer is set before emulation starts and immutable during emulation */
    furi_assert(app->card);

    if(app->card->atr_len > 0) {
        uint32_t copy_len = app->card->atr_len;
        if(copy_len > CCID_EMU_MAX_ATR_LEN) copy_len = CCID_EMU_MAX_ATR_LEN;
        memcpy(dataBlock, app->card->atr, copy_len);
        *dataBlockLen = copy_len;
    } else {
        /* Fallback minimal ATR: direct convention, T=0 */
        dataBlock[0] = 0x3B;
        dataBlock[1] = 0x00;
        *dataBlockLen = 2;
    }

    FURI_LOG_I("CcidHandler", "ICC Power ON, ATR len=%lu", (unsigned long)*dataBlockLen);
}

static void ccid_xfr_datablock(
    const uint8_t* pcToReaderDataBlock,
    uint32_t pcToReaderDataBlockLen,
    uint8_t* readerToPcDataBlock,
    uint32_t* readerToPcDataBlockLen,
    void* context) {
    CcidEmulatorApp* app = context;
    furi_assert(app);
    /* card pointer is set before emulation starts and immutable during emulation */
    furi_assert(app->card);

    const uint8_t* cmd = pcToReaderDataBlock;
    uint32_t cmd_len = pcToReaderDataBlockLen;

    int rule_idx = match_rule(app->card, cmd, cmd_len);
    bool matched;

    if(rule_idx >= 0) {
        const CcidRule* rule = &app->card->rules[rule_idx];
        uint32_t copy_len = rule->response_len;
        if(copy_len > CCID_MAX_DATA_BLOCK_LEN) copy_len = CCID_MAX_DATA_BLOCK_LEN;
        memcpy(readerToPcDataBlock, rule->response, copy_len);
        *readerToPcDataBlockLen = copy_len;
        matched = true;

        FURI_LOG_D("CcidHandler", "Rule %d matched", rule_idx);
    } else {
        /* No rule matched -- send default response */
        if(app->card->default_response_len > 0) {
            uint32_t copy_len = app->card->default_response_len;
            if(copy_len > CCID_MAX_DATA_BLOCK_LEN) copy_len = CCID_MAX_DATA_BLOCK_LEN;
            memcpy(readerToPcDataBlock, app->card->default_response, copy_len);
            *readerToPcDataBlockLen = copy_len;
        } else {
            /* Fallback: SW 6D00 = Instruction not supported */
            readerToPcDataBlock[0] = 0x6D;
            readerToPcDataBlock[1] = 0x00;
            *readerToPcDataBlockLen = 2;
        }
        matched = false;

        FURI_LOG_D("CcidHandler", "No rule matched, sending default response");
    }

    /* Log the exchange.  Use a short mutex timeout so we never stall the USB
     * driver, and set a volatile flag instead of calling
     * view_dispatcher_send_custom_event from this thread (which can overflow
     * the event queue under heavy APDU load and trigger furi_check). */
    if(furi_mutex_acquire(app->log_mutex, furi_ms_to_ticks(5)) == FuriStatusOk) {
        uint16_t idx = app->log_count % CCID_EMU_LOG_MAX_ENTRIES;
        CcidApduLogEntry* entry = &app->log_entries[idx];

        entry->timestamp = furi_get_tick();
        bytes_to_hex_str(cmd, cmd_len, entry->command_hex, sizeof(entry->command_hex));
        bytes_to_hex_str(
            readerToPcDataBlock,
            *readerToPcDataBlockLen,
            entry->response_hex,
            sizeof(entry->response_hex));
        entry->matched = matched;

        app->log_count++;
        app->log_dirty = true;

        furi_mutex_release(app->log_mutex);
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void ccid_handler_start(CcidEmulatorApp* app) {
    furi_assert(app);
    furi_assert(app->card);
    furi_assert(!app->emulating);

    /* Configure callback struct (but don't register yet — CCID mode must be active first) */
    app->ccid_callbacks.icc_power_on_callback = ccid_icc_power_on;
    app->ccid_callbacks.xfr_datablock_callback = ccid_xfr_datablock;

    /* TODO: VID/PID customization is not supported by the SDK's usb_ccid
       interface.  If needed, a custom FuriHalUsbInterface would be required.
       For now we use the default usb_ccid descriptor. */
    const CcidUsbPreset* preset = &ccid_usb_presets[app->usb_preset_index];
    (void)preset; /* suppress unused warning until custom descriptors are supported */

    /* Save current USB interface so we can restore it later */
    app->prev_usb_if = furi_hal_usb_get_config();

    /* Switch USB to CCID mode FIRST — the CCID subsystem must be active
     * before we can register callbacks or insert a smartcard.
     * Setting callbacks before switching triggers furi_check. */
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_ccid, NULL);

    /* Now register callbacks (CCID mode is active) */
    furi_hal_usb_ccid_set_callbacks(&app->ccid_callbacks, app);

    /* Insert virtual smartcard */
    furi_hal_usb_ccid_insert_smartcard();

    app->emulating = true;
    FURI_LOG_I("CcidHandler", "CCID emulation started");
}

void ccid_handler_stop(CcidEmulatorApp* app) {
    furi_assert(app);

    if(!app->emulating) return;

    /* Remove virtual smartcard */
    furi_hal_usb_ccid_remove_smartcard();

    /* Clear callbacks BEFORE switching USB mode — calling CCID functions
     * after the USB interface has been switched away from CCID triggers
     * furi_check on Momentum firmware. */
    furi_hal_usb_ccid_set_callbacks(NULL, NULL);

    /* Restore previous USB interface */
    furi_hal_usb_unlock();
    if(app->prev_usb_if) {
        furi_hal_usb_set_config(app->prev_usb_if, NULL);
        app->prev_usb_if = NULL;
    }

    app->emulating = false;
    FURI_LOG_I("CcidHandler", "CCID emulation stopped");
}
