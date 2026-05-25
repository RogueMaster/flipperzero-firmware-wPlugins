#pragma once

#include <furi.h>
#include <flipper_format/flipper_format.h>

#define PSA_BF_STATUS_IDLE      0
#define PSA_BF_STATUS_RUNNING   1
#define PSA_BF_STATUS_FOUND     2
#define PSA_BF_STATUS_NOT_FOUND 3
#define PSA_BF_STATUS_CANCELLED 4

typedef struct PsaBfState PsaBfState;
struct PsaBfState {
    volatile uint8_t cancel;
    volatile uint32_t progress_current;
    volatile uint32_t progress_total;
    volatile uint8_t status;
    void (*on_done)(void* context);
    void* on_done_ctx;

    uint32_t key1_low;
    uint32_t key1_high;
    uint16_t key2_low;

    uint8_t decrypted_button;
    uint32_t decrypted_serial;
    uint32_t decrypted_counter;
    uint16_t decrypted_crc;
    uint32_t decrypted_seed;
    uint8_t decrypted_type;
};
