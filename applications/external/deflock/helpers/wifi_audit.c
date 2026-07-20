// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "wifi_audit.h"

// esp wifi_auth_mode_t values (stable low range across IDF versions).
#define AUTH_OPEN          0
#define AUTH_WEP           1
#define AUTH_WPA_PSK       2
#define AUTH_WPA2_PSK      3
#define AUTH_WPA_WPA2_PSK  4
#define AUTH_ENTERPRISE    5
#define AUTH_WPA3_PSK      6
#define AUTH_WPA2_WPA3_PSK 7
#define AUTH_WAPI_PSK      8
#define AUTH_OWE           9

// esp wifi_cipher_type_t values that include TKIP.
#define CIPHER_TKIP      3
#define CIPHER_TKIP_CCMP 5

const char* wifi_auth_str(uint8_t authmode) {
    switch(authmode) {
    case AUTH_OPEN:
        return "Open";
    case AUTH_WEP:
        return "WEP";
    case AUTH_WPA_PSK:
        return "WPA";
    case AUTH_WPA2_PSK:
        return "WPA2";
    case AUTH_WPA_WPA2_PSK:
        return "WPA/2";
    case AUTH_ENTERPRISE:
        return "WPA2-E";
    case AUTH_WPA3_PSK:
        return "WPA3";
    case AUTH_WPA2_WPA3_PSK:
        return "WPA2/3";
    case AUTH_WAPI_PSK:
        return "WAPI";
    case AUTH_OWE:
        return "OWE";
    default:
        return "?";
    }
}

const char* wifi_grade_str(WifiGrade grade) {
    switch(grade) {
    case WifiGradeCritical:
        return "CRIT";
    case WifiGradeWeak:
        return "WEAK";
    case WifiGradeOk:
        return "OK";
    case WifiGradeStrong:
        return "STRONG";
    case WifiGradeInfo:
    default:
        return "INFO";
    }
}

static WifiGrade worse(WifiGrade a, WifiGrade b) {
    return (a > b) ? a : b;
}

WifiGrade wifi_audit_grade(
    uint8_t authmode,
    uint8_t pairwise,
    bool wps,
    const char* ssid,
    FuriString* reasons) {
    WifiGrade grade;

    switch(authmode) {
    case AUTH_OPEN:
        grade = WifiGradeCritical;
        if(reasons) furi_string_cat(reasons, "Open: traffic unencrypted\n");
        break;
    case AUTH_WEP:
        grade = WifiGradeCritical;
        if(reasons) furi_string_cat(reasons, "WEP: trivially cracked\n");
        break;
    case AUTH_WPA_PSK:
        grade = WifiGradeWeak;
        if(reasons) furi_string_cat(reasons, "WPA1: deprecated, weak\n");
        break;
    case AUTH_WPA_WPA2_PSK:
        grade = WifiGradeWeak;
        if(reasons) furi_string_cat(reasons, "WPA/WPA2 mixed: allows WPA1\n");
        break;
    case AUTH_WPA2_PSK:
        grade = WifiGradeOk;
        if(reasons)
            furi_string_cat(reasons, "WPA2-PSK: capture handshake/\nPMKID -> offline crack\n");
        break;
    case AUTH_ENTERPRISE:
        grade = WifiGradeStrong;
        if(reasons) furi_string_cat(reasons, "WPA2-Enterprise (EAP)\n");
        break;
    case AUTH_WPA3_PSK:
        grade = WifiGradeStrong;
        if(reasons) furi_string_cat(reasons, "WPA3-SAE: strong\n");
        break;
    case AUTH_WPA2_WPA3_PSK:
        grade = WifiGradeStrong;
        if(reasons) furi_string_cat(reasons, "WPA2/WPA3 transitional\n");
        break;
    case AUTH_OWE:
        grade = WifiGradeOk;
        if(reasons) furi_string_cat(reasons, "OWE: encrypted but open auth\n");
        break;
    default:
        grade = WifiGradeInfo;
        if(reasons) furi_string_cat(reasons, "Unrecognised auth mode\n");
        break;
    }

    if(wps) {
        grade = worse(grade, WifiGradeWeak);
        if(reasons) furi_string_cat(reasons, "WPS on: PIN/Pixie-Dust risk\n");
    }
    if(pairwise == CIPHER_TKIP || pairwise == CIPHER_TKIP_CCMP) {
        grade = worse(grade, WifiGradeWeak);
        if(reasons) furi_string_cat(reasons, "TKIP cipher: weak/deprecated\n");
    }
    if(!ssid || ssid[0] == '\0') {
        if(reasons) furi_string_cat(reasons, "Hidden SSID (info)\n");
    }

    return grade;
}
