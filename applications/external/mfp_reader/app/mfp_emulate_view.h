#pragma once

#include "mfp_poller.h"

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct MfpEmulateView MfpEmulateView;

typedef enum {
    MfpEmulateViewStateRunning = 0,
    MfpEmulateViewStateSummary,
} MfpEmulateViewState;

/* Per-sector activity bitfield — combined with OR as events arrive. */
#define MFP_SECTOR_LOADED  0x01  /* keys present, sector can respond */
#define MFP_SECTOR_AUTHED  0x02  /* reader authenticated at least once */
#define MFP_SECTOR_READ    0x04  /* at least one block was read */
#define MFP_SECTOR_WRITTEN 0x08  /* at least one block was written */

typedef struct {
    MfpEmulateViewState state;
    uint32_t auths;
    uint32_t reads;
    uint32_t writes;
    uint8_t  last_op;       /* 'A' | 'R' | 'W' | 0 */
    uint8_t  last_sector;
    uint8_t  last_block;
    bool     allow_overwrite;
    MfpCardSize card_size;
    uint8_t  uid[7];
    uint8_t  uid_len;
    uint8_t  total_sectors;
    uint8_t  sector_flags[MFP_SECTORS_4K];
    bool     modified_saved;   /* summary: did writes persist? */
    char     summary_path[32]; /* summary: short filename */
} MfpEmulateViewModel;

MfpEmulateView* mfp_emulate_view_alloc(void);
void mfp_emulate_view_free(MfpEmulateView* view);
View* mfp_emulate_view_get_view(MfpEmulateView* view);

/* Reset model to initial Running state for a new emulation session. */
void mfp_emulate_view_reset(
    MfpEmulateView* view,
    MfpCardSize card_size,
    uint8_t total_sectors,
    const uint8_t* uid,
    uint8_t uid_len,
    const bool* sectors_loaded,
    bool allow_overwrite);

/* Called from scene's activity callback on every reader interaction. */
void mfp_emulate_view_record(
    MfpEmulateView* view,
    uint32_t auths,
    uint32_t reads,
    uint32_t writes,
    uint8_t last_op,
    uint8_t last_sector,
    uint8_t last_block);

/* Switch to summary state with final counts + save info. */
void mfp_emulate_view_show_summary(
    MfpEmulateView* view,
    bool modified_saved,
    const char* summary_path);
