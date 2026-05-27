#include "wmbus_app_layer.h"
#include <stdio.h>
#include <string.h>

/* DIF data length lookup (EN 13757-3 §6.4). */
static int dif_data_len(uint8_t dif_field) {
    switch(dif_field & 0x0F) {
    case 0x0: return 0;          /* no data */
    case 0x1: return 1;
    case 0x2: return 2;
    case 0x3: return 3;
    case 0x4: return 4;
    case 0x5: return 4;          /* 32-bit real */
    case 0x6: return 6;
    case 0x7: return 8;
    case 0x8: return 0;          /* selection for readout */
    case 0x9: return 1;          /* 2-digit BCD */
    case 0xA: return 2;          /* 4-digit BCD */
    case 0xB: return 3;          /* 6-digit BCD */
    case 0xC: return 4;          /* 8-digit BCD */
    case 0xD: return -1;         /* variable length, follows */
    case 0xE: return 6;          /* 12-digit BCD */
    case 0xF: return 0;          /* manufacturer-specific marker */
    }
    return 0;
}

static bool dif_is_bcd(uint8_t dif_field) {
    uint8_t f = dif_field & 0x0F;
    return f == 0x9 || f == 0xA || f == 0xB || f == 0xC || f == 0xE;
}

static int64_t read_int_le(const uint8_t* p, int n) {
    int64_t v = 0;
    for(int i = n - 1; i >= 0; i--) v = (v << 8) | p[i];
    if(n < 8 && (p[n - 1] & 0x80)) {
        for(int i = n; i < 8; i++) v |= ((int64_t)0xFF << (i * 8));
    }
    return v;
}

static int64_t read_bcd(const uint8_t* p, int n) {
    /* EN 13757-3 §6.4.3: sign lives in the top nibble of the MSB. */
    int64_t v = 0;
    bool neg = (p[n - 1] & 0xF0) == 0xF0;
    for(int i = n - 1; i >= 0; i--) {
        uint8_t hi = (p[i] >> 4) & 0x0F;
        uint8_t lo = p[i] & 0x0F;
        if(i == n - 1 && neg) hi = 0;
        v = v * 100 + (hi <= 9 ? hi : 0) * 10 + (lo <= 9 ? lo : 0);
    }
    return neg ? -v : v;
}

/* VIF lookup tables — three of them, mirroring EN 13757-3 Annex C and
 * cross-checked against wmbusmeters' canonical table at
 * `_refs/wmbusmeters/src/dvparser.h:30-78`:
 *
 *   k_vif      — primary VIFs (no extension prefix)
 *   k_vif_fb   — extension after primary == 0xFB / 0x7B
 *   k_vif_fd   — extension after primary == 0xFD / 0x7D
 *
 * Each entry covers a contiguous run of VIF codes; the sub-bits within
 * the run encode the decimal exponent offset relative to exp_base. The
 * `kind_hint` lets the parser route bytes through string/integer/raw
 * decoders without re-checking the VIF code (e.g. ModelVersion is
 * always rendered as ASCII even though the data field is integer). */

typedef enum {
    VifKindAuto = 0,   /* honour DIF data field (BCD/int/real/etc.)   */
    VifKindAscii,      /* render bytes as ASCII reversed (FW versions)*/
    VifKindHex,        /* render bytes as hex (error flags, raw IDs)  */
} VifKindHint;

typedef struct {
    uint8_t  mask;       /* mask applied to VIF (without ext bit 0x80) */
    uint8_t  match;      /* expected value after mask */
    int8_t   exp_base;   /* 10^x = exp_base + (vif & ~mask) */
    const char* quantity;
    const char* unit;
    VifKindHint kind;
} VifEntry;

/* Primary VIFs (no extension marker). EN 13757-3 §6.4.4 + Annex C. */
static const VifEntry k_vif[] = {
    /* Energy: 0000_0xxx → 10^(x-3) Wh */
    {0x78, 0x00, -3, "Energy",      "Wh",     VifKindAuto},
    /* Energy: 0000_1xxx → 10^(x-3) kJ */
    {0x78, 0x08, -3, "Energy",      "kJ",     VifKindAuto},
    /* Volume: 0001_0xxx → 10^(x-6) m^3 */
    {0x78, 0x10, -6, "Volume",      "m^3",    VifKindAuto},
    /* Mass:   0001_1xxx → 10^(x-3) kg */
    {0x78, 0x18, -3, "Mass",        "kg",     VifKindAuto},
    /* Power:  0010_1xxx → 10^(x-3) W */
    {0x78, 0x28, -3, "Power",       "W",      VifKindAuto},
    /* Power:  0011_0xxx → 10^(x-3) kJ/h */
    {0x78, 0x30, -3, "Power",       "kJ/h",   VifKindAuto},
    /* Volume flow */
    {0x78, 0x38, -6, "VolFlow",  "m^3/h",  VifKindAuto},
    {0x78, 0x40, -7, "VolFlow",  "m^3/min",VifKindAuto},
    {0x78, 0x48, -9, "VolFlow",  "m^3/s",  VifKindAuto},
    {0x78, 0x50, -3, "MassFlow", "kg/h",   VifKindAuto},
    /* Temperatures (4-wide runs each) */
    {0x7C, 0x58, -3, "FlowTemp", "C",      VifKindAuto},
    {0x7C, 0x5C, -3, "RetTemp",  "C",      VifKindAuto},
    {0x7C, 0x60, -3, "TempDiff", "K",      VifKindAuto},
    {0x7C, 0x64, -3, "ExtTemp",  "C",      VifKindAuto},
    /* Pressure 1 mbar .. 1000 bar */
    {0x7C, 0x68, -3, "Pressure",    "bar",    VifKindAuto},
    /* Lifetime / averaging counters (units encoded in sub-bits, but
     * the user-visible value is "raw integer ticks" for our purposes). */
    {0x7C, 0x20, 0, "OnTime",     "h",      VifKindAuto},
    {0x7C, 0x24, 0, "Operating",  "h",      VifKindAuto},
    {0x7C, 0x70, 0, "Averaging",  "s",      VifKindAuto},
    {0x7C, 0x74, 0, "Actuality",  "s",      VifKindAuto},
    /* Date type G (16-bit) and DateTime type F (32-bit) */
    {0xFF, 0x6C, 0, "Date",         "",       VifKindAuto},
    {0xFF, 0x6D, 0, "DateTime",     "",       VifKindAuto},
    /* Heat-cost allocator (dimensionless), fabrication & enhanced IDs,
     * bus address. */
    {0xFF, 0x6E, 0, "HCA",          "u",      VifKindAuto},
    {0xFF, 0x78, 0, "FabNo",      "",       VifKindAuto},
    {0xFF, 0x79, 0, "EnhancedID", "",       VifKindAuto},
    {0xFF, 0x7A, 0, "BusAddr",    "",       VifKindAuto},
};

/* Extension VIF table after primary 0xFB. EN 13757-3 Table 14a. */
static const VifEntry k_vif_fb[] = {
    /* Energy MWh: 0000_000n → 10^(n-1) MWh, run of 2 codes */
    {0x7E, 0x00, -1, "Energy",      "MWh",   VifKindAuto},
    /* Energy GJ:  0000_100n → 10^(n-1) GJ */
    {0x7E, 0x08, -1, "Energy",      "GJ",    VifKindAuto},
    /* Volume m^3 nominal: 0001_000n → 10^(n-1) m^3 (after the
     * standard run of 8 already covered by the primary VIF) */
    {0x7E, 0x10, -1, "Volume",      "m^3",   VifKindAuto},
    /* Mass tonne: 0001_100n → 10^(n-1) t */
    {0x7E, 0x18, -1, "Mass",        "t",     VifKindAuto},
    /* Volume cubic feet: 0010_000n */
    {0x7E, 0x20,  0, "Volume",      "ft^3",  VifKindAuto},
    /* Volume gallon US: 0010_001n */
    {0x7E, 0x22,  0, "Volume",      "USgal", VifKindAuto},
    /* Volume flow gallon/{minute,hour}: 0010_010n / 0010_011n */
    {0x7E, 0x24,  0, "VolFlow",  "gal/h", VifKindAuto},
    {0x7E, 0x26,  0, "VolFlow",  "gal/m", VifKindAuto},
    /* Power MW: 0010_100n → 10^(n-1) MW */
    {0x7E, 0x28, -1, "Power",       "MW",    VifKindAuto},
    /* Power GJ/h: 0011_000n → 10^(n-1) GJ/h */
    {0x7E, 0x30, -1, "Power",       "GJ/h",  VifKindAuto},
    /* Flow temperature in F: 0101_10nn */
    {0x7C, 0x58, -3, "FlowTemp", "F",     VifKindAuto},
    {0x7C, 0x5C, -3, "RetTemp",  "F",     VifKindAuto},
    {0x7C, 0x60, -3, "TempDiff", "F",     VifKindAuto},
    {0x7C, 0x64, -3, "ExtTemp",  "F",     VifKindAuto},
    /* Cold/Warm-water-side flow temp (Kamstrup uses these). */
    {0x7C, 0x68, -3, "ColdTemp", "C",     VifKindAuto},
    /* Relative humidity 0001_101n → 10^(n-1) %RH */
    {0x7E, 0x1A, -1, "Humidity",    "%RH",   VifKindAuto},
    /* Volume @ 0°C and other reference conditions (compressed-gas) */
    {0x7E, 0x6E,  0, "VolRef",   "m^3",   VifKindAuto},
    /* Phase angle, frequency — used by smart electricity meters. */
    {0x7E, 0x70, -1, "Phase",       "deg",   VifKindAuto},
    {0x7E, 0x72, -3, "Frequency",   "Hz",    VifKindAuto},
};

/* Extension VIF table after primary 0xFD. EN 13757-3 Table 14b/c.
 * This is the "parameter & administrative" extension — most of the
 * non-numeric metadata (model name, firmware, error flags, …) lives
 * here. */
static const VifEntry k_vif_fd[] = {
    /* Credit / debit (8-digit BCD) */
    {0x7C, 0x00, -3, "Credit",      "EUR",   VifKindAuto},
    {0x7C, 0x04, -3, "Debit",       "EUR",   VifKindAuto},
    /* Identification / metadata (single-byte VIF) */
    {0x7F, 0x08,  0, "AccessNo",    "",      VifKindAuto},
    {0x7F, 0x09,  0, "Medium",      "",      VifKindAuto},
    {0x7F, 0x0A,  0, "Manuf",       "",      VifKindAuto},
    {0x7F, 0x0B,  0, "ParamSet",    "",      VifKindHex},
    {0x7F, 0x0C,  0, "Model",       "",      VifKindAscii},
    {0x7F, 0x0D,  0, "HWVer",       "",      VifKindAscii},
    {0x7F, 0x0E,  0, "FWVer",       "",      VifKindAscii},
    {0x7F, 0x0F,  0, "SWVer",       "",      VifKindAscii},
    {0x7F, 0x10,  0, "Location",    "",      VifKindAuto},
    {0x7F, 0x11,  0, "Customer",    "",      VifKindAuto},
    {0x7F, 0x12,  0, "AccessLvl",   "",      VifKindAuto},
    {0x7F, 0x13,  0, "PassWord",    "",      VifKindAuto},
    {0x7F, 0x16,  0, "Password",    "",      VifKindAuto},
    {0x7F, 0x17,  0, "ErrFlags",    "",      VifKindHex},
    {0x7F, 0x18,  0, "ErrMask",     "",      VifKindHex},
    {0x7F, 0x1A,  0, "DigitalOut",  "",      VifKindHex},
    {0x7F, 0x1B,  0, "DigitalIn",   "",      VifKindHex},
    {0x7F, 0x1C,  0, "BaudRate",    "Bd",    VifKindAuto},
    /* Duration since last readout (4 codes, sub-bits encode unit) */
    {0x7C, 0x2C,  0, "SinceRead",   "tick",  VifKindAuto},
    /* Duration of tariff (3 codes) */
    {0x7C, 0x30,  0, "TariffDur",   "tick",  VifKindAuto},
    /* Dimensionless / counter */
    {0x7F, 0x3A,  0, "Counter",     "",      VifKindAuto},
    /* Voltage  0100_xxxx → 10^(x-9)  V  (16 codes) */
    {0x70, 0x40, -9, "Voltage",     "V",     VifKindAuto},
    /* Amperage 0101_xxxx → 10^(x-12) A  (16 codes) */
    {0x70, 0x50,-12, "Current",     "A",     VifKindAuto},
    /* Reset / cumulation counters */
    {0x7F, 0x60,  0, "Resets",      "",      VifKindAuto},
    {0x7F, 0x61,  0, "Cumul",       "",      VifKindAuto},
    /* Special supplier info (manufacturer-specific blob) */
    {0x7F, 0x67,  0, "SupplierInfo","",      VifKindHex},
    /* Reactive / apparent energy & power (smart-meter trio) */
    {0x7C, 0x68, -1, "Q-energy",    "kvarh", VifKindAuto},
    {0x7C, 0x6C, -1, "S-energy",    "kVAh",  VifKindAuto},
    /* Remaining battery (days) */
    {0x7F, 0x74,  0, "BatteryDays", "d",     VifKindAuto},
    /* Phase difference / frequency for grid meters */
    {0x7C, 0x70, -1, "Phase",       "deg",   VifKindAuto},
    {0x7C, 0x74, -3, "Frequency",   "Hz",    VifKindAuto},
};

static const VifEntry* vif_table_lookup(const VifEntry* tbl, size_t n,
                                        uint8_t vif) {
    uint8_t v = vif & 0x7F;
    for(size_t i = 0; i < n; i++) {
        if((v & tbl[i].mask) == tbl[i].match) return &tbl[i];
    }
    return NULL;
}

static const VifEntry* vif_lookup(uint8_t vif) {
    return vif_table_lookup(k_vif, sizeof(k_vif)/sizeof(k_vif[0]), vif);
}
static const VifEntry* vif_lookup_fb(uint8_t vife) {
    return vif_table_lookup(k_vif_fb, sizeof(k_vif_fb)/sizeof(k_vif_fb[0]),
                            vife);
}
static const VifEntry* vif_lookup_fd(uint8_t vife) {
    return vif_table_lookup(k_vif_fd, sizeof(k_vif_fd)/sizeof(k_vif_fd[0]),
                            vife);
}

/* DIF/VIF walker. */

size_t wmbus_app_parse(const uint8_t* a, size_t len, WmbusRecordCb cb, void* ctx) {
    size_t i = 0;
    size_t produced = 0;
    while(i < len) {
        WmbusRecord r;
        memset(&r, 0, sizeof(r));

        /* Skip idle fillers */
        if(a[i] == 0x2F) { i++; continue; }
        if(a[i] == 0x0F || a[i] == 0x1F) {
            /* Manufacturer-specific data follows until end. */
            r.kind = WmbusValueRaw;
            r.quantity = "Manufacturer";
            r.unit = "";
            r.raw = &a[i + 1];
            r.raw_len = (uint8_t)((len - i - 1) > 255 ? 255 : (len - i - 1));
            r.hdr[0] = a[i]; r.hdr_len = 1;
            if(cb) cb(&r, ctx);
            produced++;
            break;
        }

        /* DIF + DIFE chain */
        uint8_t dif = a[i++];
        r.hdr[r.hdr_len++] = dif;
        r.storage_no = (dif >> 6) & 0x01;            /* LSB of storage */
        r.tariff = 0;
        r.subunit = 0;
        uint8_t func_field = (dif >> 4) & 0x03;
        (void)func_field;
        while(i < len && (a[i - 1] & 0x80)) {
            uint8_t dife = a[i++];
            if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = dife;
            r.storage_no |= (uint8_t)((dife & 0x0F) << (1 + 4 * (r.hdr_len - 2)));
            r.tariff   |= (uint8_t)(((dife >> 4) & 0x03) << (2 * (r.hdr_len - 2)));
            r.subunit  |= (uint8_t)(((dife >> 6) & 0x01) << (r.hdr_len - 2));
        }

        /* VIF + VIFE chain.
         *
         * EN 13757-3 / OMS encodes the VIF in up to two bytes:
         *   - primary VIF in [vif & 0x7F]
         *   - if primary == 0xFB or 0xFD: the *next* VIFE byte holds
         *     the actual VIF code in an extension table
         *   - if primary == 0x7C / 0xFC: the next byte is a length
         *     and that many ASCII bytes follow before any further VIFE
         *
         * Additional VIFE bytes after the "real" VIF carry semantic
         * modifiers (per-storage, per-tariff, error states, …). We
         * walk past them but only consume the high-bit-extension
         * convention; their flag meanings are not yet rendered.
         *
         * The two extension markers are matched by their LOW bits
         * (0x7B = 0xFB & 0x7F, 0x7D = 0xFD & 0x7F) so the high bit
         * (continuation) is irrelevant. */
        if(i >= len) break;
        uint8_t vif = a[i++];
        uint8_t vif_low = vif & 0x7F;
        VifKindHint kind_hint = VifKindAuto;
        if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = vif;

        const VifEntry* ve = NULL;
        bool plain_text_vif = false;
        char plain_text_buf[16];
        size_t plain_text_len = 0;

        if(vif_low == 0x7B || vif_low == 0x7D) {
            /* Extension VIF — must be followed by another VIFE byte. */
            if(i >= len) break;
            uint8_t vife = a[i++];
            if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = vife;
            ve = (vif_low == 0x7B) ? vif_lookup_fb(vife) : vif_lookup_fd(vife);
            if(ve) {
                r.quantity = ve->quantity;
                r.unit     = ve->unit;
                r.exponent = (int8_t)(ve->exp_base + (vife & ~ve->mask & 0x7F));
                kind_hint  = ve->kind;
            }
            /* Skip further VIFE flag bytes; their semantics are not
             * yet rendered but they must be consumed. */
            uint8_t prev = vife;
            while((prev & 0x80) && i < len) {
                prev = a[i++];
                if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = prev;
            }
        } else if(vif_low == 0x7C) {
            /* Plain-text VIF: next byte is the length (LVAR), followed
             * by ASCII characters in reverse byte order, optionally
             * followed by VIFE flag bytes. The text is the *quantity*
             * label; the actual data field comes after. */
            if(i >= len) break;
            uint8_t tlen = a[i++];
            if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = tlen;
            if(tlen > sizeof(plain_text_buf) - 1) tlen = sizeof(plain_text_buf) - 1;
            if(i + tlen > len) break;
            for(uint8_t k = 0; k < tlen; k++) {
                plain_text_buf[k] = (char)a[i + tlen - 1 - k];
                if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = a[i + k];
            }
            plain_text_buf[tlen] = 0;
            plain_text_len = tlen;
            i += tlen;
            plain_text_vif = true;
            r.quantity = plain_text_buf;
            r.unit     = "";
            r.exponent = 0;
            /* Skip trailing VIFE flag bytes if the original VIF had
             * the high bit set. */
            if(vif & 0x80) {
                uint8_t prev = vif;
                while((prev & 0x80) && i < len) {
                    prev = a[i++];
                    if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = prev;
                }
            }
        } else {
            /* Standard primary VIF + optional VIFE chain. */
            ve = vif_lookup(vif);
            if(ve) {
                r.quantity = ve->quantity;
                r.unit     = ve->unit;
                r.exponent = (int8_t)(ve->exp_base + (vif & ~ve->mask & 0x7F));
                kind_hint  = ve->kind;
            }
            uint8_t prev = vif;
            while((prev & 0x80) && i < len) {
                prev = a[i++];
                if(r.hdr_len < sizeof(r.hdr)) r.hdr[r.hdr_len++] = prev;
            }
        }
        if(!ve && !plain_text_vif) {
            r.quantity = "Unknown";
            r.unit     = "";
            r.exponent = 0;
        }
        (void)plain_text_len;

        /* Length & data */
        int dlen = dif_data_len(dif);
        if(dlen < 0) {
            if(i >= len) break;
            dlen = a[i++];
            if(dlen >= 0xC0) dlen -= 0xC0; /* string indicators */
        }
        if(dlen == 0 || (size_t)dlen > len - i) {
            r.kind = WmbusValueNone;
            r.raw = &a[i]; r.raw_len = 0;
            if(cb) cb(&r, ctx);
            produced++;
            break;
        }

        const uint8_t* d = &a[i]; i += dlen;
        r.raw = d; r.raw_len = (uint8_t)dlen;

        /* VIF-driven overrides for non-numeric metadata. The hint
         * forces the data to be rendered as ASCII (firmware/model
         * strings) or hex (error flags / supplier blobs) regardless
         * of the DIF's numeric data field. */
        if(kind_hint == VifKindAscii && dlen > 0) {
            r.kind = WmbusValueString;
            int max_n = (int)sizeof(r.s) - 1;
            int n = (dlen > max_n) ? max_n : dlen;
            int wp = 0;
            for(int k = n - 1; k >= 0; k--) {
                uint8_t ch = d[k];
                r.s[wp++] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '.';
            }
            r.s[wp] = 0; r.s_len = (size_t)wp;
        } else if(kind_hint == VifKindHex && dlen > 0) {
            r.kind = WmbusValueString;
            int wp = 0;
            int max_n = (int)sizeof(r.s) - 1;
            for(int k = dlen - 1; k >= 0 && wp + 2 <= max_n; k--) {
                wp += snprintf(r.s + wp, sizeof(r.s) - (size_t)wp,
                               "%02X", d[k]);
            }
            r.s[wp] = 0; r.s_len = (size_t)wp;
        } else if(dif_is_bcd(dif)) {
            r.kind = WmbusValueBcd;
            r.i = read_bcd(d, dlen);
        } else if((vif & 0x7F) == 0x6C) {
            /* 16-bit "type G" date */
            if(dlen >= 2) {
                uint16_t w = (uint16_t)(d[0] | (d[1] << 8));
                int day = w & 0x1F;
                int mon = (w >> 8) & 0x0F;
                int yr  = ((w >> 5) & 0x07) | (((w >> 12) & 0x0F) << 3);
                snprintf(r.s, sizeof(r.s), "%04d-%02d-%02d", 2000 + yr, mon, day);
                r.kind = WmbusValueDate; r.s_len = strlen(r.s);
            }
        } else if((vif & 0x7F) == 0x6D) {
            /* 32-bit "type F" datetime */
            if(dlen >= 4) {
                int min = d[0] & 0x3F;
                int hr  = d[1] & 0x1F;
                int day = d[2] & 0x1F;
                int mon = d[3] & 0x0F;
                int yr  = ((d[2] >> 5) & 0x07) | (((d[3] >> 4) & 0x0F) << 3);
                snprintf(r.s, sizeof(r.s), "%04d-%02d-%02d %02d:%02d",
                         2000 + yr, mon, day, hr, min);
                r.kind = WmbusValueDate; r.s_len = strlen(r.s);
            }
        } else if((dif & 0x0F) == 0x5) {
            /* 32-bit IEEE 754 float */
            union { uint32_t u; float f; } u = { 0 };
            u.u = (uint32_t)(d[0] | (d[1] << 8) | (d[2] << 16) | ((uint32_t)d[3] << 24));
            r.kind = WmbusValueReal;
            r.r = (double)u.f;
        } else if((dif & 0x0F) == 0xD) {
            /* Variable-length string */
            r.kind = WmbusValueString;
            int max_n = (int)sizeof(r.s) - 1;
            uint8_t n = (uint8_t)((dlen > max_n) ? max_n : dlen);
            for(int k = 0; k < n; k++) r.s[k] = d[n - 1 - k]; /* LSB-first reversed */
            r.s[n] = 0; r.s_len = n;
        } else {
            r.kind = WmbusValueInt;
            r.i = read_int_le(d, dlen);
        }

        if(cb) cb(&r, ctx);
        produced++;
    }
    return produced;
}

/* Renderer: short labels + real units, no scientific notation, drops
 * uninformative records (Unknown, zero fillers, empty dates). */
typedef struct { char* out; size_t cap; size_t pos; uint8_t emitted; } RenderCtx;

#define RENDER_MAX_LINES 10

static void format_scaled_int(int64_t i, int e, const char* unit,
                              char* o, size_t cap) {
    /* Apply 10^e scale via integer math; never use float. */
    int64_t scaled = i;
    int frac_digits = 0;
    if(e >= 0) {
        while(e > 0 && scaled <  (int64_t)1000000000000LL) { scaled *= 10; e--; }
    } else {
        frac_digits = -e;
        if(frac_digits > 6) frac_digits = 6;
    }
    if(frac_digits == 0) {
        snprintf(o, cap, "%lld %s", (long long)scaled, unit ? unit : "");
        return;
    }
    int64_t divisor = 1;
    for(int k = 0; k < frac_digits; k++) divisor *= 10;
    int64_t whole = scaled / divisor;
    int64_t frac  = scaled % divisor;
    if(frac < 0) frac = -frac;
    /* Trim trailing zeros, but never below 1 fraction digit. */
    while(frac_digits > 1 && (frac % 10) == 0) { frac /= 10; frac_digits--; }
    if(frac == 0)
        snprintf(o, cap, "%lld %s", (long long)whole, unit ? unit : "");
    else {
        /* Hand-roll the zero-padding to keep the output buffer bounded
         * (the %0*lld combo confuses -Wformat-truncation). */
        char fbuf[8];
        int  fi = 0;
        int64_t f = frac;
        for(int k = 0; k < frac_digits && fi < (int)sizeof(fbuf) - 1; k++) {
            fbuf[fi++] = (char)('0' + (int)((f / divisor / 10) % 10));
            (void)0; /* unused branch */
            divisor /= 10;
        }
        /* Simpler: just snprintf with explicit padding string. */
        char zeros[8] = "000000";
        int leading = frac_digits;
        int64_t tmp = frac;
        while(tmp > 0) { tmp /= 10; leading--; }
        if(leading < 0) leading = 0;
        if(leading > 6) leading = 6;
        zeros[leading] = 0;
        snprintf(o, cap, "%lld.%s%lld %s",
                 (long long)whole, zeros, (long long)frac, unit ? unit : "");
        (void)fbuf; (void)fi;
    }
}

static bool record_is_useful(const WmbusRecord* r) {
    if(!r->quantity) return false;
    if(strcmp(r->quantity, "Unknown") == 0) return false;
    if(strcmp(r->quantity, "Manufacturer") == 0) return false;
    /* Skip integer/BCD records that are exactly zero — usually filler.
     * Exception: certain *parameter* records (model/firmware/error/
     * counters) are emitted as strings, which keeps them visible
     * regardless of value. Voltage / Current zero readings are kept
     * too because they are meaningful on smart-meter disconnect. */
    if((r->kind == WmbusValueInt || r->kind == WmbusValueBcd) && r->i == 0) {
        if(strcmp(r->quantity, "Voltage")  == 0) return true;
        if(strcmp(r->quantity, "Current")  == 0) return true;
        if(strcmp(r->quantity, "Power")    == 0) return true;
        if(strcmp(r->quantity, "Frequency")== 0) return true;
        return false;
    }
    /* Skip empty date strings. */
    if(r->kind == WmbusValueDate && (!r->s_len || r->s[0] == 0)) return false;
    /* Skip empty strings (firmware version etc.). */
    if(r->kind == WmbusValueString && (!r->s_len || r->s[0] == 0)) return false;
    return true;
}

static void render_cb(const WmbusRecord* r, void* c) {
    RenderCtx* x = (RenderCtx*)c;
    if(x->pos >= x->cap || x->emitted >= RENDER_MAX_LINES) return;
    if(!record_is_useful(r)) return;

    char val[48];
    switch(r->kind) {
    case WmbusValueInt:
    case WmbusValueBcd:
        format_scaled_int(r->i, r->exponent, r->unit, val, sizeof(val));
        break;
    case WmbusValueReal: {
        /* IEEE float -> scaled integer (×1000) so we never need to print
         * a double. The toolchain treats fp constants as float, so we do
         * the multiplication via printf which accepts double natively. */
        snprintf(val, sizeof(val), "%.3f %s", r->r, r->unit ? r->unit : "");
        break;
    }
    case WmbusValueString:
    case WmbusValueDate:
        snprintf(val, sizeof(val), "%s", r->s); break;
    case WmbusValueRaw: {
        size_t k = r->raw_len < 6 ? r->raw_len : 6;
        char* p = val; *p = 0;
        for(size_t i = 0; i < k; i++)
            p += snprintf(p, val + sizeof(val) - p, "%02X", r->raw[i]);
        if(r->raw_len > k) snprintf(p, val + sizeof(val) - p, "..");
        break;
    }
    default:
        return; /* WmbusValueNone — skip silently */
    }

    int n = snprintf(x->out + x->pos, x->cap - x->pos, "%s: %s\n",
                     r->quantity, val);
    if(n > 0) {
        x->pos += (size_t)n;
        x->emitted++;
    }
}

size_t wmbus_app_render(const uint8_t* a, size_t len, char* out, size_t out_size) {
    RenderCtx c = { out, out_size, 0, 0 };
    if(out_size) out[0] = 0;
    wmbus_app_parse(a, len, render_cb, &c);
    return c.pos;
}
