#include <datetime.h>
#include <stdbool.h>

#ifndef GTT_I_H
#define GTT_I_H

// BIP Piemonte (GTT, Turin). Layout reverse engineered from a single card:
// only the timestamps are confirmed, type/code are shown as raw values.

#define GTT_CONTRACT_COUNT 4
#define GTT_EVENT_COUNT    3

// Timestamps are minutes since 2005-01-01 00:00 in standard time (CET, no DST)
#define GTT_EPOCH 1104537600

typedef struct {
    bool present;
    uint8_t type; // byte 0
    uint16_t code; // bytes 4-5
    uint32_t id; // bytes 6-8, referenced by events
    DateTime date; // bytes 16-18
} GttCardContract;

typedef struct {
    bool present;
    uint32_t contract_id; // bytes 2-4
    uint8_t code; // byte 11
    DateTime date; // bytes 6-8
} GttCardEvent;

typedef struct {
    bool personalised;
    GttCardContract contracts[GTT_CONTRACT_COUNT];
    GttCardEvent events[GTT_EVENT_COUNT];
} GttCardData;

#endif // GTT_I_H
