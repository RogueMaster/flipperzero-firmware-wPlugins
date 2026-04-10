#pragma once

#include "mfp_poller.h"

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct MfpResultView MfpResultView;

/* Per-sector key state packed into a single byte. */
#define MFP_RESULT_SECTOR_FAIL    0x00  /* no key found */
#define MFP_RESULT_SECTOR_KEY_A   0x01
#define MFP_RESULT_SECTOR_KEY_B   0x02
#define MFP_RESULT_SECTOR_KEY_AB  0x03

typedef struct {
    MfpCardSize card_size;
    MfpSecurityLevel sl;
    uint8_t  uid[7];
    uint8_t  uid_len;
    uint8_t  total_sectors;
    uint8_t  sectors_ok;
    uint8_t  sectors_a_only;
    uint8_t  sectors_b_only;
    uint8_t  sectors_ab;
    uint8_t  sector_states[MFP_SECTORS_4K];
} MfpResultViewModel;

typedef void (*MfpResultViewActionsCallback)(void* ctx);

MfpResultView* mfp_result_view_alloc(void);
void mfp_result_view_free(MfpResultView* view);
View* mfp_result_view_get_view(MfpResultView* view);

void mfp_result_view_set_actions_callback(
    MfpResultView* view, MfpResultViewActionsCallback cb, void* ctx);

void mfp_result_view_update(
    MfpResultView* view,
    MfpCardSize card_size,
    MfpSecurityLevel sl,
    const uint8_t* uid,
    uint8_t uid_len,
    uint8_t total_sectors,
    const uint8_t* sector_states);
