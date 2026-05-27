#pragma once

typedef enum {
    NfcProtocolIso14443_3a = 0,
    NfcProtocolIso14443_3b = 1,
    NfcProtocolIso14443_4a = 2,
    NfcProtocolIso14443_4b = 3,
    NfcProtocolIso15693_3 = 4,
    NfcProtocolFelica = 5,
    NfcProtocolMfUltralight = 6,
    NfcProtocolMfClassic = 7,
    NfcProtocolMfPlus = 8,
    NfcProtocolMfDesfire = 9,
    NfcProtocolSlix = 10,
    NfcProtocolSt25tb = 11,
    NfcProtocolNum = 12,
    NfcProtocolInvalid = 13,
} NfcProtocol;
