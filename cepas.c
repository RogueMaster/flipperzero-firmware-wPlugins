#include "cepas.h"

#include <stdio.h>
#include <string.h>

static int32_t read_s24(const uint8_t* p) {
    int32_t v = ((int32_t)p[0] << 16) | ((int32_t)p[1] << 8) | (int32_t)p[2];
    if(v & 0x800000) v -= 0x1000000;
    return v;
}

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

void cepas_parse_txn(const uint8_t* data, CepasTxn* out) {
    out->type = data[0];
    out->amount = read_s24(&data[1]);
    out->time = read_u32(&data[4]);
    for(size_t i = 0; i < 8; i++) {
        uint8_t c = data[8 + i];
        out->user_data[i] = (c >= 0x20 && c < 0x7F) ? (char)c : ' ';
    }
    out->user_data[8] = '\0';
    /* Trim trailing spaces. */
    for(int i = 7; i >= 0 && out->user_data[i] == ' '; i--)
        out->user_data[i] = '\0';
}

bool cepas_parse_purse(const uint8_t* data, size_t len, CepasPurse* out) {
    memset(out, 0, sizeof(*out));
    if(len < CEPAS_PURSE_MIN_LEN) return false;

    out->version = data[0];
    out->status = data[1];
    out->balance = read_s24(&data[2]);
    out->autoload = read_s24(&data[5]);
    memcpy(out->can, &data[8], 8);
    memcpy(out->csn, &data[16], 8);
    out->expiry_days = read_u16(&data[24]);
    out->creation_days = read_u16(&data[26]);
    out->log_count = data[40];

    if(len >= 46 + CEPAS_TXN_SIZE) {
        cepas_parse_txn(&data[46], &out->last_txn);
        out->has_last_txn = true;
    }
    return true;
}

size_t cepas_parse_history(const uint8_t* data, size_t len, CepasTxn* out, size_t max) {
    size_t n = 0;
    for(size_t off = 0; off + CEPAS_TXN_SIZE <= len && n < max; off += CEPAS_TXN_SIZE) {
        cepas_parse_txn(&data[off], &out[n++]);
    }
    return n;
}

CepasIssuer cepas_issuer(const CepasPurse* purse) {
    /* CAN prefixes: EZ-Link 100x / 800x, NETS 111x. */
    uint8_t d0 = purse->can[0];
    uint8_t d1 = purse->can[1] >> 4;
    if((d0 == 0x10 || d0 == 0x80) && d1 == 0x0) return CepasIssuerEzLink;
    if(d0 == 0x11 && d1 == 0x1) return CepasIssuerNets;
    return CepasIssuerUnknown;
}

const char* cepas_issuer_name(CepasIssuer issuer) {
    switch(issuer) {
    case CepasIssuerEzLink:
        return "EZ-Link";
    case CepasIssuerNets:
        return "NETS FlashPay";
    default:
        return "CEPAS card";
    }
}

const char* cepas_txn_type_name(uint8_t type) {
    switch(type) {
    case 48:
        return "MRT";
    case 49:
        return "Bus";
    case 117:
    case 3:
        return "Top-up";
    case 118:
        return "Bus refund";
    case 0xF0:
    case 5:
        return "Card created";
    case 4:
        return "Service";
    case 1:
        return "Retail";
    default:
        return NULL;
    }
}

void cepas_format_can(const uint8_t can[8], char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X%02X %02X%02X %02X%02X %02X%02X",
        can[0],
        can[1],
        can[2],
        can[3],
        can[4],
        can[5],
        can[6],
        can[7]);
}

void cepas_format_amount(int32_t cents, bool show_plus, char* out, size_t out_size) {
    const char* sign = "";
    uint32_t mag;
    if(cents < 0) {
        sign = "-";
        mag = (uint32_t)(-(int64_t)cents);
    } else {
        if(show_plus && cents > 0) sign = "+";
        mag = (uint32_t)cents;
    }
    snprintf(
        out,
        out_size,
        "%s$%lu.%02lu",
        sign,
        (unsigned long)(mag / 100),
        (unsigned long)(mag % 100));
}

uint32_t cepas_time_to_wallclock(uint32_t seconds) {
    return (uint32_t)(CEPAS_EPOCH_WALLCLOCK + seconds);
}

uint32_t cepas_days_to_wallclock(uint16_t days) {
    return (uint32_t)(CEPAS_EPOCH_WALLCLOCK + (uint32_t)days * 86400UL);
}

/* Copies a station name for code, or the code itself when unknown. */
static void station_or_code(const char* code, char* out, size_t n) {
    const char* name = cepas_station_name(code);
    snprintf(out, n, "%s", name ? name : code);
}

/* LRT codes look like SE1, SW5, PE3, PW7, BP6. */
static bool is_lrt(const char* code) {
    bool digit = code[2] >= '0' && code[2] <= '9';
    if(!digit) return false;
    if((code[0] == 'S' || code[0] == 'P') && (code[1] == 'E' || code[1] == 'W')) return true;
    return code[0] == 'B' && code[1] == 'P';
}

/* Bus records carry "BUS118", "BUS 118" or "SVC 43": returns the route. */
static bool bus_route(const char* user_data, char* out, size_t n) {
    if(strncmp(user_data, "BUS", 3) != 0 && strncmp(user_data, "SVC", 3) != 0) return false;
    size_t j = 0;
    for(const char* p = user_data + 3; *p && j + 1 < n; p++) {
        if(*p != ' ') out[j++] = *p;
    }
    out[j] = '\0';
    return j > 0;
}

void cepas_txn_describe(const CepasTxn* txn, CepasTxnText* out) {
    memset(out, 0, sizeof(*out));
    const char* ud = txn->user_data;
    size_t ud_len = strlen(ud);
    char a[8] = {0}, b[8] = {0};
    bool two_codes = ud_len >= 7 && (ud[3] == '-' || ud[3] == ' ');
    if(two_codes) {
        memcpy(a, ud, 3);
        size_t blen = ud_len - 4;
        if(blen > 3) blen = 3;
        memcpy(b, ud + 4, blen);
    }

    const char* type_name = cepas_txn_type_name(txn->type);
    char route[12];

    switch(txn->type) {
    case 48: /* MRT / LRT */
        snprintf(out->kind, sizeof(out->kind), "MRT");
        if(two_codes) {
            station_or_code(a, out->from, sizeof(out->from));
            station_or_code(b, out->to, sizeof(out->to));
            if(is_lrt(a) && is_lrt(b)) snprintf(out->kind, sizeof(out->kind), "LRT");
            snprintf(out->detail, sizeof(out->detail), "%s > %s", out->from, out->to);
        } else {
            station_or_code(ud, out->detail, sizeof(out->detail));
        }
        return;
    case 49: /* Bus */
    case 118: /* Bus refund */
        snprintf(out->kind, sizeof(out->kind), "%s", type_name);
        if(bus_route(ud, route, sizeof(route))) {
            snprintf(out->detail, sizeof(out->detail), "Service %s", route);
        } else {
            snprintf(out->detail, sizeof(out->detail), "%s", ud);
        }
        return;
    case 117:
    case 3: /* Top-up: "PGL GTM" = Punggol ticket machine */
        snprintf(out->kind, sizeof(out->kind), "Top-up");
        if(two_codes) {
            char st[24];
            station_or_code(a, st, sizeof(st));
            if(strcmp(b, "GTM") == 0) {
                snprintf(out->detail, sizeof(out->detail), "%s (machine)", st);
            } else if(strcmp(b, "PSC") == 0) {
                snprintf(out->detail, sizeof(out->detail), "%s (counter)", st);
            } else {
                snprintf(out->detail, sizeof(out->detail), "%s %s", st, b);
            }
        } else if(ud_len) {
            station_or_code(ud, out->detail, sizeof(out->detail));
        }
        return;
    default:
        break;
    }

    if(strcmp(ud, "TL") == 0) {
        /* Records made at a TransitLink ticket office (e.g. refunds and
         * balance transfers). The type codes themselves are undocumented. */
        snprintf(out->kind, sizeof(out->kind), "Ticket office");
        snprintf(out->detail, sizeof(out->detail), "TransitLink office");
        return;
    }
    snprintf(out->kind, sizeof(out->kind), "%s", type_name ? type_name : "Other");
    if(ud_len) station_or_code(ud, out->detail, sizeof(out->detail));
}
