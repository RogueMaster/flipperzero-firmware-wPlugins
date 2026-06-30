#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#define FLIPPER_WEDGE_NFC_UID_MAX_LEN 10
#define FLIPPER_WEDGE_NDEF_MAX_LEN 1024  // Buffer size (max user setting is 1000 chars, +24 for safety)

typedef struct FlipperWedgeNfc FlipperWedgeNfc;

typedef enum {
    FlipperWedgeNfcErrorNone,            // Success
    FlipperWedgeNfcErrorNotForumCompliant, // Tag is not NFC Forum compliant (e.g., MIFARE Classic)
    FlipperWedgeNfcErrorUnsupportedType, // Tag detected but unsupported NFC Forum Type for NDEF
    FlipperWedgeNfcErrorNoTextRecord,    // Supported type but no NDEF text record found
} FlipperWedgeNfcError;

typedef struct {
    uint8_t uid[FLIPPER_WEDGE_NFC_UID_MAX_LEN];
    uint8_t uid_len;
    char ndef_text[FLIPPER_WEDGE_NDEF_MAX_LEN];
    bool has_ndef;
    FlipperWedgeNfcError error;
} FlipperWedgeNfcData;

typedef void (*FlipperWedgeNfcCallback)(FlipperWedgeNfcData* data, void* context);

/** Allocate NFC reader
 *
 * @return FlipperWedgeNfc instance
 */
FlipperWedgeNfc* flipper_wedge_nfc_alloc(void);

/** Free NFC reader
 *
 * @param instance FlipperWedgeNfc instance
 */
void flipper_wedge_nfc_free(FlipperWedgeNfc* instance);

/** Set callback for tag detection
 *
 * @param instance FlipperWedgeNfc instance
 * @param callback Callback function
 * @param context Callback context
 */
void flipper_wedge_nfc_set_callback(
    FlipperWedgeNfc* instance,
    FlipperWedgeNfcCallback callback,
    void* context);

/** Start NFC scanning
 *
 * @param instance FlipperWedgeNfc instance
 * @param parse_ndef Whether to parse NDEF records
 */
void flipper_wedge_nfc_start(FlipperWedgeNfc* instance, bool parse_ndef);

/** Stop NFC scanning
 *
 * @param instance FlipperWedgeNfc instance
 */
void flipper_wedge_nfc_stop(FlipperWedgeNfc* instance);

/** Check if NFC is currently scanning
 *
 * @param instance FlipperWedgeNfc instance
 * @return true if scanning
 */
bool flipper_wedge_nfc_is_scanning(FlipperWedgeNfc* instance);

/** Process NFC state machine from main thread
 * Call this in the tick event handler to safely process NFC events
 *
 * @param instance FlipperWedgeNfc instance
 * @return true if a tag was successfully read
 */
bool flipper_wedge_nfc_tick(FlipperWedgeNfc* instance);
