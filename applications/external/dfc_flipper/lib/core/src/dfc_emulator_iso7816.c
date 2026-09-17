/* ISO 7816 SELECT, which the standard-class path shares with every
 * transport. The Flipper listener and the host virtual PICC both call it. */
#include "dfc_emulator_i.h"

#define TAG                  DFC_EMULATOR_TAG
#define ISO14443_4A_CID_MASK DFC_ISO14443_4A_CID_MASK
#define ISO14443_4A_NAD_MASK DFC_ISO14443_4A_NAD_MASK


bool dfc_emulator_handle_iso7816_select(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    size_t prefix_len,
    bool* app_selected) {
    if(apdu_len < 4 || apdu[0] != DFC_ISO7816_CLA_STANDARD) return false;
    if(app_selected) *app_selected = false;

    if(apdu[1] != DFC_ISO7816_INS_SELECT) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI);
        dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO);
        return true;
    }

    uint8_t p1 = apdu[2];
    uint8_t lc = (apdu_len > 4) ? apdu[4] : 0;
    const uint8_t* data = (lc > 0 && apdu_len >= (size_t)(5 + lc)) ? &apdu[5] : NULL;
    DFC_LOG_T(TAG, "ISO7816 SELECT p1=%02X lc=%u data_present=%u", p1, lc, data ? 1U : 0U);

    if(p1 == 0x04 && data) {
        /* Applications first, then the PICC-level well-known name.
         * section 1.3: a name an application carries selects that application,
         * never the PICC level. Real credentials do carry DFC_ISO_AID as their own
         * DF name, so checking it first would answer 90 00 and then serve every
         * following command from the wrong level. */
        for(size_t i = 0; i < emulator->credential->num_apps; i++) {
            DfcApplication* app = &emulator->credential->apps[i];
            if(app->iso_aid_len > 0 && lc == app->iso_aid_len && memcmp(data, app->iso_aid, lc) == 0) {
                DFC_LOG_T(TAG, "ISO7816 SELECT matched app index %u", (unsigned int)i);
                dfc_emulator_reset_session(emulator);
                emulator->selected_application = DfcEmulatorSelectedApplicationApp;
                emulator->selected_app_index = i;
                dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_OK_HI);
                dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_OK_LO);
                if(app_selected) *app_selected = true;
                return true;
            }
        }

        if(lc == sizeof(DFC_ISO_AID) && memcmp(data, DFC_ISO_AID, lc) == 0) {
            DFC_LOG_T(TAG, "ISO7816 SELECT matched HID ISO AID");
            dfc_emulator_reset_session(emulator);
            emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
            dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_OK_HI);
            dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_OK_LO);
            return true;
        }
    }

    DFC_LOG_T(TAG, "ISO7816 SELECT not found p1=%02X lc=%u", p1, lc);
    dfc_emulator_reset_session(emulator);
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
    dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_NOT_FOUND_HI);
    dfc_bytebuf_append_byte(tx_buffer, DFC_ISO7816_SW_NOT_FOUND_LO);
    return dfc_bytebuf_get_size_bytes(tx_buffer) > prefix_len;
}
