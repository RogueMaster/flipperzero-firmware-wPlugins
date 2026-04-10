#pragma once

#include "mfp_poller.h"
#include "mfp_app.h"

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct MfpDumpView MfpDumpView;

typedef enum {
    MfpDumpViewStateScanning = 0,
    MfpDumpViewStateComplete,
    MfpDumpViewStateCardLost,
    MfpDumpViewStateNoKeys,
} MfpDumpViewState;

/* Per-sector visual state, derived from MfpSectorResult but with
 * an extra "currently trying" flag used only during the scan. */
#define MFP_DUMP_SECTOR_PENDING  0
#define MFP_DUMP_SECTOR_TRYING   1
#define MFP_DUMP_SECTOR_OK       2
#define MFP_DUMP_SECTOR_FAILED   3

typedef struct {
    MfpDumpViewState state;
    uint8_t  total_sectors;
    uint8_t  sectors_done;
    uint8_t  sectors_ok;
    uint8_t  current_sector;
    uint8_t  sector_states[MFP_SECTORS_4K];
    uint32_t default_keys;
    uint32_t dict_keys;
    MfpCardSize card_size;
    uint8_t  uid[7];
    uint8_t  uid_len;
} MfpDumpViewModel;

MfpDumpView* mfp_dump_view_alloc(void);
void mfp_dump_view_free(MfpDumpView* view);
View* mfp_dump_view_get_view(MfpDumpView* view);

/* Seed the view at the start of a scan. */
void mfp_dump_view_reset(
    MfpDumpView* view,
    MfpCardSize card_size,
    uint8_t total_sectors,
    const uint8_t* uid,
    uint8_t uid_len,
    uint32_t default_keys,
    uint32_t dict_keys);

/* Apply the app's current scan_* state + sector_results into the
 * view model. Called from the scene's custom-event handler whenever
 * the NFC thread reports progress. */
void mfp_dump_view_sync(MfpDumpView* view, const MfpApp* app);

/* Flip the view to one of the final states. */
void mfp_dump_view_set_state(MfpDumpView* view, MfpDumpViewState state);
