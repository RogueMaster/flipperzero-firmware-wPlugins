#pragma once

/*
 * CEPAS (Singapore EZ-Link / NETS FlashPay) data parsing.
 * Pure C, no Flipper dependencies, so it can be unit tested on a PC.
 *
 * Layouts follow Metrodroid's CEPAS implementation:
 *   Read Purse:   90 32 <purse> 00 00
 *   Read History: 90 32 <purse> 00 01 <start record> 00   (16-byte records)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CEPAS_PURSE_ID      0x03
#define CEPAS_MAX_TXNS      30
#define CEPAS_TXN_SIZE      16
#define CEPAS_PURSE_MIN_LEN 46 /* everything up to the last-transaction record */

/* 1995-01-01 00:00 expressed as a UTC Unix time. CEPAS times count from
 * 1995-01-01 00:00 Singapore time, so adding this gives Singapore wall-clock
 * time that can be fed to a UTC date formatter directly. */
#define CEPAS_EPOCH_WALLCLOCK 788918400UL

typedef enum {
    CepasIssuerUnknown,
    CepasIssuerEzLink,
    CepasIssuerNets,
} CepasIssuer;

typedef struct {
    uint8_t type;
    int32_t amount; /* cents; negative = money taken off the card */
    uint32_t time; /* seconds since the CEPAS epoch */
    char user_data[9]; /* ASCII location / service code */
} CepasTxn;

typedef struct {
    uint8_t version;
    uint8_t status;
    int32_t balance; /* cents */
    int32_t autoload; /* cents */
    uint8_t can[8];
    uint8_t csn[8];
    uint16_t expiry_days;
    uint16_t creation_days;
    uint8_t log_count;
    bool has_last_txn;
    CepasTxn last_txn;
} CepasPurse;

typedef struct {
    CepasPurse purse;
    CepasTxn txns[CEPAS_MAX_TXNS];
    size_t txn_count;
    CepasIssuer issuer;
    bool has_purse;
} CepasCard;

bool cepas_parse_purse(const uint8_t* data, size_t len, CepasPurse* out);
void cepas_parse_txn(const uint8_t* data, CepasTxn* out);
size_t cepas_parse_history(const uint8_t* data, size_t len, CepasTxn* out, size_t max);

CepasIssuer cepas_issuer(const CepasPurse* purse);
const char* cepas_issuer_name(CepasIssuer issuer);

/* Returns NULL for unknown types. */
const char* cepas_txn_type_name(uint8_t type);

/* "1009 1234 5678 9012" (needs 20 bytes). */
void cepas_format_can(const uint8_t can[8], char* out, size_t out_size);

/* "$12.34", "-$1.20", or "+$10.00" when show_plus is set. */
void cepas_format_amount(int32_t cents, bool show_plus, char* out, size_t out_size);

/* Station name for a 3-character code ("PGL" -> "Punggol"), or NULL. */
const char* cepas_station_name(const char* code);

/* Human-readable text for one transaction. */
typedef struct {
    char kind[20]; /* "MRT", "Bus", "Top-up", "Ticket office", ... */
    char detail[64]; /* "City Hall > Punggol", "Service 118", ... */
    char from[24]; /* MRT/LRT only, else empty */
    char to[24]; /* MRT/LRT only, else empty */
} CepasTxnText;

void cepas_txn_describe(const CepasTxn* txn, CepasTxnText* out);

/* Singapore wall-clock time as a UTC-style Unix timestamp. */
uint32_t cepas_time_to_wallclock(uint32_t seconds);
uint32_t cepas_days_to_wallclock(uint16_t days);
