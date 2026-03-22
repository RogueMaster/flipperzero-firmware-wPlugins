#include "wfr_nfc.h"
#include <string.h>

/* WiFi Simple Configuration attribute IDs */
#define WSC_ATTR_CREDENTIAL  0x100E
#define WSC_ATTR_SSID        0x1045
#define WSC_ATTR_AUTH_TYPE   0x1003
#define WSC_ATTR_NET_KEY     0x1027

/* WSC Authentication Type values */
#define WSC_AUTH_OPEN        0x0001
#define WSC_AUTH_WPA_PSK     0x0002
#define WSC_AUTH_SHARED      0x0004 /* WEP */
#define WSC_AUTH_WPA2_PSK    0x0020

/* NDEF TLV types */
#define NDEF_TLV_NULL        0x00
#define NDEF_TLV_NDEF_MSG    0x03
#define NDEF_TLV_TERMINATOR  0xFE

/* NDEF record TNF values */
#define NDEF_TNF_MEDIA_TYPE  0x02

/* NDEF Capability Container magic byte */
#define NDEF_CC_MAGIC        0xE1

static const char wifi_ndef_type[] = "application/vnd.wfa.wsc";
#define WIFI_NDEF_TYPE_LEN 23

static bool parse_wsc_attributes(
    const uint8_t* data,
    size_t len,
    WfrWifiCreds* creds,
    bool* found_ssid) {
    size_t pos = 0;

    while(pos + 4 <= len) {
        uint16_t attr_id = ((uint16_t)data[pos] << 8) | data[pos + 1];
        uint16_t attr_len = ((uint16_t)data[pos + 2] << 8) | data[pos + 3];
        pos += 4;

        if(pos + attr_len > len) break;

        switch(attr_id) {
        case WSC_ATTR_CREDENTIAL:
            /* Credential is a container — recurse into it */
            parse_wsc_attributes(data + pos, attr_len, creds, found_ssid);
            break;

        case WSC_ATTR_SSID:
            if(attr_len > 0 && attr_len <= WFR_SSID_MAX_LEN) {
                memcpy(creds->ssid, data + pos, attr_len);
                creds->ssid[attr_len] = '\0';
                *found_ssid = true;
            }
            break;

        case WSC_ATTR_AUTH_TYPE:
            if(attr_len == 2) {
                uint16_t auth = ((uint16_t)data[pos] << 8) | data[pos + 1];
                if(auth == WSC_AUTH_OPEN)
                    creds->security = WFR_SEC_OPEN;
                else if(auth == WSC_AUTH_SHARED)
                    creds->security = WFR_SEC_WEP;
                else
                    creds->security = WFR_SEC_WPA;
            }
            break;

        case WSC_ATTR_NET_KEY:
            if(attr_len > 0 && attr_len <= WFR_PASS_MAX_LEN) {
                memcpy(creds->password, data + pos, attr_len);
                creds->password[attr_len] = '\0';
            }
            break;
        }

        pos += attr_len;
    }

    return *found_ssid;
}

static bool parse_ndef_record(const uint8_t* data, size_t len, WfrWifiCreds* creds) {
    if(len < 3) return false;

    uint8_t flags = data[0];
    uint8_t tnf = flags & 0x07;
    bool sr = (flags >> 4) & 1;
    bool il = (flags >> 3) & 1;

    /* We want Media-type TNF for WiFi WSC */
    if(tnf != NDEF_TNF_MEDIA_TYPE) return false;

    uint8_t type_len = data[1];
    size_t pos = 2;

    /* Payload length */
    size_t payload_len;
    if(sr) {
        if(pos >= len) return false;
        payload_len = data[pos++];
    } else {
        if(pos + 4 > len) return false;
        payload_len = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos + 1] << 16) |
                      ((uint32_t)data[pos + 2] << 8) | data[pos + 3];
        pos += 4;
    }

    /* ID length (skip if present) */
    size_t id_len = 0;
    if(il) {
        if(pos >= len) return false;
        id_len = data[pos++];
    }

    /* Check type field */
    if(pos + type_len > len) return false;
    if(type_len != WIFI_NDEF_TYPE_LEN ||
       memcmp(&data[pos], wifi_ndef_type, type_len) != 0) {
        return false;
    }
    pos += type_len;

    /* Skip ID */
    pos += id_len;

    /* Payload = WSC attributes */
    if(pos + payload_len > len) return false;

    memset(creds, 0, sizeof(WfrWifiCreds));
    bool found_ssid = false;
    return parse_wsc_attributes(&data[pos], payload_len, creds, &found_ssid);
}

static bool parse_ndef_message(const uint8_t* data, size_t len, WfrWifiCreds* creds) {
    size_t pos = 0;

    /* Iterate NDEF records within the message */
    while(pos < len) {
        if(pos + 3 > len) break;

        uint8_t flags = data[pos];
        bool sr = (flags >> 4) & 1;
        bool il = (flags >> 3) & 1;
        bool me = (flags >> 6) & 1;

        uint8_t type_len = data[pos + 1];
        size_t hdr_size = 2;

        /* Payload length field size */
        size_t payload_len;
        if(sr) {
            if(pos + 3 > len) break;
            payload_len = data[pos + 2];
            hdr_size += 1;
        } else {
            if(pos + 6 > len) break;
            payload_len = ((uint32_t)data[pos + 2] << 24) |
                          ((uint32_t)data[pos + 3] << 16) |
                          ((uint32_t)data[pos + 4] << 8) | data[pos + 5];
            hdr_size += 4;
        }

        /* ID length */
        size_t id_len = 0;
        if(il) {
            if(pos + hdr_size >= len) break;
            id_len = data[pos + hdr_size];
            hdr_size += 1;
        }

        size_t record_total = hdr_size + type_len + id_len + payload_len;
        if(pos + record_total > len) break;

        /* Try parsing this record as WiFi */
        if(parse_ndef_record(&data[pos], record_total, creds)) {
            return true;
        }

        pos += record_total;

        if(me) break; /* Message End flag */
    }

    return false;
}

bool wfr_nfc_parse_wifi_tag(
    const uint8_t* tag_pages,
    uint16_t pages_read,
    WfrWifiCreds* out_creds) {
    if(pages_read < 5 || !tag_pages || !out_creds) return false;

    /* Page 3 = Capability Container. Check NDEF magic byte. */
    const uint8_t* cc = &tag_pages[3 * 4];
    if(cc[0] != NDEF_CC_MAGIC) return false;

    /* NDEF data starts at page 4 */
    const uint8_t* ndef_area = &tag_pages[4 * 4];
    size_t ndef_area_len = (size_t)(pages_read - 4) * 4;

    /* Parse TLV structure to find NDEF Message */
    size_t pos = 0;
    while(pos < ndef_area_len) {
        uint8_t tlv_type = ndef_area[pos++];

        if(tlv_type == NDEF_TLV_NULL) continue;
        if(tlv_type == NDEF_TLV_TERMINATOR) break;

        /* Read TLV length */
        if(pos >= ndef_area_len) break;
        size_t tlv_len;
        if(ndef_area[pos] == 0xFF) {
            if(pos + 3 > ndef_area_len) break;
            tlv_len = ((size_t)ndef_area[pos + 1] << 8) | ndef_area[pos + 2];
            pos += 3;
        } else {
            tlv_len = ndef_area[pos++];
        }

        if(pos + tlv_len > ndef_area_len) break;

        if(tlv_type == NDEF_TLV_NDEF_MSG) {
            if(parse_ndef_message(&ndef_area[pos], tlv_len, out_creds)) {
                return true;
            }
        }

        pos += tlv_len;
    }

    return false;
}
