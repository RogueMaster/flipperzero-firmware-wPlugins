#include "card_info.h"

#include <nfc/protocols/nfc_protocol.h>
#include <stdio.h>

MToolsCardProtocols mtools_card_protocols(const NfcScannerEvent* event) {
    MToolsCardProtocols result = {0};
    if(!event || event->type != NfcScannerEventTypeDetected) return result;
    for(size_t i = 0; i < event->data.protocol_num; i++) {
        NfcProtocol protocol = event->data.protocols[i];
        if(protocol == NfcProtocolMfUltralight ||
           nfc_protocol_has_parent(protocol, NfcProtocolMfUltralight))
            result.type2 = true;
        if(protocol == NfcProtocolMfClassic || protocol == NfcProtocolIso14443_3a ||
           nfc_protocol_has_parent(protocol, NfcProtocolIso14443_3a))
            result.iso14443a = true;
        if(protocol == NfcProtocolIso15693_3 ||
           nfc_protocol_has_parent(protocol, NfcProtocolIso15693_3))
            result.iso15693 = true;
    }
    return result;
}

const char* mtools_iso15_chip_name(const uint8_t* uid, size_t uid_len) {
    if(!uid || uid_len != 8 || uid[0] != 0xE0) return "ISO15693";
    if(uid[1] == 0x04) {
        if(uid[2] == 0x01) {
            switch(uid[3] & 0x18) {
            case 0x00: return "ICODE SLI";
            case 0x10: return "ICODE SLIX";
            case 0x08: return "ICODE SLIX2";
            default: return "NXP ISO15";
            }
        }
        if(uid[2] == 0x02) return (uid[3] & 0x10) ? "ICODE SLIX-S" : "ICODE SLI-S";
        if(uid[2] == 0x03) return (uid[3] & 0x10) ? "ICODE SLIX-L" : "ICODE SLI-L";
        return "NXP ISO15";
    }
    if(uid[1] == 0x07) {
        if((uid[2] & 0xF0) == 0x00 || (uid[2] & 0xF0) == 0x10 ||
           (uid[2] & 0xF8) == 0x80)
            return "Tag-it HF-I PLUS";
        if((uid[2] & 0xFC) == 0xC4) return "Tag-it HF-I Pro";
        if((uid[2] & 0xF8) == 0xC0) return "Tag-it HF-I Std";
        return "TI ISO15";
    }
    return "ISO15693";
}

void mtools_uid_format_hex(char* output, size_t size, const uint8_t* uid, size_t uid_len) {
    if(!output || size == 0) return;
    output[0] = '\0';
    if(!uid) return;
    size_t used = 0;
    for(size_t i = 0; i < uid_len && size - used > 2; i++) {
        int written = snprintf(output + used, size - used, "%02X", uid[i]);
        if(written != 2) break;
        used += 2;
    }
}
