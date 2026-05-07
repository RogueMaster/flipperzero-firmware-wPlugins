#include "../include/nfc_tools_ndef.h"
#include "../include/nfc_tools_social.h"
#include "../include/nfc_tools_search.h"

// ── NDEF parser ─────────────────────────────────────────────────────────────

static const char* const ndef_uri_prefixes[] = {
    [0x00] = "",
    [0x01] = "http://www.",
    [0x02] = "https://www.",
    [0x03] = "http://",
    [0x04] = "https://",
    [0x05] = "tel:",
    [0x06] = "mailto:",
    [0x07] = "ftp://anonymous:anonymous@",
    [0x08] = "ftp://ftp.",
    [0x09] = "ftps://",
    [0x0A] = "sftp://",
    [0x0B] = "smb://",
    [0x0C] = "nfs://",
    [0x0D] = "ftp://",
    [0x0E] = "dav://",
    [0x0F] = "news:",
    [0x10] = "telnet://",
    [0x11] = "imap:",
    [0x12] = "rtsp://",
    [0x13] = "urn:",
    [0x14] = "pop:",
    [0x15] = "sip:",
    [0x16] = "sips:",
    [0x17] = "tftp:",
    [0x18] = "btspp://",
    [0x19] = "btl2cap://",
    [0x1A] = "btgoep://",
    [0x1B] = "tcpobex://",
    [0x1C] = "irdaobex://",
    [0x1D] = "file://",
    [0x1E] = "urn:epc:id:",
    [0x1F] = "urn:epc:tag:",
    [0x20] = "urn:epc:pat:",
    [0x21] = "urn:epc:raw:",
    [0x22] = "urn:epc:",
    [0x23] = "urn:nfc:",
};
#define NDEF_URI_PREFIXES_COUNT (sizeof(ndef_uri_prefixes) / sizeof(ndef_uri_prefixes[0]))

// ─── Record type decoders ────────────────────────────────────────────────────

static void ndef_decode_uri(const uint8_t* payload, size_t len, FuriString* out) {
    if(len < 1) return;
    furi_string_cat_str(out, "[URL]\n");
    uint8_t pfx_id = payload[0];
    if(pfx_id < NDEF_URI_PREFIXES_COUNT) {
        furi_string_cat_str(out, ndef_uri_prefixes[pfx_id]);
    }
    for(size_t i = 1; i < len; i++) {
        if(payload[i] >= 0x20 && payload[i] < 0x7F) {
            furi_string_cat_printf(out, "%c", payload[i]);
        }
    }
}

static void ndef_decode_text(const uint8_t* payload, size_t len, FuriString* out) {
    if(len < 1) return;
    uint8_t status = payload[0];
    uint8_t lang_len = status & 0x3F;
    if((size_t)(1 + lang_len) >= len) return;

    furi_string_cat_str(out, "[Text]\n");
    const uint8_t* text = payload + 1 + lang_len;
    size_t text_len = len - 1 - lang_len;
    for(size_t i = 0; i < text_len; i++) {
        uint8_t c = text[i];
        if(c >= 0x20 && c < 0x7F) {
            furi_string_cat_printf(out, "%c", c);
        } else if(c == '\n' || c == '\r') {
            furi_string_cat_str(out, "\n");
        }
    }
}

// Recursive search for a WPS TLV attribute (supports the Credential container)
static bool
    wifi_find_attr(const uint8_t* data, size_t len, uint16_t target, const uint8_t** val, uint16_t* vlen) {
    size_t pos = 0;
    while(pos + 4 <= len) {
        uint16_t id = ((uint16_t)data[pos] << 8) | data[pos + 1];
        uint16_t alen = ((uint16_t)data[pos + 2] << 8) | data[pos + 3];
        pos += 4;
        if(pos + alen > len) break;
        if(id == target) {
            *val = data + pos;
            *vlen = alen;
            return true;
        }
        // Recurse into the Credential container (0x100E)
        if(id == 0x100E) {
            if(wifi_find_attr(data + pos, alen, target, val, vlen)) return true;
        }
        pos += alen;
    }
    return false;
}

static void ndef_decode_wifi(const uint8_t* payload, size_t len, FuriString* out) {
    furi_string_cat_str(out, "[WiFi]\n");

    const uint8_t* val;
    uint16_t vlen;

    // SSID (0x1045)
    if(wifi_find_attr(payload, len, 0x1045, &val, &vlen)) {
        furi_string_cat_str(out, "SSID: ");
        for(uint16_t i = 0; i < vlen; i++) {
            if(val[i] >= 0x20 && val[i] < 0x7F) {
                furi_string_cat_printf(out, "%c", val[i]);
            }
        }
        furi_string_cat_str(out, "\n");
    }

    // Auth type (0x1003)
    if(wifi_find_attr(payload, len, 0x1003, &val, &vlen) && vlen >= 2) {
        uint16_t auth = ((uint16_t)val[0] << 8) | val[1];
        const char* auth_s =
            (auth == 0x0020) ? "WPA2-Personal" :
            (auth == 0x0022) ? "WPA/WPA2" :
            (auth == 0x0002) ? "WPA-Personal" :
            (auth == 0x0001) ? "Open" :
                               "Other";
        furi_string_cat_printf(out, "Auth: %s\n", auth_s);
    }

    // Password (0x1027)
    if(wifi_find_attr(payload, len, 0x1027, &val, &vlen) && vlen > 0) {
        furi_string_cat_str(out, "PWD: ");
        for(uint16_t i = 0; i < vlen; i++) {
            if(val[i] >= 0x20 && val[i] < 0x7F) {
                furi_string_cat_printf(out, "%c", val[i]);
            }
        }
    }
}

static void ndef_decode_vcard(const uint8_t* payload, size_t len, FuriString* out) {
    furi_string_cat_str(out, "[vCard]\n");
    // Search for FN, TEL, EMAIL fields in the raw text
    const char* fields[] = {"FN:", "TEL:", "EMAIL:", "ORG:", NULL};
    const char* src = (const char*)payload;
    for(int f = 0; fields[f]; f++) {
        const char* p = src;
        size_t flen = strlen(fields[f]);
        // Scan line by line
        while((size_t)(p - src) < len) {
            if((size_t)(p - src) + flen <= len && memcmp(p, fields[f], flen) == 0) {
                const char* val_start = p + flen;
                const char* end = val_start;
                while((size_t)(end - src) < len && *end != '\r' && *end != '\n') end++;
                size_t val_len = (size_t)(end - val_start);
                if(val_len > 40) val_len = 40;
                furi_string_cat_str(out, fields[f]);
                for(size_t i = 0; i < val_len; i++) {
                    if(val_start[i] >= 0x20 && val_start[i] < 0x7F) {
                        furi_string_cat_printf(out, "%c", val_start[i]);
                    }
                }
                furi_string_cat_str(out, "\n");
                break;
            }
            // Advance to the next line
            while((size_t)(p - src) < len && *p != '\n') p++;
            if((size_t)(p - src) < len) p++;
        }
    }
}

// ─── Structured NDEF record population ───────────────────────────────────────

static void ndef_build_uri_value(const uint8_t* payload, size_t len, char* out, size_t out_sz) {
    if(len < 1) return;
    uint8_t pfx_id = payload[0];
    size_t written = 0;
    if(pfx_id < NDEF_URI_PREFIXES_COUNT) {
        written = strlcpy(out, ndef_uri_prefixes[pfx_id], out_sz);
    }
    for(size_t i = 1; i < len && written + 1 < out_sz; i++) {
        uint8_t c = payload[i];
        if(c >= 0x20 && c < 0x7F) {
            out[written++] = (char)c;
        }
    }
    if(written < out_sz) out[written] = '\0';
}

static void ndef_build_text_value(const uint8_t* payload, size_t len, char* out, size_t out_sz) {
    if(len < 1) return;
    uint8_t status  = payload[0];
    uint8_t lang_len = status & 0x3F;
    if((size_t)(1 + lang_len) >= len) return;
    const uint8_t* text = payload + 1 + lang_len;
    size_t text_len = len - 1 - lang_len;
    size_t written = 0;
    for(size_t i = 0; i < text_len && written + 1 < out_sz; i++) {
        uint8_t c = text[i];
        if(c >= 0x20 && c < 0x7F) {
            out[written++] = (char)c;
        } else if((c == '\n' || c == '\r') && written + 1 < out_sz) {
            out[written++] = '\n';
        }
    }
    if(written < out_sz) out[written] = '\0';
}

// __attribute__((noinline)) prevents the compiler from inlining this function
// into its callers. Without it, GCC -Os (used by ufbt) inlines all the way into
// nfc_tools_ndef_parse_type2_tag_structured, sees that rec = &app->ndef_records[i]
// and emits a spurious -Wrestrict warning on snprintf(rec->summary, ..., rec->value)
// calls (the two pointers are distinct fields of the same struct, never aliased).
static __attribute__((noinline)) void ndef_fill_record(
    NfcToolsNdefRecord* rec,
    uint8_t tnf,
    const uint8_t* type,
    size_t type_len,
    const uint8_t* payload,
    size_t payload_len)
{
    memset(rec, 0, sizeof(*rec));
    rec->tnf = tnf;

    // Type string
    size_t show = type_len < sizeof(rec->type_str) - 1 ? type_len : sizeof(rec->type_str) - 1;
    for(size_t i = 0; i < show; i++) {
        uint8_t c = type[i];
        rec->type_str[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    rec->type_str[show] = '\0';

    // Raw payload (truncated)
    rec->payload_len = (uint16_t)payload_len;
    size_t copy = payload_len < NFC_TOOLS_NDEF_PAYLOAD_MAX ? payload_len : NFC_TOOLS_NDEF_PAYLOAD_MAX;
    memcpy(rec->payload, payload, copy);

    // Decode according to type
    if(tnf == 0x01 && type_len == 1 && type[0] == 'U') {
        rec->type = NfcToolsNdefTypeUri;
        ndef_build_uri_value(payload, payload_len, rec->value, sizeof(rec->value));
        rec->has_qr = (rec->value[0] != '\0');
        snprintf(rec->summary, sizeof(rec->summary), "URL: %.34s", rec->value);

    } else if(tnf == 0x01 && type_len == 1 && type[0] == 'T') {
        rec->type = NfcToolsNdefTypeText;
        ndef_build_text_value(payload, payload_len, rec->value, sizeof(rec->value));
        rec->has_qr = (rec->value[0] != '\0');
        snprintf(rec->summary, sizeof(rec->summary), "Text: %.33s", rec->value);

    } else if(tnf == 0x02 && type_len == 23 &&
              memcmp(type, "application/vnd.wfa.wsc", 23) == 0) {
        rec->type = NfcToolsNdefTypeWifi;
        // Extract the SSID for the summary
        const uint8_t* val; uint16_t vlen;
        if(wifi_find_attr(payload, payload_len, 0x1045, &val, &vlen) && vlen > 0) {
            size_t w = vlen < sizeof(rec->value) - 1 ? vlen : sizeof(rec->value) - 1;
            memcpy(rec->value, val, w);
            rec->value[w] = '\0';
        }
        snprintf(rec->summary, sizeof(rec->summary), "WiFi: %.33s", rec->value);
        rec->has_qr = false;

    } else if(tnf == 0x02 && type_len >= 10 && memcmp(type, "text/vcard", 10) == 0) {
        rec->type = NfcToolsNdefTypeVcard;
        // Extract FN for the summary
        const char* fn = strstr((const char*)payload, "FN:");
        if(fn) {
            const char* end = fn + 3;
            size_t max = payload_len - (size_t)(fn - (const char*)payload) - 3;
            size_t w = 0;
            while(w < max && w + 1 < sizeof(rec->value) && end[w] != '\r' && end[w] != '\n') {
                rec->value[w] = end[w]; w++;
            }
            rec->value[w] = '\0';
        }
        snprintf(rec->summary, sizeof(rec->summary), "vCard: %.32s", rec->value);
        rec->has_qr = false;

    } else if(tnf == 0x01 && type_len == 2 && memcmp(type, "Sp", 2) == 0) {
        rec->type = NfcToolsNdefTypeSmartPoster;
        snprintf(rec->summary, sizeof(rec->summary), "SmartPoster (%u B)", (unsigned)payload_len);
        rec->has_qr = false;

    } else if(tnf == 0x00) {
        rec->type = NfcToolsNdefTypeEmpty;
        strlcpy(rec->summary, "Empty record", sizeof(rec->summary));
        rec->has_qr = false;

    } else if(tnf == 0x04) {
        rec->type = NfcToolsNdefTypeExternal;
        snprintf(rec->summary, sizeof(rec->summary), "Ext: %.33s", rec->type_str);
        rec->has_qr = false;

    } else {
        rec->type = NfcToolsNdefTypeMime;
        snprintf(rec->summary, sizeof(rec->summary), "MIME: %.32s", rec->type_str);
        rec->has_qr = false;
    }
}

// ─── Structured variant of the NDEF message parser ───────────────────────────

static void ndef_parse_message_structured(
    NfcToolsApp* app,
    const uint8_t* msg,
    size_t msg_len)
{
    size_t pos = 0;

    while(pos < msg_len && app->ndef_record_count < NFC_TOOLS_MAX_NDEF_RECORDS) {
        uint8_t flags_tnf = msg[pos++];
        uint8_t tnf = flags_tnf & 0x07;
        bool sr = (flags_tnf >> 4) & 1;
        bool il = (flags_tnf >> 3) & 1;
        bool me = (flags_tnf >> 6) & 1;

        if(pos >= msg_len) break;
        uint8_t type_len = msg[pos++];

        uint32_t payload_len;
        if(sr) {
            if(pos >= msg_len) break;
            payload_len = msg[pos++];
        } else {
            if(pos + 4 > msg_len) break;
            payload_len = ((uint32_t)msg[pos] << 24) | ((uint32_t)msg[pos+1] << 16) |
                          ((uint32_t)msg[pos+2] << 8) | msg[pos+3];
            pos += 4;
        }

        uint8_t id_len = 0;
        if(il) {
            if(pos >= msg_len) break;
            id_len = msg[pos++];
        }

        if(pos + type_len > msg_len) break;
        const uint8_t* type = msg + pos;
        pos += type_len;
        pos += id_len;

        if(pos + payload_len > msg_len) break;
        const uint8_t* payload = msg + pos;
        pos += payload_len;

        ndef_fill_record(
            &app->ndef_records[app->ndef_record_count],
            tnf, type, type_len, payload, (size_t)payload_len);
        app->ndef_record_count++;

        if(me) break;
    }
}

static void ndef_decode_record(
    uint8_t tnf,
    const uint8_t* type,
    size_t type_len,
    const uint8_t* payload,
    size_t payload_len,
    FuriString* out) {
    if(tnf == 0x01 && type_len == 1 && type[0] == 'U') {
        ndef_decode_uri(payload, payload_len, out);
    } else if(tnf == 0x01 && type_len == 1 && type[0] == 'T') {
        ndef_decode_text(payload, payload_len, out);
    } else if(tnf == 0x02 && type_len == 23 &&
              memcmp(type, "application/vnd.wfa.wsc", 23) == 0) {
        ndef_decode_wifi(payload, payload_len, out);
    } else if(tnf == 0x02 && type_len >= 10 && memcmp(type, "text/vcard", 10) == 0) {
        ndef_decode_vcard(payload, payload_len, out);
    } else {
        // Unknown type: show TNF + truncated type name
        furi_string_cat_str(out, "[NDEF TNF:");
        furi_string_cat_printf(out, "%u", tnf);
        if(type_len > 0) {
            furi_string_cat_str(out, " ");
            size_t show = type_len < 20 ? type_len : 20;
            for(size_t i = 0; i < show; i++) {
                if(type[i] >= 0x20 && type[i] < 0x7F) {
                    furi_string_cat_printf(out, "%c", type[i]);
                }
            }
        }
        furi_string_cat_printf(out, "]\n%u bytes", (unsigned)payload_len);
    }
}

// ─── Parse an NDEF message (sequence of records) ─────────────────────────────

static void ndef_parse_message(const uint8_t* msg, size_t msg_len, FuriString* out) {
    size_t pos = 0;
    bool first = true;

    while(pos < msg_len) {
        if(pos >= msg_len) break;
        uint8_t flags_tnf = msg[pos++];
        uint8_t tnf = flags_tnf & 0x07;
        bool sr = (flags_tnf >> 4) & 1;
        bool il = (flags_tnf >> 3) & 1;
        bool me = (flags_tnf >> 6) & 1;

        if(pos >= msg_len) break;
        uint8_t type_len = msg[pos++];

        uint32_t payload_len;
        if(sr) {
            if(pos >= msg_len) break;
            payload_len = msg[pos++];
        } else {
            if(pos + 4 > msg_len) break;
            payload_len = ((uint32_t)msg[pos] << 24) | ((uint32_t)msg[pos + 1] << 16) |
                          ((uint32_t)msg[pos + 2] << 8) | msg[pos + 3];
            pos += 4;
        }

        uint8_t id_len = 0;
        if(il) {
            if(pos >= msg_len) break;
            id_len = msg[pos++];
        }

        if(pos + type_len > msg_len) break;
        const uint8_t* type = msg + pos;
        pos += type_len;

        pos += id_len; // ID ignored

        if(pos + payload_len > msg_len) break;
        const uint8_t* payload = msg + pos;
        pos += payload_len;

        if(!first) furi_string_cat_str(out, "\n");
        first = false;

        ndef_decode_record(tnf, type, type_len, payload, payload_len, out);

        if(me) break;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void nfc_tools_ndef_parse_type2_tag(const uint8_t* data, size_t data_len, FuriString* out) {
    size_t pos = 0;
    while(pos < data_len) {
        uint8_t tlv = data[pos++];
        if(tlv == 0xFE) break;   // Terminator
        if(tlv == 0x00) continue; // Padding

        // TLV length
        if(pos >= data_len) break;
        size_t tlen;
        if(data[pos] == 0xFF) {
            pos++;
            if(pos + 2 > data_len) break;
            tlen = ((size_t)data[pos] << 8) | data[pos + 1];
            pos += 2;
        } else {
            tlen = data[pos++];
        }

        if(pos + tlen > data_len) break;

        if(tlv == 0x03) { // NDEF Message
            ndef_parse_message(data + pos, tlen, out);
        }
        // Other TLVs (Lock Control 0x01, Mem Control 0x02, Prop 0xFD) are ignored

        pos += tlen;
    }
}

void nfc_tools_ndef_parse_type2_tag_structured(
    NfcToolsApp* app,
    const uint8_t* data,
    size_t data_len)
{
    size_t pos = 0;
    while(pos < data_len) {
        uint8_t tlv = data[pos++];
        if(tlv == 0xFE) break;
        if(tlv == 0x00) continue;

        if(pos >= data_len) break;
        size_t tlen;
        if(data[pos] == 0xFF) {
            pos++;
            if(pos + 2 > data_len) break;
            tlen = ((size_t)data[pos] << 8) | data[pos+1];
            pos += 2;
        } else {
            tlen = data[pos++];
        }

        if(pos + tlen > data_len) break;
        if(tlv == 0x03) {
            ndef_parse_message_structured(app, data + pos, tlen);
        }
        pos += tlen;
    }
}

// ── URL encoder (RFC 3986) ─────────────────────────────────────────────────

static void url_encode(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 4 < dest_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = (char)c;
        } else {
            dest[j++] = '%';
            dest[j++] = hex[(c >> 4) & 0x0F];
            dest[j++] = hex[c & 0x0F];
        }
    }
    dest[j] = '\0';
}

uint8_t* nfc_tools_ndef_build(NfcToolsApp* app, size_t* out_size) {
    uint8_t tnf = 0x00;
    const char* type = "";
    uint8_t* payload = NULL;
    uint8_t* pay_it = NULL;
    size_t payload_len = 0;

    switch(app->ndef_type) {

    case NdefTypeEmpty:
        tnf = 0x00;
        type = "";
        payload_len = 0;
        payload = NULL;
        break;

    case NdefTypeUnitLink: {
        tnf = 0x01;
        type = "U";
        const char* base = "unit.link/";
        size_t base_len = strlen(base);
        size_t alias_len = strlen(app->ndef_buf1);
        payload_len = 1 + base_len + alias_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x04; // prefix "https://"
        memcpy(pay_it, base, base_len);
        pay_it += base_len;
        memcpy(pay_it, app->ndef_buf1, alias_len);
        break;
    }

    case NdefTypeUrl:
    case NdefTypeCustomUri: {
        tnf = 0x01;
        type = "U";
        size_t url_len = strlen(app->ndef_buf1);
        payload_len = url_len + 1;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00; // No automatic prefix
        memcpy(pay_it, app->ndef_buf1, url_len);
        break;
    }

    case NdefTypeText: {
        tnf = 0x01;
        type = "T";
        size_t text_len = strlen(app->ndef_buf1);
        payload_len = text_len + 3; // 1 (status) + 2 (language "en")
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x02; // Language code length = 2
        *pay_it++ = 'e';
        *pay_it++ = 'n';
        memcpy(pay_it, app->ndef_buf1, text_len);
        break;
    }

    case NdefTypeWifi: {
        tnf = 0x02;
        type = "application/vnd.wfa.wsc";
        uint8_t ssid_len = (uint8_t)strlen(app->ndef_buf1);
        uint8_t pass_len = (uint8_t)strlen(app->ndef_buf2);
        payload_len = ssid_len + pass_len + 39;
        payload = pay_it = malloc(payload_len);

        *pay_it++ = 0x10; *pay_it++ = 0x0E;
        *pay_it++ = 0x00; *pay_it++ = (uint8_t)(ssid_len + pass_len + 35);

        *pay_it++ = 0x10; *pay_it++ = 0x26;
        *pay_it++ = 0x00; *pay_it++ = 0x01;
        *pay_it++ = 0x01;

        *pay_it++ = 0x10; *pay_it++ = 0x45;
        *pay_it++ = (ssid_len >> 8) & 0xFF;
        *pay_it++ = ssid_len & 0xFF;
        memcpy(pay_it, app->ndef_buf1, ssid_len);
        pay_it += ssid_len;

        *pay_it++ = 0x10; *pay_it++ = 0x03;
        *pay_it++ = 0x00; *pay_it++ = 0x02;
        *pay_it++ = 0x00;
        *pay_it++ = (pass_len > 0) ? 0x20 : 0x01;

        *pay_it++ = 0x10; *pay_it++ = 0x0F;
        *pay_it++ = 0x00; *pay_it++ = 0x02;
        *pay_it++ = 0x00;
        *pay_it++ = (pass_len > 0) ? 0x08 : 0x01;

        *pay_it++ = 0x10; *pay_it++ = 0x27;
        *pay_it++ = (pass_len >> 8) & 0xFF;
        *pay_it++ = pass_len & 0xFF;
        memcpy(pay_it, app->ndef_buf2, pass_len);
        pay_it += pass_len;

        *pay_it++ = 0x10; *pay_it++ = 0x20;
        *pay_it++ = 0x00; *pay_it++ = 0x06;
        *pay_it++ = 0xFF; *pay_it++ = 0xFF;
        *pay_it++ = 0xFF; *pay_it++ = 0xFF;
        *pay_it++ = 0xFF; *pay_it++ = 0xFF;
        break;
    }

    case NdefTypeBitcoin: {
        tnf  = 0x01;
        type = "U";

        const char* btc_scheme = "bitcoin:";
        size_t scheme_len  = strlen(btc_scheme);
        size_t addr_len    = strlen(app->ndef_buf1);
        bool   has_amount  = (app->ndef_buf2[0] != '\0');
        bool   has_message = (app->ndef_buf3[0] != '\0');

        char* enc_msg = NULL;
        size_t enc_msg_len = 0;
        if(has_message) {
            size_t raw_len = strlen(app->ndef_buf3);
            enc_msg = malloc(raw_len * 3 + 1);
            url_encode(app->ndef_buf3, enc_msg, raw_len * 3 + 1);
            enc_msg_len = strlen(enc_msg);
        }

        size_t amount_len = has_amount  ? strlen(app->ndef_buf2) : 0;
        payload_len = 1 + scheme_len + addr_len;
        if(has_amount)  payload_len += 8  + amount_len;
        if(has_message) payload_len += 9 + enc_msg_len;

        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, btc_scheme,     scheme_len);  pay_it += scheme_len;
        memcpy(pay_it, app->ndef_buf1, addr_len);    pay_it += addr_len;

        if(has_amount || has_message) {
            if(has_amount) {
                memcpy(pay_it, "?amount=", 8); pay_it += 8;
                memcpy(pay_it, app->ndef_buf2, amount_len); pay_it += amount_len;
            }
            if(has_message) {
                if(has_amount) {
                    memcpy(pay_it, "&message=", 9); pay_it += 9;
                } else {
                    memcpy(pay_it, "?message=", 9); pay_it += 9;
                }
                memcpy(pay_it, enc_msg, enc_msg_len); pay_it += enc_msg_len;
            }
        }
        if(enc_msg) free(enc_msg);
        break;
    }

    case NdefTypeSearch: {
        tnf  = 0x01;
        type = "U";
        const NfcToolsSearchEngine* eng =
            &nfc_tools_search_engines[app->search_engine_index];

        size_t kw_raw_len = strlen(app->ndef_buf1);
        char*  encoded    = malloc(kw_raw_len * 3 + 1);
        url_encode(app->ndef_buf1, encoded, kw_raw_len * 3 + 1);

        size_t prefix_len  = strlen(eng->url_prefix);
        size_t kw_enc_len  = strlen(encoded);
        size_t suffix_len  = strlen(eng->url_suffix);
        payload_len = 1 + prefix_len + kw_enc_len + suffix_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, eng->url_prefix, prefix_len); pay_it += prefix_len;
        memcpy(pay_it, encoded,         kw_enc_len); pay_it += kw_enc_len;
        memcpy(pay_it, eng->url_suffix, suffix_len); pay_it += suffix_len;
        free(encoded);
        break;
    }

    case NdefTypeContact: {
        tnf  = 0x02;
        type = "text/vcard";

        FuriString* vc = furi_string_alloc();
        furi_string_cat_str(vc, "BEGIN:VCARD\r\nVERSION:3.0\r\n");
        furi_string_cat_printf(vc, "FN:%s\r\n",  app->ndef_buf1);
        furi_string_cat_printf(vc, "N:;%s;;;\r\n", app->ndef_buf1);
        if(app->ndef_buf2[0])
            furi_string_cat_printf(vc, "ORG:%s\r\n", app->ndef_buf2);
        if(app->ndef_buf3[0])
            furi_string_cat_printf(vc, "ADR:;;%s;;;;\r\n", app->ndef_buf3);
        if(app->ndef_buf4[0] && !(app->ndef_buf4[0] == '+' && app->ndef_buf4[1] == '\0'))
            furi_string_cat_printf(vc, "TEL:%s\r\n", app->ndef_buf4);
        if(app->ndef_buf5[0])
            furi_string_cat_printf(vc, "EMAIL:%s\r\n", app->ndef_buf5);
        if(app->ndef_buf6[0] &&
           strcmp(app->ndef_buf6, "https://") != 0)
            furi_string_cat_printf(vc, "URL:%s\r\n", app->ndef_buf6);
        furi_string_cat_str(vc, "END:VCARD\r\n");

        payload_len = furi_string_size(vc);
        payload     = malloc(payload_len);
        memcpy(payload, furi_string_get_cstr(vc), payload_len);
        furi_string_free(vc);
        break;
    }

    case NdefTypeLocation: {
        tnf  = 0x01;
        type = "U";
        const char* geo    = "geo:";
        size_t geo_len     = strlen(geo);
        size_t lat_len     = strlen(app->ndef_buf1);
        size_t lon_len     = strlen(app->ndef_buf2);
        payload_len = 1 + geo_len + lat_len + 1 + lon_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, geo,             geo_len); pay_it += geo_len;
        memcpy(pay_it, app->ndef_buf1,  lat_len); pay_it += lat_len;
        *pay_it++ = ',';
        memcpy(pay_it, app->ndef_buf2,  lon_len); pay_it += lon_len;
        break;
    }

    case NdefTypeSocial: {
        tnf  = 0x01;
        type = "U";
        const NfcToolsSocialNetwork* net =
            &nfc_tools_social_networks[app->social_network_index];
        size_t prefix_len = strlen(net->url_prefix);
        size_t user_len   = strlen(app->ndef_buf1);
        size_t suffix_len = strlen(net->url_suffix);
        payload_len = 1 + prefix_len + user_len + suffix_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, net->url_prefix, prefix_len); pay_it += prefix_len;
        memcpy(pay_it, app->ndef_buf1,  user_len);   pay_it += user_len;
        memcpy(pay_it, net->url_suffix, suffix_len); pay_it += suffix_len;
        break;
    }

    case NdefTypeCustomData: {
        tnf  = 0x02;
        type = app->ndef_buf1;
        size_t data_len = strlen(app->ndef_buf3);
        payload_len = data_len;
        if(data_len > 0) {
            payload = pay_it = malloc(data_len);
            memcpy(pay_it, app->ndef_buf3, data_len);
        }
        break;
    }

    case NdefTypeBluetooth: {
        tnf  = 0x02;
        type = "application/vnd.bluetooth.ep.oob";

        uint8_t mac[6]      = {0};
        uint8_t byte_idx    = 0;
        uint8_t nibble_count = 0;
        uint8_t current_byte = 0;
        for(const char* s = app->ndef_buf1; *s && byte_idx < 6; s++) {
            char c = *s;
            uint8_t nibble;
            if     (c >= '0' && c <= '9') nibble = (uint8_t)(c - '0');
            else if(c >= 'a' && c <= 'f') nibble = (uint8_t)(c - 'a' + 10);
            else if(c >= 'A' && c <= 'F') nibble = (uint8_t)(c - 'A' + 10);
            else continue;
            current_byte = (uint8_t)((current_byte << 4) | nibble);
            if(++nibble_count == 2) {
                mac[byte_idx++] = current_byte;
                current_byte    = 0;
                nibble_count    = 0;
            }
        }

        payload_len = 8;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x08;
        *pay_it++ = 0x00;
        for(int i = 5; i >= 0; i--) *pay_it++ = mac[i];
        break;
    }

    case NdefTypeSms: {
        tnf = 0x01;
        type = "U";
        const char* sms_prefix = "sms:";
        size_t prefix_len  = strlen(sms_prefix);
        size_t num_len     = strlen(app->ndef_buf1);
        size_t msg_len     = strlen(app->ndef_buf2);
        bool   has_body    = (msg_len > 0);

        size_t uri_len = prefix_len + num_len;
        if(has_body) uri_len += 6 + msg_len;

        payload_len = 1 + uri_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, sms_prefix, prefix_len); pay_it += prefix_len;
        memcpy(pay_it, app->ndef_buf1, num_len); pay_it += num_len;
        if(has_body) {
            memcpy(pay_it, "?body=", 6); pay_it += 6;
            memcpy(pay_it, app->ndef_buf2, msg_len); pay_it += msg_len;
        }
        break;
    }

    case NdefTypePhone: {
        tnf = 0x01;
        type = "U";
        size_t num_len = strlen(app->ndef_buf1);
        payload_len = 1 + num_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x05; // prefix "tel:"
        memcpy(pay_it, app->ndef_buf1, num_len);
        break;
    }

    case NdefTypeFacetime: {
        tnf = 0x01;
        type = "U";
        const char* ft_prefix = "facetime:";
        size_t prefix_len = strlen(ft_prefix);
        size_t val_len    = strlen(app->ndef_buf1);
        payload_len = 1 + prefix_len + val_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, ft_prefix, prefix_len); pay_it += prefix_len;
        memcpy(pay_it, app->ndef_buf1, val_len);
        break;
    }

    case NdefTypeFacetimeAudio: {
        tnf = 0x01;
        type = "U";
        const char* fta_prefix = "facetime-audio:";
        size_t prefix_len = strlen(fta_prefix);
        size_t val_len    = strlen(app->ndef_buf1);
        payload_len = 1 + prefix_len + val_len;
        payload = pay_it = malloc(payload_len);
        *pay_it++ = 0x00;
        memcpy(pay_it, fta_prefix, prefix_len); pay_it += prefix_len;
        memcpy(pay_it, app->ndef_buf1, val_len);
        break;
    }

    case NdefTypeMail: {
        tnf = 0x01;
        type = "U";
        size_t addr_len    = strlen(app->ndef_buf1);
        size_t subject_len = strlen(app->ndef_buf2);
        size_t body_len    = strlen(app->ndef_buf3);
        bool has_subject = (subject_len > 0);
        bool has_body    = (body_len > 0);

        size_t suffix_len = addr_len;
        if(has_subject || has_body) {
            suffix_len += 1;
            if(has_subject) suffix_len += 8 + subject_len;
            if(has_subject && has_body) suffix_len += 1;
            if(has_body)    suffix_len += 5 + body_len;
        }
        payload_len = 1 + suffix_len;
        payload = pay_it = malloc(payload_len);

        *pay_it++ = 0x06; // prefix "mailto:"
        memcpy(pay_it, app->ndef_buf1, addr_len);
        pay_it += addr_len;
        if(has_subject || has_body) {
            *pay_it++ = '?';
            if(has_subject) {
                memcpy(pay_it, "subject=", 8); pay_it += 8;
                memcpy(pay_it, app->ndef_buf2, subject_len); pay_it += subject_len;
            }
            if(has_subject && has_body) *pay_it++ = '&';
            if(has_body) {
                memcpy(pay_it, "body=", 5); pay_it += 5;
                memcpy(pay_it, app->ndef_buf3, body_len); pay_it += body_len;
            }
        }
        break;
    }
    }

    // ── Record header assembly ─────────────────────────────────────────────
    uint8_t flags = 0;
    flags |= (1 << 7); // MB (Message Begin)
    flags |= (1 << 6); // ME (Message End)
    flags |= tnf;
    size_t type_len = strlen(type);

    bool short_record = (payload_len < 0xFF);
    if(short_record) flags |= (1 << 4); // SR

    size_t header_len = 1        // Flags+TNF
                        + 1      // Type length
                        + (short_record ? 1 : 4) // Payload length
                        + type_len;
    size_t record_len = header_len + payload_len;

    // ── Buffer TLV final ───────────────────────────────────────────────────
    size_t tlv_len_size = (record_len < 0xFF) ? 1 : 3;
    size_t total = 1 + tlv_len_size + record_len + 1; // 0x03 + len + record + 0xFE

    uint8_t* buf = malloc(total);
    uint8_t* b = buf;

    *b++ = 0x03; // TLV NDEF Message
    if(record_len < 0xFF) {
        *b++ = (uint8_t)record_len;
    } else {
        *b++ = 0xFF;
        *b++ = (record_len >> 8) & 0xFF;
        *b++ = record_len & 0xFF;
    }

    *b++ = flags;
    *b++ = (uint8_t)type_len;
    if(short_record) {
        *b++ = (uint8_t)payload_len;
    } else {
        *b++ = (payload_len >> 24) & 0xFF;
        *b++ = (payload_len >> 16) & 0xFF;
        *b++ = (payload_len >> 8) & 0xFF;
        *b++ = payload_len & 0xFF;
    }
    memcpy(b, type, type_len);
    b += type_len;
    if(payload_len > 0 && payload != NULL) {
        memcpy(b, payload, payload_len);
    }
    b += payload_len;
    if(payload) free(payload);

    *b++ = 0xFE; // TLV Terminator

    furi_check((size_t)(b - buf) == total);
    *out_size = total;
    return buf;
}

const char* nfc_tools_ndef_write_label(NdefType type) {
    switch(type) {
    case NdefTypeUrl:        return NTS_WRITE_TITLE_URL;
    case NdefTypeCustomUri:  return NTS_WRITE_TITLE_URI;
    case NdefTypeText:       return NTS_WRITE_TITLE_TEXT;
    case NdefTypeWifi:       return NTS_WRITE_TITLE_WIFI;
    case NdefTypeUnitLink:   return NTS_WRITE_TITLE_UNIT_LINK;
    case NdefTypeMail:       return NTS_WRITE_TITLE_MAIL;
    case NdefTypePhone:      return NTS_WRITE_TITLE_PHONE;
    case NdefTypeSms:             return NTS_WRITE_TITLE_SMS;
    case NdefTypeFacetime:        return NTS_WRITE_TITLE_FACETIME;
    case NdefTypeFacetimeAudio:   return NTS_WRITE_TITLE_FACETIME_AUDIO;
    case NdefTypeBluetooth:       return NTS_WRITE_TITLE_BLUETOOTH;
    case NdefTypeCustomData: return NTS_WRITE_TITLE_CUSTOM_DATA;
    case NdefTypeSocial:     return NTS_WRITE_TITLE_SOCIAL;
    case NdefTypeLocation:   return NTS_WRITE_TITLE_LOCATION;
    case NdefTypeContact:    return NTS_WRITE_TITLE_CONTACT;
    case NdefTypeSearch:     return NTS_WRITE_TITLE_SEARCH;
    case NdefTypeBitcoin:    return NTS_WRITE_TITLE_BITCOIN;
    case NdefTypeEmpty:      return NTS_WRITE_TITLE_ERASE;
    default:                 return NTS_WRITE_TITLE_NDEF;
    }
}
