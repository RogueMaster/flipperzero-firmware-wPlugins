#pragma once
/*
 * Application-layer (EN 13757-3) DIF/VIF parser.
 *
 * Only a *practical* subset is implemented — enough to read consumption
 * values from typical heat/water/gas/electricity meters. The parser walks
 * the records and emits a callback per record so the caller can render or
 * store them. Manufacturer-specific records (DIF=0x0F / 0x1F) are surfaced
 * as opaque blobs for the driver registry to interpret.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    WmbusValueNone = 0,
    WmbusValueInt,      /* signed integer (.i) */
    WmbusValueReal,     /* double (.r), already scaled */
    WmbusValueBcd,      /* BCD digits as integer (.i) */
    WmbusValueString,   /* ASCII string (.s, .s_len) */
    WmbusValueDate,     /* date(time), formatted in .s */
    WmbusValueRaw,      /* unknown DIF — raw bytes */
} WmbusValueKind;

typedef struct {
    /* Decoded fields */
    WmbusValueKind kind;
    int64_t        i;
    double         r;
    char           s[24];
    size_t         s_len;

    /* Description */
    const char* quantity;   /* "Volume", "Energy", ... */
    const char* unit;       /* "m^3", "kWh", "GJ", ... */
    int8_t      exponent;   /* 10^exponent applied to .i to obtain physical */

    /* Storage / tariff / sub-unit */
    uint8_t storage_no;
    uint8_t tariff;
    uint8_t subunit;

    /* Raw header bytes (DIF, DIFEs, VIF, VIFEs) for advanced consumers. */
    uint8_t  hdr[12];
    uint8_t  hdr_len;
    /* Raw value bytes (for the WmbusValueRaw fallback). */
    const uint8_t* raw;
    uint8_t  raw_len;
} WmbusRecord;

typedef void (*WmbusRecordCb)(const WmbusRecord* r, void* ctx);

/* Walk a decrypted APDU and emit records. Returns the number of records
 * successfully decoded (errors stop parsing but earlier records are kept). */
size_t wmbus_app_parse(
    const uint8_t* apdu,
    size_t len,
    WmbusRecordCb cb,
    void* ctx);

/* Convenience: render an entire APDU into a multi-line string buffer. */
size_t wmbus_app_render(
    const uint8_t* apdu, size_t len,
    char* out, size_t out_size);
